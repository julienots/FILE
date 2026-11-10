package com.superessence.tagforge.nfc

import java.io.ByteArrayOutputStream
import java.net.URLEncoder
import java.security.MessageDigest

/** Encodeurs purs (sans dépendance Android) : testables en JVM. */
object Encoders {

    fun enc(s: String): String = URLEncoder.encode(s, "UTF-8").replace("+", "%20")

    fun digits(s: String): String = s.filter { it.isDigit() || it == '+' }

    // ---------------------------------------------------------------- Wi-Fi (Wi-Fi Simple Config, lu nativement par Android)
    const val WSC_MIME = "application/vnd.wfa.wsc"

    fun wifiPayload(ssid: String, password: String, auth: String, hidden: Boolean = false): ByteArray {
        val authType = when (auth) {
            "OPEN" -> 0x0001
            "WPA" -> 0x0002
            "WPA_WPA2" -> 0x0022
            else -> 0x0020 // WPA2 (et réseaux WPA2/WPA3 en transition)
        }
        val encType = when (auth) {
            "OPEN" -> 0x0001
            "WPA" -> 0x0004
            "WPA_WPA2" -> 0x000C
            else -> 0x0008
        }
        val cred = ByteArrayOutputStream()
        cred.tlv(0x1026, byteArrayOf(1))
        cred.tlv(0x1045, ssid.toByteArray(Charsets.UTF_8))
        cred.tlv(0x1003, short(authType))
        cred.tlv(0x100F, short(encType))
        cred.tlv(0x1027, if (auth == "OPEN") ByteArray(0) else password.toByteArray(Charsets.UTF_8))
        cred.tlv(0x1020, ByteArray(6) { 0xFF.toByte() })
        if (hidden) cred.tlv(0x1060, byteArrayOf(1)) // indicateur "réseau masqué" (extension, ignorée si non gérée)
        val out = ByteArrayOutputStream()
        out.tlv(0x104A, byteArrayOf(0x10))
        out.tlv(0x100E, cred.toByteArray())
        out.tlv(0x1049, byteArrayOf(0x00, 0x37, 0x2A, 0x00, 0x01, 0x20)) // WFA vendor ext : version 2.0
        return out.toByteArray()
    }

    data class WifiInfo(val ssid: String, val password: String, val auth: String)

    fun parseWifi(payload: ByteArray): WifiInfo? {
        var ssid: String? = null
        var key = ""
        var auth = "?"
        fun walk(b: ByteArray) {
            var i = 0
            while (i + 4 <= b.size) {
                val t = ((b[i].toInt() and 0xFF) shl 8) or (b[i + 1].toInt() and 0xFF)
                val l = ((b[i + 2].toInt() and 0xFF) shl 8) or (b[i + 3].toInt() and 0xFF)
                if (i + 4 + l > b.size) return
                val v = b.copyOfRange(i + 4, i + 4 + l)
                when (t) {
                    0x100E -> walk(v)
                    0x1045 -> ssid = String(v, Charsets.UTF_8)
                    0x1027 -> key = String(v, Charsets.UTF_8)
                    0x1003 -> auth = when (((v[0].toInt() and 0xFF) shl 8) or (v[1].toInt() and 0xFF)) {
                        0x0001 -> "Ouvert"; 0x0002 -> "WPA"; 0x0020 -> "WPA2"; 0x0022 -> "WPA/WPA2"; else -> "Autre"
                    }
                }
                i += 4 + l
            }
        }
        walk(payload)
        return ssid?.let { WifiInfo(it, key, auth) }
    }

    private fun short(v: Int) = byteArrayOf((v shr 8).toByte(), v.toByte())

    private fun ByteArrayOutputStream.tlv(type: Int, value: ByteArray) {
        write(type shr 8); write(type and 0xFF)
        write(value.size shr 8); write(value.size and 0xFF)
        write(value)
    }

    // ---------------------------------------------------------------- Bluetooth (appairage par "handover", natif Android)
    const val BT_MIME = "application/vnd.bluetooth.ep.oob"

    fun parseMac(mac: String): ByteArray? {
        val hex = mac.filter { it.isLetterOrDigit() }
        if (hex.length != 12 || !hex.all { it in "0123456789abcdefABCDEF" }) return null
        return ByteArray(6) { hex.substring(it * 2, it * 2 + 2).toInt(16).toByte() }
    }

    fun bluetoothPayload(mac: String, name: String): ByteArray {
        val m = parseMac(mac) ?: throw IllegalArgumentException("Adresse MAC invalide (format AA:BB:CC:DD:EE:FF)")
        val body = ByteArrayOutputStream()
        body.write(m.reversedArray())
        if (name.isNotBlank()) {
            val n = name.toByteArray(Charsets.UTF_8)
            body.write(n.size + 1); body.write(0x09); body.write(n)
        }
        val b = body.toByteArray()
        val total = b.size + 2
        return byteArrayOf(total.toByte(), (total shr 8).toByte()) + b
    }

    fun parseBluetooth(p: ByteArray): String {
        if (p.size < 8) return hex(p)
        val mac = p.copyOfRange(2, 8).reversedArray().joinToString(":") { "%02X".format(it) }
        var name = ""
        var i = 8
        while (i < p.size) {
            val l = p[i].toInt() and 0xFF
            if (l == 0 || i + l >= p.size) break
            val t = p[i + 1].toInt() and 0xFF
            if (t == 0x09 || t == 0x08) name = String(p, i + 2, l - 1, Charsets.UTF_8)
            i += l + 1
        }
        return if (name.isNotEmpty()) "$name ($mac)" else mac
    }

    // ---------------------------------------------------------------- vCard
    fun vcard(v: Map<String, String>): String = buildString {
        fun g(k: String) = v[k].orEmpty().trim()
        append("BEGIN:VCARD\r\nVERSION:3.0\r\n")
        append("N:${g("last")};${g("first")};;;\r\n")
        append("FN:${listOf(g("first"), g("last")).filter { it.isNotEmpty() }.joinToString(" ")}\r\n")
        if (g("org").isNotEmpty()) append("ORG:${g("org")}\r\n")
        if (g("title").isNotEmpty()) append("TITLE:${g("title")}\r\n")
        if (g("phone").isNotEmpty()) append("TEL;TYPE=CELL:${g("phone")}\r\n")
        if (g("phone2").isNotEmpty()) append("TEL;TYPE=WORK:${g("phone2")}\r\n")
        if (g("email").isNotEmpty()) append("EMAIL:${g("email")}\r\n")
        if (g("url").isNotEmpty()) append("URL:${g("url")}\r\n")
        if (g("address").isNotEmpty()) append("ADR:;;${g("address")};;;;\r\n")
        if (g("birthday").isNotEmpty()) append("BDAY:${g("birthday")}\r\n")
        if (g("note").isNotEmpty()) append("NOTE:${g("note").replace("\n", "\\n")}\r\n")
        append("END:VCARD\r\n")
    }

    // ---------------------------------------------------------------- Agenda Google
    /** "2026-12-24 19:30" -> "20261224T193000" */
    fun calDate(s: String): String {
        val d = s.filter { it.isDigit() }
        return when {
            d.length >= 12 -> d.substring(0, 8) + "T" + d.substring(8, 12) + "00"
            d.length == 8 -> d
            else -> d
        }
    }

    // ---------------------------------------------------------------- Mot de passe NTAG
    /** 8 chiffres hexa -> utilisés tels quels ; sinon dérivés du texte (SHA-256). Retourne PWD(4) + PACK(2). */
    fun ntagPassword(input: String): Pair<ByteArray, ByteArray> {
        val t = input.trim()
        if (t.length == 8 && t.all { it in "0123456789abcdefABCDEF" }) {
            val pwd = ByteArray(4) { t.substring(it * 2, it * 2 + 2).toInt(16).toByte() }
            return pwd to byteArrayOf(0x54, 0x46) // PACK "TF"
        }
        val h = MessageDigest.getInstance("SHA-256").digest(t.toByteArray(Charsets.UTF_8))
        return h.copyOfRange(0, 4) to h.copyOfRange(4, 6)
    }

    // ---------------------------------------------------------------- Divers
    fun hex(b: ByteArray?, sep: String = " "): String = b?.joinToString(sep) { "%02X".format(it) } ?: ""

    fun printable(b: ByteArray): Boolean {
        if (b.isEmpty()) return true
        val s = runCatching { String(b, Charsets.UTF_8) }.getOrNull() ?: return false
        return s.all { !it.isISOControl() || it == '\n' || it == '\r' || it == '\t' } && !s.contains('\uFFFD')
    }

    /** Type 2 : TLV NDEF + terminateur, complété à un multiple de 4 octets. */
    fun type2Tlv(ndef: ByteArray): ByteArray {
        val out = ByteArrayOutputStream()
        out.write(0x03)
        if (ndef.size < 0xFF) out.write(ndef.size) else { out.write(0xFF); out.write(ndef.size shr 8); out.write(ndef.size and 0xFF) }
        out.write(ndef)
        out.write(0xFE)
        while (out.size() % 4 != 0) out.write(0)
        return out.toByteArray()
    }

    /** Remplace {n} (compteur), {n3} (compteur sur 3 chiffres), {date} et {time}. */
    fun placeholders(s: String, n: Int, now: java.util.Date = java.util.Date()): String {
        if (!s.contains('{')) return s
        return s.replace("{n}", n.toString())
            .replace("{n2}", "%02d".format(n))
            .replace("{n3}", "%03d".format(n))
            .replace("{n4}", "%04d".format(n))
            .replace("{date}", java.text.SimpleDateFormat("yyyy-MM-dd", java.util.Locale.FRANCE).format(now))
            .replace("{time}", java.text.SimpleDateFormat("HH:mm", java.util.Locale.FRANCE).format(now))
    }

    /** Place estimée pour un message NDEF (TLV inclus) sur les puces courantes. */
    val CHIPS = listOf(
        "NTAG210" to 48, "NTAG213" to 144, "Ultralight EV1 (128)" to 128, "NTAG215" to 504,
        "NTAG216" to 888, "MIFARE Classic 1K" to 716, "NTAG I²C 2K" to 1904,
    )

    fun tlvSize(ndefLen: Int) = ndefLen + (if (ndefLen < 0xFF) 2 else 4) + 1
}
