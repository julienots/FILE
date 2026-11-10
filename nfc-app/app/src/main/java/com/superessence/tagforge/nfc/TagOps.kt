package com.superessence.tagforge.nfc

import android.nfc.FormatException
import android.nfc.NdefMessage
import android.nfc.NdefRecord
import android.nfc.Tag
import android.nfc.TagLostException
import android.nfc.tech.IsoDep
import android.nfc.tech.MifareClassic
import android.nfc.tech.MifareUltralight
import android.nfc.tech.Ndef
import android.nfc.tech.NdefFormatable
import android.nfc.tech.NfcA
import android.nfc.tech.NfcB
import android.nfc.tech.NfcF
import android.nfc.tech.NfcV
import java.io.IOException

data class TagInfo(
    val uid: String,
    val techs: List<String>,
    val chip: String,
    val ndefType: String?,
    val maxSize: Int,
    val usedSize: Int,
    val writable: Boolean,
    val canLock: Boolean,
    val message: NdefMessage?,
    val records: List<ParsedRecord>,
    val details: List<Pair<String, String>>,
    val memory: List<String>,
    val time: Long = System.currentTimeMillis(),
)

class TagError(msg: String) : Exception(msg)

object TagOps {

    // ------------------------------------------------------------------ LECTURE
    fun read(tag: Tag, dumpMemory: Boolean): TagInfo {
        val techs = tag.techList.map { it.substringAfterLast('.') }
        val details = mutableListOf<Pair<String, String>>()
        var chip = guessChip(tag)
        val memory = mutableListOf<String>()

        NfcA.get(tag)?.let { a ->
            details += "ATQA" to Encoders.hex(a.atqa)
            details += "SAK" to "%02X".format(a.sak)
            details += "Norme" to "ISO 14443-A"
        }
        NfcB.get(tag)?.let { b ->
            details += "Norme" to "ISO 14443-B"
            details += "Données appli" to Encoders.hex(b.applicationData)
            details += "Infos protocole" to Encoders.hex(b.protocolInfo)
        }
        NfcF.get(tag)?.let { f ->
            details += "Norme" to "FeliCa (JIS X 6319-4)"
            details += "Code système" to Encoders.hex(f.systemCode)
            details += "Fabricant" to Encoders.hex(f.manufacturer)
        }
        NfcV.get(tag)?.let { v ->
            details += "Norme" to "ISO 15693 (NFC-V)"
            details += "DSFID" to "%02X".format(v.dsfId)
            details += "Flags" to "%02X".format(v.responseFlags)
        }
        IsoDep.get(tag)?.let { d ->
            d.historicalBytes?.let { details += "Octets historiques" to Encoders.hex(it) }
            d.hiLayerResponse?.let { details += "Réponse couche haute" to Encoders.hex(it) }
            details += "ISO-DEP" to "Oui (carte à puce / ISO 7816)"
        }
        MifareClassic.get(tag)?.let { m ->
            details += "MIFARE Classic" to "${m.size} octets · ${m.sectorCount} secteurs · ${m.blockCount} blocs"
        }

        val ndef = Ndef.get(tag)
        val msg = ndef?.cachedNdefMessage
        val ndefType = ndef?.type?.let { prettyNdefType(it) }

        if (dumpMemory) {
            runCatching { memory += dumpType2(tag, details).also { if (it.chip != null) chip = it.chip }.lines }
                .onFailure { if (MifareClassic.get(tag) == null) memory += "Lecture mémoire : ${it.message ?: it.javaClass.simpleName}" }
            if (memory.isEmpty() && MifareClassic.get(tag) != null) runCatching { memory += dumpClassic(tag) }
                .onFailure { memory += "MIFARE Classic : ${it.message}" }
        }

        return TagInfo(
            uid = Encoders.hex(tag.id, ":"),
            techs = techs,
            chip = chip,
            ndefType = ndefType,
            maxSize = ndef?.maxSize ?: 0,
            usedSize = msg?.byteArrayLength ?: 0,
            writable = ndef?.isWritable ?: (NdefFormatable.get(tag) != null),
            canLock = ndef?.canMakeReadOnly() ?: false,
            message = msg,
            records = NdefParser.parse(msg),
            details = details,
            memory = memory,
        )
    }

    private fun prettyNdefType(t: String) = when (t) {
        Ndef.NFC_FORUM_TYPE_1 -> "NFC Forum Type 1 (Topaz)"
        Ndef.NFC_FORUM_TYPE_2 -> "NFC Forum Type 2 (NTAG / Ultralight)"
        Ndef.NFC_FORUM_TYPE_3 -> "NFC Forum Type 3 (FeliCa)"
        Ndef.NFC_FORUM_TYPE_4 -> "NFC Forum Type 4 (DESFire / ISO-DEP)"
        Ndef.MIFARE_CLASSIC -> "MIFARE Classic"
        else -> t
    }

    private fun guessChip(tag: Tag): String {
        MifareUltralight.get(tag)?.let { return if (it.type == MifareUltralight.TYPE_ULTRALIGHT_C) "MIFARE Ultralight C" else "MIFARE Ultralight / NTAG" }
        MifareClassic.get(tag)?.let {
            return when (it.type) {
                MifareClassic.TYPE_PLUS -> "MIFARE Plus"; MifareClassic.TYPE_PRO -> "MIFARE Pro"
                else -> "MIFARE Classic " + if (it.size >= 4096) "4K" else if (it.size >= 1024) "1K" else "Mini"
            }
        }
        if (IsoDep.get(tag) != null) return "Carte ISO-DEP (DESFire, carte bancaire, passeport…)"
        if (NfcV.get(tag) != null) return "ISO 15693 (ICODE, ST25…)"
        if (NfcF.get(tag) != null) return "FeliCa"
        return "Tag NFC"
    }

    data class Version(val chip: String?, val pages: Int, val cfgBase: Int?)

    /** Identification NXP via GET_VERSION (0x60). */
    fun version(a: NfcA): Version {
        val v = try { a.transceive(byteArrayOf(0x60)) } catch (e: IOException) { null }
        if (v == null || v.size < 8) {
            reconnect(a)
            return Version(null, 16, null)
        }
        val type = v[2].toInt(); val size = v[6].toInt() and 0xFF
        return when {
            type == 4 && size == 0x0B -> Version("NTAG210", 20, 0x10)
            type == 4 && size == 0x0E -> Version("NTAG212", 41, 0x25)
            type == 4 && size == 0x0F -> Version("NTAG213", 45, 0x29)
            type == 4 && size == 0x11 -> Version("NTAG215", 135, 0x83)
            type == 4 && size == 0x13 -> Version("NTAG216", 231, 0xE3)
            type == 3 && size == 0x0B -> Version("MIFARE Ultralight EV1 (MF0UL11)", 20, 0x10)
            type == 3 && size == 0x0E -> Version("MIFARE Ultralight EV1 (MF0UL21)", 41, 0x25)
            type == 4 && v[3].toInt() == 5 -> Version("NTAG I²C", if (size == 0x13) 231 else 485, null)
            else -> Version("Puce NXP (type %02X, taille %02X)".format(type, size), 16 + (1 shl (size shr 1)) / 4, null)
        }
    }

    private fun reconnect(a: NfcA) {
        // après une commande refusée, la puce repasse en veille : on rétablit la liaison
        runCatching { a.close() }; runCatching { a.connect() }
    }

    class Dump(val chip: String?, val lines: List<String>)

    fun dumpType2(tag: Tag, details: MutableList<Pair<String, String>>): Dump {
        val a = NfcA.get(tag) ?: throw TagError("pas de NfcA")
        if (MifareClassic.get(tag) != null || IsoDep.get(tag) != null) throw TagError("non Type 2")
        a.use {
            a.connect()
            val ver = version(a)
            ver.chip?.let { details += "Puce identifiée" to it }
            runCatching {
                val sig = a.transceive(byteArrayOf(0x3C, 0x00))
                if (sig.size >= 32) details += "Signature NXP (ECC)" to Encoders.hex(sig.copyOf(32), "")
            }.onFailure { reconnect(a) }
            runCatching {
                val cnt = a.transceive(byteArrayOf(0x39, 0x02))
                if (cnt.size >= 3) details += "Compteur de lectures NFC" to ((cnt[0].toInt() and 0xFF) or ((cnt[1].toInt() and 0xFF) shl 8) or ((cnt[2].toInt() and 0xFF) shl 16)).toString()
            }.onFailure { reconnect(a) }
            val lines = mutableListOf<String>()
            var p = 0
            while (p < ver.pages) {
                val r = try { a.transceive(byteArrayOf(0x30, p.toByte())) } catch (e: IOException) {
                    lines += "%03d  ── protégé / fin de mémoire".format(p); break
                }
                for (k in 0 until 4) {
                    val page = p + k
                    if (page >= ver.pages || r.size < (k + 1) * 4) break
                    val b = r.copyOfRange(k * 4, k * 4 + 4)
                    val ascii = b.map { c -> val ch = (c.toInt() and 0xFF).toChar(); if (ch in ' '..'~') ch else '.' }.joinToString("")
                    lines += "%03d  %s  %s  %s".format(page, Encoders.hex(b), ascii, pageLabel(page, ver))
                }
                p += 4
            }
            if (lines.size >= 4) {
                val cc = lines.getOrNull(3)
                if (cc != null) details += "Mémoire lue" to "${lines.size} pages × 4 octets"
            }
            return Dump(ver.chip, lines)
        }
    }

    private fun pageLabel(p: Int, v: Version): String = when {
        p <= 1 -> "UID"
        p == 2 -> "UID/verrous"
        p == 3 -> "CC"
        v.cfgBase != null && p == v.cfgBase - 1 -> "verrous dyn."
        v.cfgBase != null && p == v.cfgBase -> "CFG0 (AUTH0)"
        v.cfgBase != null && p == v.cfgBase + 1 -> "CFG1 (ACCESS)"
        v.cfgBase != null && p == v.cfgBase + 2 -> "PWD"
        v.cfgBase != null && p == v.cfgBase + 3 -> "PACK"
        else -> ""
    }

    private val CLASSIC_KEYS = listOf(MifareClassic.KEY_DEFAULT, MifareClassic.KEY_MIFARE_APPLICATION_DIRECTORY, MifareClassic.KEY_NFC_FORUM)

    fun dumpClassic(tag: Tag): List<String> {
        val m = MifareClassic.get(tag) ?: return emptyList()
        val out = mutableListOf<String>()
        m.use {
            m.connect()
            for (s in 0 until m.sectorCount) {
                val ok = CLASSIC_KEYS.any { k -> runCatching { m.authenticateSectorWithKeyA(s, k) }.getOrDefault(false) }
                if (!ok) { out += "Secteur %02d : clé inconnue".format(s); continue }
                val first = m.sectorToBlock(s)
                for (b in first until first + m.getBlockCountInSector(s)) {
                    val data = runCatching { m.readBlock(b) }.getOrNull()
                    out += "S%02d B%03d  %s".format(s, b, data?.let { Encoders.hex(it) } ?: "illisible")
                }
            }
        }
        return out
    }

    // ------------------------------------------------------------------ ÉCRITURE
    /**
     * Écrit un message NDEF. [authPassword] : mot de passe si le tag est déjà protégé (NTAG21x).
     * [lockAfter] : verrouillage définitif. [newPassword] : protège le tag après l'écriture.
     */
    fun write(tag: Tag, msg: NdefMessage, authPassword: String?, lockAfter: Boolean, newPassword: String?, protectRead: Boolean): String {
        val bytes = msg.toByteArray()
        val log = StringBuilder()

        if (!authPassword.isNullOrBlank()) {
            writeType2Raw(tag, bytes, authPassword)
            log.append("Écrit avec authentification (${bytes.size} octets).")
        } else {
            val ndef = Ndef.get(tag)
            if (ndef != null) {
                ndef.use {
                    ndef.connect()
                    if (!ndef.isWritable) throw TagError("Ce tag est en lecture seule (verrouillé).")
                    if (bytes.size > ndef.maxSize) throw TagError("Contenu trop grand : ${bytes.size} octets pour ${ndef.maxSize} disponibles. Utilisez un tag plus grand (NTAG215/216) ou réduisez le contenu.")
                    try { ndef.writeNdefMessage(msg) } catch (e: FormatException) { throw TagError("Message invalide : ${e.message}") }
                    log.append("Écrit : ${bytes.size} / ${ndef.maxSize} octets.")
                    if (lockAfter && newPassword.isNullOrBlank()) {
                        if (!ndef.canMakeReadOnly()) throw TagError("Écrit, mais ce tag ne peut pas être verrouillé.")
                        ndef.makeReadOnly(); log.append(" Tag verrouillé définitivement.")
                    }
                }
            } else {
                val f = NdefFormatable.get(tag) ?: throw TagError("Ce tag n'est pas compatible NDEF (ex. carte bancaire, badge MIFARE Classic sur un téléphone sans puce NXP…).")
                f.use {
                    f.connect()
                    if (lockAfter && newPassword.isNullOrBlank()) f.formatReadOnly(msg) else f.format(msg)
                }
                log.append("Tag formaté puis écrit (${bytes.size} octets).")
                if (lockAfter && newPassword.isNullOrBlank()) log.append(" Verrouillé.")
            }
        }
        if (!newPassword.isNullOrBlank()) {
            setPassword(tag, newPassword, protectRead, authPassword)
            log.append(" Mot de passe activé" + if (protectRead) " (lecture et écriture)." else " (écriture).")
        }
        return log.toString()
    }

    /** Écriture directe Type 2 (NTAG/Ultralight) page par page, après authentification éventuelle. */
    fun writeType2Raw(tag: Tag, ndef: ByteArray, password: String?) {
        val a = NfcA.get(tag) ?: throw TagError("Écriture protégée : réservée aux tags NTAG / Ultralight.")
        a.use {
            a.connect()
            val ver = version(a)
            if (!password.isNullOrBlank()) auth(a, password)
            val tlv = Encoders.type2Tlv(ndef)
            val cc = a.transceive(byteArrayOf(0x30, 3)).copyOf(4)
            val capacity = (cc[2].toInt() and 0xFF) * 8
            if (cc[0] != 0xE1.toByte()) throw TagError("Tag non formaté NDEF (CC absent).")
            if (tlv.size > capacity) throw TagError("Contenu trop grand : ${tlv.size} octets pour $capacity.")
            if (ver.cfgBase != null && 4 + tlv.size / 4 > ver.cfgBase - 1) throw TagError("Contenu trop grand pour ${ver.chip}.")
            for (i in tlv.indices step 4) {
                val page = 4 + i / 4
                a.transceive(byteArrayOf(0xA2.toByte(), page.toByte(), tlv[i], tlv[i + 1], tlv[i + 2], tlv[i + 3]))
            }
        }
    }

    private fun auth(a: NfcA, password: String) {
        val (pwd, pack) = Encoders.ntagPassword(password)
        val resp = try { a.transceive(byteArrayOf(0x1B) + pwd) } catch (e: IOException) { throw TagError("Mot de passe refusé.") }
        if (resp.size < 2 || resp[0] != pack[0] || resp[1] != pack[1]) {
            // PACK différent : le mot de passe a peut-être été défini par une autre appli, on accepte s'il n'y a pas eu d'erreur
        }
    }

    // ------------------------------------------------------------------ MOT DE PASSE NTAG21x
    fun setPassword(tag: Tag, password: String, protectRead: Boolean, currentPassword: String? = null) {
        val a = NfcA.get(tag) ?: throw TagError("Mot de passe : réservé aux NTAG21x / Ultralight EV1.")
        val (pwd, pack) = Encoders.ntagPassword(password)
        a.use {
            if (!a.isConnected) a.connect()
            val v = version(a)
            val base = v.cfgBase ?: throw TagError("Cette puce (${v.chip ?: "inconnue"}) ne gère pas le mot de passe NTAG.")
            if (!currentPassword.isNullOrBlank()) auth(a, currentPassword)
            write4(a, base + 2, pwd)
            write4(a, base + 3, byteArrayOf(pack[0], pack[1], 0, 0))
            val cfg1 = a.transceive(byteArrayOf(0x30, (base + 1).toByte())).copyOf(4)
            cfg1[0] = if (protectRead) (cfg1[0].toInt() or 0x80).toByte() else (cfg1[0].toInt() and 0x7F).toByte()
            write4(a, base + 1, cfg1)
            val cfg0 = a.transceive(byteArrayOf(0x30, base.toByte())).copyOf(4)
            cfg0[3] = 0x04 // protection à partir de la page 4 (tout le contenu utilisateur)
            write4(a, base, cfg0)
        }
    }

    fun removePassword(tag: Tag, password: String) {
        val a = NfcA.get(tag) ?: throw TagError("Réservé aux NTAG21x.")
        a.use {
            a.connect()
            val v = version(a)
            val base = v.cfgBase ?: throw TagError("Cette puce ne gère pas le mot de passe NTAG.")
            auth(a, password)
            val cfg0 = a.transceive(byteArrayOf(0x30, base.toByte())).copyOf(4)
            cfg0[3] = 0xFF.toByte()
            write4(a, base, cfg0)
            val cfg1 = a.transceive(byteArrayOf(0x30, (base + 1).toByte())).copyOf(4)
            cfg1[0] = (cfg1[0].toInt() and 0x7F).toByte()
            write4(a, base + 1, cfg1)
        }
    }

    private fun write4(a: NfcA, page: Int, d: ByteArray) {
        a.transceive(byteArrayOf(0xA2.toByte(), page.toByte(), d[0], d[1], d[2], d[3]))
    }

    // ------------------------------------------------------------------ OUTILS
    fun erase(tag: Tag): String {
        val empty = NdefMessage(arrayOf(NdefRecord(NdefRecord.TNF_EMPTY, null, null, null)))
        return write(tag, empty, null, false, null, false).let { "Tag effacé." }
    }

    fun format(tag: Tag): String {
        val f = NdefFormatable.get(tag)
        if (f == null) {
            if (Ndef.get(tag) != null) { erase(tag); return "Tag déjà au format NDEF : contenu effacé." }
            throw TagError("Ce tag ne peut pas être formaté en NDEF par Android.")
        }
        f.use { f.connect(); f.format(NdefMessage(arrayOf(NdefRecord(NdefRecord.TNF_EMPTY, null, null, null)))) }
        return "Tag formaté (NDEF vide)."
    }

    fun lock(tag: Tag): String {
        val ndef = Ndef.get(tag) ?: throw TagError("Tag non NDEF : écrivez d'abord un contenu.")
        ndef.use {
            ndef.connect()
            if (!ndef.isWritable) return "Ce tag est déjà verrouillé."
            if (!ndef.canMakeReadOnly()) throw TagError("Ce tag ne permet pas le verrouillage.")
            if (!ndef.makeReadOnly()) throw TagError("Le verrouillage a échoué.")
        }
        return "Tag verrouillé définitivement (lecture seule)."
    }

    fun readMessage(tag: Tag): NdefMessage {
        val ndef = Ndef.get(tag) ?: throw TagError("Tag non NDEF ou vide.")
        ndef.use {
            ndef.connect()
            return ndef.ndefMessage ?: throw TagError("Le tag est vide.")
        }
    }

    fun humanError(e: Throwable): String = when (e) {
        is TagError -> e.message ?: "Erreur"
        is TagLostException -> "Tag perdu : gardez le téléphone immobile contre le tag jusqu'à la fin."
        is IOException -> "Communication interrompue (${e.message ?: "E/S"}). Réessayez sans bouger le téléphone."
        is IllegalArgumentException -> e.message ?: "Contenu invalide"
        is SecurityException -> "Tag retiré trop tôt. Réessayez."
        else -> e.message ?: e.javaClass.simpleName
    }
}
