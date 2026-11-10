package com.superessence.tagforge.nfc

import android.nfc.NdefMessage
import android.nfc.NdefRecord
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.*
import androidx.compose.ui.graphics.vector.ImageVector
import com.superessence.tagforge.nfc.Encoders.enc
import com.superessence.tagforge.tasks.LANGS
import com.superessence.tagforge.tasks.TaskCodec
import com.superessence.tagforge.tasks.TaskItem
import com.superessence.tagforge.tasks.TaskType
import org.json.JSONObject

enum class FieldKind { TEXT, MULTILINE, NUMBER, PHONE, EMAIL, URL, SELECT, SWITCH, PASSWORD, TASKS, APP }

data class FieldDef(
    val key: String,
    val label: String,
    val kind: FieldKind = FieldKind.TEXT,
    val hint: String = "",
    val options: List<Pair<String, String>> = emptyList(),
    val default: String = "",
    val required: Boolean = false,
)

enum class Category(val label: String) {
    WEB("Web & liens"), COMM("Communication"), VOICE("Voix & Google"), PLACES("Lieux & navigation"),
    NETWORK("Wi-Fi & Bluetooth"), SOCIAL("Réseaux sociaux"), BUSINESS("Pro, agenda & paiement"),
    APPS("Applications"), AUTOMATION("Automatisation (TagForge)"), DATA("Données & avancé"),
}

/** NATIVE : fonctionne sur tout Android sans aucune appli. TAGFORGE : nécessite TagForge installé sur le téléphone qui lit. */
enum class Support(val label: String) { NATIVE("Sans appli"), TAGFORGE("Avec TagForge") }

class RecordType(
    val id: String,
    val title: String,
    val category: Category,
    val icon: ImageVector,
    val support: Support,
    val description: String,
    val fields: List<FieldDef>,
    val build: (Map<String, String>) -> List<NdefRecord>,
    val summary: (Map<String, String>) -> String,
)

/** Un enregistrement configuré par l'utilisateur (type + valeurs). */
data class RecordItem(val typeId: String, val values: Map<String, String>) {
    val type: RecordType? get() = RecordTypes.byId(typeId)

    fun withPlaceholders(n: Int) = RecordItem(typeId, values.mapValues { Encoders.placeholders(it.value, n) })

    fun toJson(): JSONObject = JSONObject().put("t", typeId).put("v", JSONObject(values))

    companion object {
        fun fromJson(o: JSONObject): RecordItem {
            val v = o.optJSONObject("v") ?: JSONObject()
            return RecordItem(o.getString("t"), v.keys().asSequence().associateWith { v.optString(it) })
        }
    }
}

object RecordTypes {

    private fun uri(u: String) = NdefRecord.createUri(u)
    private fun need(v: Map<String, String>, k: String, label: String): String =
        v[k]?.trim()?.takeIf { it.isNotEmpty() } ?: throw IllegalArgumentException("« $label » est obligatoire")

    private fun normUrl(u: String): String {
        val t = u.trim()
        return if (t.contains("://") || t.startsWith("mailto:") || t.startsWith("tel:")) t else "https://$t"
    }

    private fun tasks(list: List<TaskItem>): List<NdefRecord> {
        if (list.isEmpty()) throw IllegalArgumentException("Ajoutez au moins une tâche")
        return listOf(
            NdefRecord.createExternal(TaskCodec.DOMAIN, TaskCodec.TYPE, TaskCodec.encode(list).toByteArray(Charsets.UTF_8)),
            NdefRecord.createApplicationRecord(APP_PACKAGE),
        )
    }

    const val APP_PACKAGE = "com.superessence.tagforge"

    private val SOCIAL = listOf(
        "instagram" to "Instagram", "tiktok" to "TikTok", "youtube" to "YouTube", "x" to "X (Twitter)",
        "facebook" to "Facebook", "linkedin" to "LinkedIn", "snapchat" to "Snapchat", "threads" to "Threads",
        "twitch" to "Twitch", "pinterest" to "Pinterest", "github" to "GitHub", "telegram" to "Telegram",
        "discord" to "Discord (code d'invitation)", "spotify" to "Spotify (artiste)", "soundcloud" to "SoundCloud",
        "reddit" to "Reddit", "bluesky" to "Bluesky", "vinted" to "Vinted (membre)", "linktree" to "Linktree",
    )

    fun socialUrl(p: String, user: String): String {
        val u = user.trim().removePrefix("@")
        return when (p) {
            "instagram" -> "https://instagram.com/$u"
            "tiktok" -> "https://www.tiktok.com/@$u"
            "youtube" -> "https://www.youtube.com/@$u"
            "x" -> "https://x.com/$u"
            "facebook" -> "https://facebook.com/$u"
            "linkedin" -> "https://www.linkedin.com/in/$u"
            "snapchat" -> "https://www.snapchat.com/add/$u"
            "threads" -> "https://www.threads.net/@$u"
            "twitch" -> "https://www.twitch.tv/$u"
            "pinterest" -> "https://pinterest.com/$u"
            "github" -> "https://github.com/$u"
            "telegram" -> "https://t.me/$u"
            "discord" -> "https://discord.gg/$u"
            "spotify" -> "https://open.spotify.com/artist/$u"
            "soundcloud" -> "https://soundcloud.com/$u"
            "reddit" -> "https://www.reddit.com/user/$u"
            "bluesky" -> "https://bsky.app/profile/$u"
            "vinted" -> "https://www.vinted.fr/member/$u"
            "linktree" -> "https://linktr.ee/$u"
            else -> "https://$u"
        }
    }

    private val MAPS_MODES = listOf("driving" to "Voiture", "walking" to "À pied", "bicycling" to "Vélo", "transit" to "Transports")

    val ALL: List<RecordType> = listOf(
        // ------------------------------------------------------------------ WEB
        RecordType("url", "Lien web / URL", Category.WEB, Icons.Filled.Link, Support.NATIVE,
            "Ouvre la page dans le navigateur dès l'approche.",
            listOf(FieldDef("url", "Adresse", FieldKind.URL, "https://exemple.fr", required = true)),
            { listOf(uri(normUrl(need(it, "url", "Adresse")))) }, { normUrl(it["url"].orEmpty()) }),
        RecordType("poster", "Smart Poster (lien + titre)", Category.WEB, Icons.Filled.Web, Support.NATIVE,
            "Lien avec un titre lisible (format NFC Forum Smart Poster).",
            listOf(FieldDef("url", "Adresse", FieldKind.URL, "https://exemple.fr", required = true), FieldDef("title", "Titre", hint = "Mon site")),
            { v ->
                val inner = mutableListOf(uri(normUrl(need(v, "url", "Adresse"))))
                v["title"]?.takeIf { it.isNotBlank() }?.let { inner += NdefRecord.createTextRecord("fr", it) }
                listOf(NdefRecord(NdefRecord.TNF_WELL_KNOWN, NdefRecord.RTD_SMART_POSTER, ByteArray(0), NdefMessage(inner.toTypedArray()).toByteArray()))
            }, { (it["title"].orEmpty() + " " + it["url"].orEmpty()).trim() }),
        RecordType("google_search", "Recherche Google", Category.WEB, Icons.Filled.Search, Support.NATIVE,
            "Lance une recherche Google.",
            listOf(FieldDef("q", "Recherche", hint = "restaurant italien près de moi", required = true)),
            { listOf(uri("https://www.google.com/search?q=" + enc(need(it, "q", "Recherche")))) }, { "Google : " + it["q"] }),
        RecordType("google_review", "Avis Google (fiche établissement)", Category.WEB, Icons.Filled.Star, Support.NATIVE,
            "Ouvre directement la fenêtre « Laisser un avis » de votre établissement. Place ID : trouvable via l'outil Google « Place ID Finder ».",
            listOf(FieldDef("place", "Place ID", hint = "ChIJLU7jZClu5kcR4PcOOO6p3I0", required = true)),
            { listOf(uri("https://search.google.com/local/writereview?placeid=" + enc(need(it, "place", "Place ID")))) }, { "Avis : " + it["place"] }),
        RecordType("youtube_video", "Vidéo YouTube", Category.WEB, Icons.Filled.SmartDisplay, Support.NATIVE,
            "Ouvre une vidéo (dans l'appli YouTube si installée).",
            listOf(FieldDef("id", "Lien ou identifiant de la vidéo", hint = "dQw4w9WgXcQ", required = true)),
            { v -> val id = need(v, "id", "Vidéo"); listOf(uri(if (id.contains("/")) normUrl(id) else "https://youtu.be/$id")) }, { "YouTube : " + it["id"] }),
        RecordType("file_link", "Fichier / document en ligne", Category.WEB, Icons.Filled.Description, Support.NATIVE,
            "Lien vers un PDF, Google Drive, Dropbox, menu de restaurant…",
            listOf(FieldDef("url", "Lien du fichier", FieldKind.URL, "https://drive.google.com/…", required = true)),
            { listOf(uri(normUrl(need(it, "url", "Lien")))) }, { it["url"].orEmpty() }),

        // ------------------------------------------------------------------ COMMUNICATION
        RecordType("tel", "Appel téléphonique", Category.COMM, Icons.Filled.Phone, Support.NATIVE,
            "Ouvre le téléphone avec le numéro prêt à appeler.",
            listOf(FieldDef("n", "Numéro", FieldKind.PHONE, "+33 6 12 34 56 78", required = true)),
            { listOf(uri("tel:" + Encoders.digits(need(it, "n", "Numéro")))) }, { "Appel " + it["n"] }),
        RecordType("sms", "SMS", Category.COMM, Icons.Filled.Sms, Support.NATIVE,
            "Ouvre les messages avec le destinataire et le texte pré-remplis.",
            listOf(FieldDef("n", "Numéro", FieldKind.PHONE, required = true), FieldDef("m", "Message", FieldKind.MULTILINE)),
            { v -> val m = v["m"].orEmpty(); listOf(uri("sms:" + Encoders.digits(need(v, "n", "Numéro")) + if (m.isNotEmpty()) "?body=" + enc(m) else "")) },
            { "SMS " + it["n"] }),
        RecordType("email", "E-mail", Category.COMM, Icons.Filled.Email, Support.NATIVE,
            "Ouvre un e-mail pré-rempli (destinataire, objet, texte).",
            listOf(FieldDef("to", "Destinataire", FieldKind.EMAIL, required = true), FieldDef("s", "Objet"), FieldDef("b", "Message", FieldKind.MULTILINE)),
            { v ->
                val q = listOfNotNull(v["s"]?.takeIf { it.isNotBlank() }?.let { "subject=" + enc(it) }, v["b"]?.takeIf { it.isNotBlank() }?.let { "body=" + enc(it) })
                listOf(uri("mailto:" + need(v, "to", "Destinataire") + if (q.isEmpty()) "" else "?" + q.joinToString("&")))
            }, { "Mail " + it["to"] }),
        RecordType("whatsapp", "WhatsApp", Category.COMM, Icons.Filled.Chat, Support.NATIVE,
            "Ouvre une discussion WhatsApp avec un message pré-rempli.",
            listOf(FieldDef("n", "Numéro (format international)", FieldKind.PHONE, "33612345678", required = true), FieldDef("m", "Message", FieldKind.MULTILINE)),
            { v -> val m = v["m"].orEmpty(); listOf(uri("https://wa.me/" + need(v, "n", "Numéro").filter { it.isDigit() } + if (m.isNotEmpty()) "?text=" + enc(m) else "")) },
            { "WhatsApp " + it["n"] }),
        RecordType("telegram", "Telegram", Category.COMM, Icons.Filled.Send, Support.NATIVE,
            "Ouvre un profil, un groupe ou une chaîne Telegram.",
            listOf(FieldDef("u", "Nom d'utilisateur", hint = "moncompte", required = true)),
            { listOf(uri("https://t.me/" + need(it, "u", "Nom").removePrefix("@"))) }, { "Telegram @" + it["u"] }),
        RecordType("vcard", "Carte de visite (contact)", Category.COMM, Icons.Filled.ContactPage, Support.NATIVE,
            "Propose d'ajouter le contact au répertoire, sans appli.",
            listOf(
                FieldDef("first", "Prénom"), FieldDef("last", "Nom"), FieldDef("org", "Entreprise"), FieldDef("title", "Fonction"),
                FieldDef("phone", "Mobile", FieldKind.PHONE), FieldDef("phone2", "Tél. pro", FieldKind.PHONE), FieldDef("email", "E-mail", FieldKind.EMAIL),
                FieldDef("url", "Site web", FieldKind.URL), FieldDef("address", "Adresse"), FieldDef("birthday", "Anniversaire (AAAA-MM-JJ)"), FieldDef("note", "Note", FieldKind.MULTILINE),
            ),
            { v ->
                if (v["first"].isNullOrBlank() && v["last"].isNullOrBlank() && v["org"].isNullOrBlank()) throw IllegalArgumentException("Indiquez au moins un nom ou une entreprise")
                listOf(NdefRecord.createMime("text/vcard", Encoders.vcard(v).toByteArray(Charsets.UTF_8)))
            }, { listOf(it["first"], it["last"], it["org"]).filter { s -> !s.isNullOrBlank() }.joinToString(" ") }),

        // ------------------------------------------------------------------ VOIX
        RecordType("google_voice", "Voix Google — lecture à voix haute (sans appli)", Category.VOICE, Icons.Filled.RecordVoiceOver, Support.NATIVE,
            "Le téléphone ouvre la voix Google qui lit votre texte, sans aucune appli installée (via Google Traduction, 200 caractères max). Service Google non officiel : il peut évoluer.",
            listOf(
                FieldDef("text", "Texte à dire", FieldKind.MULTILINE, "Bienvenue chez nous !", required = true),
                FieldDef("lang", "Langue", FieldKind.SELECT, options = LANGS.map { it.first.substringBefore('-') to it.second }.distinctBy { it.first }, default = "fr"),
            ),
            { v ->
                val t = need(v, "text", "Texte")
                if (t.length > 200) throw IllegalArgumentException("200 caractères maximum pour la voix Google (actuellement ${t.length})")
                listOf(uri("https://translate.google.com/translate_tts?ie=UTF-8&client=tw-ob&tl=" + (v["lang"] ?: "fr") + "&q=" + enc(t)))
            }, { "🔊 " + it["text"] }),
        RecordType("google_translate", "Google Traduction (texte + bouton écouter)", Category.VOICE, Icons.Filled.Translate, Support.NATIVE,
            "Ouvre Google Traduction avec votre texte traduit ; un appui sur le haut-parleur le lit avec la voix Google.",
            listOf(
                FieldDef("text", "Texte", FieldKind.MULTILINE, required = true),
                FieldDef("sl", "Langue source", FieldKind.SELECT, options = listOf("auto" to "Détection auto") + LANGS.map { it.first.substringBefore('-') to it.second }.distinctBy { it.first }, default = "auto"),
                FieldDef("tl", "Traduire vers", FieldKind.SELECT, options = LANGS.map { it.first.substringBefore('-') to it.second }.distinctBy { it.first }, default = "en"),
            ),
            { v -> listOf(uri("https://translate.google.com/?sl=${v["sl"] ?: "auto"}&tl=${v["tl"] ?: "en"}&op=translate&text=" + enc(need(v, "text", "Texte")))) },
            { "Traduire : " + it["text"] }),
        RecordType("speak", "Annonce vocale instantanée", Category.VOICE, Icons.Filled.VolumeUp, Support.TAGFORGE,
            "Le téléphone parle immédiatement à l'approche, même appli fermée et sans Internet (voix Google du téléphone).",
            listOf(
                FieldDef("text", "Texte à dire", FieldKind.MULTILINE, "Bonjour, n'oubliez pas vos clés !", required = true),
                FieldDef("lang", "Langue", FieldKind.SELECT, options = LANGS, default = "fr-FR"),
                FieldDef("rate", "Vitesse", FieldKind.SELECT, options = listOf("0.7" to "Lente", "1.0" to "Normale", "1.3" to "Rapide"), default = "1.0"),
            ),
            { v -> tasks(listOf(TaskItem(TaskType.SPEAK, listOf(need(v, "text", "Texte"), v["lang"] ?: "fr-FR", v["rate"] ?: "1.0")))) },
            { "🔊 " + it["text"] }),
        RecordType("assistant", "Assistant Google / Gemini", Category.VOICE, Icons.Filled.Mic, Support.TAGFORGE,
            "Lance l'assistant vocal du téléphone à l'approche (ou une recherche Google si un texte est indiqué).",
            listOf(FieldDef("q", "Question (facultatif)", hint = "Quel temps fera-t-il demain ?")),
            { v -> val q = v["q"].orEmpty().trim(); tasks(listOf(if (q.isEmpty()) TaskItem.new(TaskType.ASSISTANT) else TaskItem(TaskType.SEARCH, listOf(q)))) },
            { if (it["q"].isNullOrBlank()) "Assistant vocal" else "Assistant : " + it["q"] }),

        // ------------------------------------------------------------------ LIEUX
        RecordType("geo", "Position GPS", Category.PLACES, Icons.Filled.Place, Support.NATIVE,
            "Ouvre la carte sur des coordonnées précises.",
            listOf(FieldDef("lat", "Latitude", FieldKind.NUMBER, "48.8584", required = true), FieldDef("lng", "Longitude", FieldKind.NUMBER, "2.2945", required = true), FieldDef("label", "Nom du lieu")),
            { v ->
                val la = need(v, "lat", "Latitude").replace(',', '.'); val lo = need(v, "lng", "Longitude").replace(',', '.')
                val l = v["label"].orEmpty()
                listOf(uri("geo:$la,$lo?q=$la,$lo" + if (l.isNotBlank()) "(" + enc(l) + ")" else ""))
            }, { "GPS ${it["lat"]}, ${it["lng"]}" }),
        RecordType("maps_place", "Adresse (Google Maps)", Category.PLACES, Icons.Filled.Map, Support.NATIVE,
            "Ouvre Google Maps sur une adresse ou un lieu.",
            listOf(FieldDef("q", "Adresse ou lieu", hint = "10 rue de Rivoli, Paris", required = true)),
            { listOf(uri("https://www.google.com/maps/search/?api=1&query=" + enc(need(it, "q", "Adresse")))) }, { "Maps : " + it["q"] }),
        RecordType("maps_dir", "Itinéraire (Google Maps)", Category.PLACES, Icons.Filled.Directions, Support.NATIVE,
            "Lance le guidage vers une destination.",
            listOf(FieldDef("d", "Destination", hint = "Gare de Lyon", required = true), FieldDef("m", "Mode", FieldKind.SELECT, options = MAPS_MODES, default = "driving")),
            { v -> listOf(uri("https://www.google.com/maps/dir/?api=1&destination=" + enc(need(v, "d", "Destination")) + "&travelmode=" + (v["m"] ?: "driving") + "&dir_action=navigate")) },
            { "Itinéraire → " + it["d"] }),
        RecordType("waze", "Itinéraire Waze", Category.PLACES, Icons.Filled.Navigation, Support.NATIVE,
            "Lance la navigation Waze.",
            listOf(FieldDef("q", "Destination", required = true)),
            { listOf(uri("https://waze.com/ul?q=" + enc(need(it, "q", "Destination")) + "&navigate=yes")) }, { "Waze → " + it["q"] }),

        // ------------------------------------------------------------------ RÉSEAU
        RecordType("wifi", "Connexion Wi-Fi", Category.NETWORK, Icons.Filled.Wifi, Support.NATIVE,
            "Propose de rejoindre le réseau Wi-Fi sans taper le mot de passe (Android 5+).",
            listOf(
                FieldDef("ssid", "Nom du réseau (SSID)", required = true),
                FieldDef("auth", "Sécurité", FieldKind.SELECT, options = listOf("WPA2" to "WPA2 / WPA3", "WPA_WPA2" to "WPA/WPA2", "WPA" to "WPA", "OPEN" to "Ouvert (sans mot de passe)"), default = "WPA2"),
                FieldDef("pass", "Mot de passe", FieldKind.PASSWORD),
                FieldDef("hidden", "Réseau masqué", FieldKind.SWITCH, default = "false"),
            ),
            { v ->
                val auth = v["auth"] ?: "WPA2"
                val pass = v["pass"].orEmpty()
                if (auth != "OPEN" && pass.length < 8) throw IllegalArgumentException("Le mot de passe Wi-Fi doit faire au moins 8 caractères")
                listOf(NdefRecord.createMime(Encoders.WSC_MIME, Encoders.wifiPayload(need(v, "ssid", "SSID"), pass, auth, v["hidden"] == "true")))
            }, { "Wi-Fi " + it["ssid"] }),
        RecordType("bluetooth", "Appairage Bluetooth", Category.NETWORK, Icons.Filled.Bluetooth, Support.NATIVE,
            "Lance l'appairage avec une enceinte, un casque… (adresse MAC de l'appareil).",
            listOf(FieldDef("mac", "Adresse MAC", hint = "AA:BB:CC:DD:EE:FF", required = true), FieldDef("name", "Nom de l'appareil")),
            { v -> listOf(NdefRecord.createMime(Encoders.BT_MIME, Encoders.bluetoothPayload(need(v, "mac", "Adresse MAC"), v["name"].orEmpty()))) },
            { "Bluetooth " + (it["name"]?.takeIf { s -> s.isNotBlank() } ?: it["mac"]) }),

        // ------------------------------------------------------------------ SOCIAL
        RecordType("social", "Profil réseau social", Category.SOCIAL, Icons.Filled.Public, Support.NATIVE,
            "Instagram, TikTok, YouTube, X, Facebook, LinkedIn, Snapchat… ouvre le profil (dans l'appli si installée).",
            listOf(FieldDef("p", "Réseau", FieldKind.SELECT, options = SOCIAL, default = "instagram"), FieldDef("u", "Nom d'utilisateur", hint = "@moncompte", required = true)),
            { v -> listOf(uri(socialUrl(v["p"] ?: "instagram", need(v, "u", "Nom d'utilisateur")))) },
            { (SOCIAL.firstOrNull { p -> p.first == it["p"] }?.second ?: "") + " " + it["u"] }),
        RecordType("spotify", "Musique / playlist Spotify, Deezer…", Category.SOCIAL, Icons.Filled.MusicNote, Support.NATIVE,
            "Collez le lien de partage d'un titre, album ou playlist.",
            listOf(FieldDef("url", "Lien de partage", FieldKind.URL, "https://open.spotify.com/playlist/…", required = true)),
            { listOf(uri(normUrl(need(it, "url", "Lien")))) }, { "♪ " + it["url"] }),

        // ------------------------------------------------------------------ PRO
        RecordType("event", "Événement d'agenda (Google Agenda)", Category.BUSINESS, Icons.Filled.Event, Support.NATIVE,
            "Propose d'ajouter l'événement à l'agenda.",
            listOf(
                FieldDef("t", "Titre", required = true), FieldDef("s", "Début (AAAA-MM-JJ HH:MM)", hint = "2026-12-24 19:30", required = true),
                FieldDef("e", "Fin (AAAA-MM-JJ HH:MM)", hint = "2026-12-24 23:00"), FieldDef("l", "Lieu"), FieldDef("d", "Description", FieldKind.MULTILINE),
            ),
            { v ->
                val s = Encoders.calDate(need(v, "s", "Début")); val e = v["e"]?.takeIf { it.isNotBlank() }?.let { Encoders.calDate(it) } ?: s
                var u = "https://calendar.google.com/calendar/render?action=TEMPLATE&text=" + enc(need(v, "t", "Titre")) + "&dates=$s/$e"
                v["l"]?.takeIf { it.isNotBlank() }?.let { u += "&location=" + enc(it) }
                v["d"]?.takeIf { it.isNotBlank() }?.let { u += "&details=" + enc(it) }
                listOf(uri(u))
            }, { "📅 " + it["t"] }),
        RecordType("paypal", "Paiement PayPal.me", Category.BUSINESS, Icons.Filled.Payments, Support.NATIVE,
            "Ouvre la page de paiement PayPal avec le montant.",
            listOf(FieldDef("u", "Nom PayPal.me", required = true), FieldDef("a", "Montant (facultatif)", FieldKind.NUMBER, "10"), FieldDef("c", "Devise", FieldKind.SELECT, options = listOf("EUR" to "EUR", "USD" to "USD", "GBP" to "GBP", "CHF" to "CHF"), default = "EUR")),
            { v -> val a = v["a"].orEmpty().replace(',', '.'); listOf(uri("https://paypal.me/" + need(v, "u", "Nom") + if (a.isNotBlank()) "/$a${v["c"] ?: "EUR"}" else "")) },
            { "PayPal " + it["u"] + " " + it["a"].orEmpty() }),
        RecordType("revolut", "Paiement Revolut / Lydia / Wero (lien)", Category.BUSINESS, Icons.Filled.CreditCard, Support.NATIVE,
            "Lien de paiement (revolut.me/…, lien Lydia, lien de cagnotte…).",
            listOf(FieldDef("url", "Lien de paiement", FieldKind.URL, "https://revolut.me/moncompte", required = true)),
            { listOf(uri(normUrl(need(it, "url", "Lien")))) }, { "Paiement " + it["url"] }),
        RecordType("menu", "Menu / catalogue / fiche produit", Category.BUSINESS, Icons.Filled.Restaurant, Support.NATIVE,
            "Lien vers votre menu, catalogue ou fiche produit (restaurant, boutique, salon).",
            listOf(FieldDef("url", "Lien", FieldKind.URL, required = true)),
            { listOf(uri(normUrl(need(it, "url", "Lien")))) }, { "Menu " + it["url"] }),

        // ------------------------------------------------------------------ APPLIS
        RecordType("aar", "Ouvrir une application", Category.APPS, Icons.Filled.Apps, Support.NATIVE,
            "Ouvre directement l'appli choisie (ou sa page Play Store si elle n'est pas installée). Enregistrement Android natif (AAR).",
            listOf(FieldDef("pkg", "Application", FieldKind.APP, "com.spotify.music", required = true)),
            { listOf(NdefRecord.createApplicationRecord(need(it, "pkg", "Application"))) }, { "Appli " + it["pkg"] }),
        RecordType("playstore", "Page Play Store", Category.APPS, Icons.Filled.Shop, Support.NATIVE,
            "Ouvre la fiche Play Store d'une application.",
            listOf(FieldDef("pkg", "Application", FieldKind.APP, required = true)),
            { listOf(uri("https://play.google.com/store/apps/details?id=" + need(it, "pkg", "Application"))) }, { "Play Store " + it["pkg"] }),

        // ------------------------------------------------------------------ AUTOMATISATION
        RecordType("tasks", "Scénario de tâches (automatisation)", Category.AUTOMATION, Icons.Filled.Bolt, Support.TAGFORGE,
            "Enchaîne plusieurs actions à l'approche : parler, lampe, mode silencieux, volume, appli, alarme, minuteur, musique, Wi-Fi, luminosité…",
            listOf(FieldDef("tasks", "Tâches", FieldKind.TASKS, default = "[]")),
            { v -> tasks(TaskCodec.decode(v["tasks"] ?: "[]")) },
            { v -> TaskCodec.decode(v["tasks"] ?: "[]").joinToString(" → ") { it.type.title } }),
        RecordType("flash", "Lampe torche", Category.AUTOMATION, Icons.Filled.FlashlightOn, Support.TAGFORGE,
            "Allume / éteint la lampe à l'approche.",
            listOf(FieldDef("a", "Action", FieldKind.SELECT, options = TaskType.FLASH.params[0].options, default = "toggle")),
            { v -> tasks(listOf(TaskItem(TaskType.FLASH, listOf(v["a"] ?: "toggle")))) }, { "Lampe torche" }),
        RecordType("silent", "Mode silencieux / vibreur", Category.AUTOMATION, Icons.Filled.NotificationsOff, Support.TAGFORGE,
            "Idéal sur la table de nuit ou à l'entrée d'une réunion.",
            listOf(FieldDef("m", "Mode", FieldKind.SELECT, options = TaskType.RINGER.params[0].options, default = "vibrate")),
            { v -> tasks(listOf(TaskItem(TaskType.RINGER, listOf(v["m"] ?: "vibrate")))) }, { "Mode sonnerie" }),
        RecordType("alarm", "Réveil / alarme", Category.AUTOMATION, Icons.Filled.Alarm, Support.TAGFORGE,
            "Crée une alarme à l'heure voulue.",
            listOf(FieldDef("h", "Heure (HH:MM)", default = "07:00", required = true), FieldDef("l", "Libellé", default = "Réveil")),
            { v -> tasks(listOf(TaskItem(TaskType.ALARM, listOf(need(v, "h", "Heure"), v["l"].orEmpty())))) }, { "Alarme " + it["h"] }),
        RecordType("timer", "Minuteur", Category.AUTOMATION, Icons.Filled.Timer, Support.TAGFORGE,
            "Lance un minuteur (cuisine, lessive, sieste…).",
            listOf(FieldDef("m", "Minutes", FieldKind.NUMBER, default = "10", required = true), FieldDef("l", "Libellé", default = "Minuteur")),
            { v -> tasks(listOf(TaskItem(TaskType.TIMER, listOf(need(v, "m", "Minutes"), v["l"].orEmpty())))) }, { "Minuteur " + it["m"] + " min" }),

        // ------------------------------------------------------------------ DONNÉES
        RecordType("text", "Texte simple", Category.DATA, Icons.Filled.TextFields, Support.NATIVE,
            "Texte brut (lu par les applis NFC ; Android seul affiche « contenu non pris en charge »).",
            listOf(FieldDef("text", "Texte", FieldKind.MULTILINE, required = true), FieldDef("lang", "Code langue", default = "fr")),
            { v -> listOf(NdefRecord.createTextRecord(v["lang"]?.ifBlank { "fr" } ?: "fr", need(v, "text", "Texte"))) }, { it["text"].orEmpty() }),
        RecordType("custom_uri", "URI personnalisé", Category.DATA, Icons.Filled.Code, Support.NATIVE,
            "N'importe quel schéma : spotify:, geo:, market:, bitcoin:, intent d'une appli…",
            listOf(FieldDef("uri", "URI", hint = "spotify:playlist:37i9dQZF1DXcBWIGoYBM5M", required = true)),
            { listOf(uri(need(it, "uri", "URI"))) }, { it["uri"].orEmpty() }),
        RecordType("mime", "Donnée MIME", Category.DATA, Icons.Filled.DataObject, Support.NATIVE,
            "Enregistrement MIME personnalisé (ex. application/json).",
            listOf(FieldDef("type", "Type MIME", default = "application/json", required = true), FieldDef("data", "Contenu", FieldKind.MULTILINE, required = true)),
            { v -> listOf(NdefRecord.createMime(need(v, "type", "Type MIME"), need(v, "data", "Contenu").toByteArray(Charsets.UTF_8))) }, { (it["type"] ?: "") + " : " + it["data"] }),
        RecordType("external", "Type externe (NFC Forum)", Category.DATA, Icons.Filled.Extension, Support.NATIVE,
            "Enregistrement « domaine:type » pour vos propres applis.",
            listOf(FieldDef("domain", "Domaine", hint = "exemple.com", required = true), FieldDef("type", "Type", hint = "monformat", required = true), FieldDef("data", "Contenu", FieldKind.MULTILINE)),
            { v -> listOf(NdefRecord.createExternal(need(v, "domain", "Domaine"), need(v, "type", "Type"), v["data"].orEmpty().toByteArray(Charsets.UTF_8))) },
            { "${it["domain"]}:${it["type"]}" }),
    )

    fun byId(id: String) = ALL.firstOrNull { it.id == id }

    /** Construit le message complet ; ajoute l'AAR si demandé. */
    fun buildMessage(items: List<RecordItem>, addAar: String? = null): NdefMessage {
        val records = mutableListOf<NdefRecord>()
        items.forEach { item ->
            val t = item.type ?: throw IllegalArgumentException("Type inconnu : ${item.typeId}")
            val built = t.build(item.values)
            // un seul AAR TagForge suffit, placé en fin de message
            built.forEach { r -> if (!(isAar(r) && records.any { isAar(it) && it.payload.contentEquals(r.payload) })) records += r }
        }
        if (records.isEmpty()) throw IllegalArgumentException("Ajoutez au moins un contenu")
        if (!addAar.isNullOrBlank() && records.none { isAar(it) && String(it.payload) == addAar }) records += NdefRecord.createApplicationRecord(addAar)
        // les AAR doivent venir après les enregistrements de contenu
        val (aars, content) = records.partition { isAar(it) }
        // si le tag contient des tâches TagForge, elles passent en tête : TagForge les exécute puis ouvre les autres liens.
        val (tf, others) = content.partition { isTagForge(it) }
        return NdefMessage((tf + others + aars).toTypedArray())
    }

    fun isTagForge(r: NdefRecord) = r.tnf == NdefRecord.TNF_EXTERNAL_TYPE && String(r.type) == TaskCodec.EXTERNAL

    fun isAar(r: NdefRecord) = r.tnf == NdefRecord.TNF_EXTERNAL_TYPE && String(r.type) == "android.com:pkg"
}
