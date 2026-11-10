package com.superessence.tagforge.nfc

import android.nfc.NdefMessage
import android.nfc.NdefRecord
import com.superessence.tagforge.tasks.TaskCodec
import com.superessence.tagforge.tasks.TaskItem

data class ParsedRecord(
    val kind: String,
    val value: String,
    val uri: String? = null,
    val tasks: List<TaskItem>? = null,
    val tnf: String = "",
    val type: String = "",
    val payloadHex: String = "",
    val size: Int = 0,
)

object NdefParser {

    private val TNF = mapOf(
        NdefRecord.TNF_EMPTY to "Vide", NdefRecord.TNF_WELL_KNOWN to "NFC Forum (well-known)",
        NdefRecord.TNF_MIME_MEDIA to "MIME", NdefRecord.TNF_ABSOLUTE_URI to "URI absolu",
        NdefRecord.TNF_EXTERNAL_TYPE to "Externe", NdefRecord.TNF_UNKNOWN to "Inconnu",
        NdefRecord.TNF_UNCHANGED to "Fragment",
    )

    fun parse(msg: NdefMessage?): List<ParsedRecord> = msg?.records?.map { parse(it) } ?: emptyList()

    fun parse(r: NdefRecord): ParsedRecord {
        val type = String(r.type, Charsets.US_ASCII)
        val payload = r.payload ?: ByteArray(0)
        val base = ParsedRecord("", "", tnf = TNF[r.tnf] ?: r.tnf.toString(), type = type,
            payloadHex = Encoders.hex(payload.take(512).toByteArray()) + if (payload.size > 512) " …" else "", size = r.toByteArray().size)
        return when (r.tnf) {
            NdefRecord.TNF_EMPTY -> base.copy(kind = "Vide", value = "(enregistrement vide)")
            NdefRecord.TNF_WELL_KNOWN -> when {
                r.type.contentEquals(NdefRecord.RTD_URI) -> {
                    val u = r.toUri()?.toString().orEmpty()
                    base.copy(kind = uriKind(u), value = prettyUri(u), uri = u)
                }
                r.type.contentEquals(NdefRecord.RTD_TEXT) -> base.copy(kind = "Texte", value = text(payload))
                r.type.contentEquals(NdefRecord.RTD_SMART_POSTER) -> {
                    val inner = runCatching { NdefMessage(payload) }.getOrNull()
                    val parts = parse(inner)
                    val u = parts.firstOrNull { it.uri != null }?.uri
                    val title = parts.firstOrNull { it.kind == "Texte" }?.value
                    base.copy(kind = "Smart Poster", value = listOfNotNull(title, u).joinToString("\n"), uri = u)
                }
                else -> base.copy(kind = "Well-known « $type »", value = raw(payload))
            }
            NdefRecord.TNF_MIME_MEDIA -> when (type.lowercase()) {
                Encoders.WSC_MIME -> {
                    val w = Encoders.parseWifi(payload)
                    base.copy(kind = "Wi-Fi", value = if (w == null) "Configuration Wi-Fi" else "Réseau : ${w.ssid}\nSécurité : ${w.auth}\nMot de passe : ${w.password.ifEmpty { "(aucun)" }}")
                }
                Encoders.BT_MIME -> base.copy(kind = "Bluetooth", value = Encoders.parseBluetooth(payload))
                "text/vcard", "text/x-vcard" -> base.copy(kind = "Contact (vCard)", value = vcardSummary(String(payload, Charsets.UTF_8)))
                else -> base.copy(kind = "MIME $type", value = raw(payload))
            }
            NdefRecord.TNF_ABSOLUTE_URI -> base.copy(kind = "URI", value = type, uri = type)
            NdefRecord.TNF_EXTERNAL_TYPE -> when (type) {
                "android.com:pkg" -> base.copy(kind = "Ouvrir l'appli (AAR)", value = String(payload, Charsets.US_ASCII))
                TaskCodec.EXTERNAL -> {
                    val t = TaskCodec.decode(String(payload, Charsets.UTF_8))
                    base.copy(kind = "Tâches TagForge (${t.size})", value = t.joinToString("\n") { "• " + it.summary() }, tasks = t)
                }
                else -> base.copy(kind = "Externe $type", value = raw(payload))
            }
            else -> base.copy(kind = base.tnf, value = raw(payload))
        }
    }

    fun text(p: ByteArray): String {
        if (p.isEmpty()) return ""
        val status = p[0].toInt()
        val langLen = status and 0x3F
        val cs = if (status and 0x80 != 0) Charsets.UTF_16 else Charsets.UTF_8
        return runCatching { String(p, 1 + langLen, p.size - 1 - langLen, cs) }.getOrDefault("")
    }

    private fun raw(p: ByteArray) = if (Encoders.printable(p)) String(p, Charsets.UTF_8) else Encoders.hex(p.take(256).toByteArray())

    private fun vcardSummary(v: String): String = v.lines().mapNotNull { l ->
        val k = l.substringBefore(':').substringBefore(';').uppercase()
        val value = l.substringAfter(':', "").replace(";", " ").trim()
        when (k) {
            "FN" -> "Nom : $value"; "ORG" -> "Entreprise : $value"; "TITLE" -> "Fonction : $value"
            "TEL" -> "Tél : $value"; "EMAIL" -> "E-mail : $value"; "URL" -> "Site : $value"; "ADR" -> "Adresse : $value"
            "NOTE" -> "Note : $value"; else -> null
        }
    }.joinToString("\n")

    fun uriKind(u: String): String {
        val l = u.lowercase()
        return when {
            l.startsWith("tel:") -> "Téléphone"
            l.startsWith("sms:") || l.startsWith("smsto:") -> "SMS"
            l.startsWith("mailto:") -> "E-mail"
            l.startsWith("geo:") -> "Position GPS"
            l.contains("translate_tts") -> "Voix Google"
            l.contains("translate.google") -> "Google Traduction"
            l.contains("wa.me/") -> "WhatsApp"
            l.contains("google.com/maps/dir") -> "Itinéraire"
            l.contains("google.com/maps") || l.contains("maps.app.goo.gl") -> "Google Maps"
            l.contains("waze.com") -> "Waze"
            l.contains("writereview") -> "Avis Google"
            l.contains("calendar.google.com") -> "Agenda"
            l.contains("play.google.com") -> "Play Store"
            l.contains("youtu") -> "YouTube"
            l.contains("instagram.com") -> "Instagram"
            l.contains("tiktok.com") -> "TikTok"
            l.contains("paypal.me") -> "PayPal"
            l.startsWith("http") -> "Lien web"
            else -> "URI"
        }
    }

    private fun prettyUri(u: String): String = runCatching {
        when {
            u.contains("translate_tts") -> "« " + java.net.URLDecoder.decode(u.substringAfter("&q=").substringBefore('&'), "UTF-8") + " »"
            else -> java.net.URLDecoder.decode(u.replace("+", "%2B"), "UTF-8")
        }
    }.getOrDefault(u)
}
