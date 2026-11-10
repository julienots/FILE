package com.superessence.tagforge.tasks

import org.json.JSONArray

data class TaskParam(
    val label: String,
    val options: List<Pair<String, String>> = emptyList(),
    val default: String = "",
    val hint: String = "",
    val numeric: Boolean = false,
    val appPicker: Boolean = false,
)

enum class TaskType(val code: String, val title: String, val params: List<TaskParam> = emptyList()) {
    SPEAK("sp", "Parler (voix de synthèse)", listOf(
        TaskParam("Texte à dire", hint = "Bienvenue à la maison !"),
        TaskParam("Langue", LANGS, "fr-FR"),
        TaskParam("Vitesse", listOf("0.7" to "Lente", "1.0" to "Normale", "1.3" to "Rapide"), "1.0"),
    )),
    ASSISTANT("as", "Lancer l'Assistant Google / Gemini"),
    SEARCH("se", "Recherche Google", listOf(TaskParam("Recherche", hint = "météo demain"))),
    FLASH("fl", "Lampe torche", listOf(TaskParam("Action", listOf("toggle" to "Basculer", "on" to "Allumer", "off" to "Éteindre"), "toggle"))),
    RINGER("ri", "Mode sonnerie", listOf(TaskParam("Mode", listOf("normal" to "Sonnerie", "vibrate" to "Vibreur", "silent" to "Silencieux", "toggle" to "Basculer sonnerie/vibreur"), "vibrate"))),
    DND("dn", "Ne pas déranger", listOf(TaskParam("État", listOf("on" to "Activer", "off" to "Désactiver", "toggle" to "Basculer"), "toggle"))),
    VOLUME("vo", "Volume", listOf(
        TaskParam("Niveau (0-100)", default = "50", numeric = true),
        TaskParam("Flux", listOf("music" to "Média", "ring" to "Sonnerie", "alarm" to "Alarme", "notif" to "Notifications"), "music"),
    )),
    MEDIA("md", "Contrôle de la musique", listOf(TaskParam("Commande", listOf("play_pause" to "Lecture / pause", "next" to "Suivant", "previous" to "Précédent", "stop" to "Stop"), "play_pause"))),
    OPEN_APP("ap", "Ouvrir une application", listOf(TaskParam("Application (package)", hint = "com.spotify.music", appPicker = true))),
    OPEN_URL("ur", "Ouvrir un lien", listOf(TaskParam("Adresse", hint = "https://…"))),
    NAVIGATE("na", "Itinéraire (Google Maps)", listOf(
        TaskParam("Destination", hint = "Tour Eiffel, Paris"),
        TaskParam("Mode", listOf("d" to "Voiture", "w" to "À pied", "b" to "Vélo", "l" to "Deux-roues"), "d"),
    )),
    DIAL("ca", "Appeler un numéro", listOf(TaskParam("Numéro", hint = "+33 6 12 34 56 78"))),
    SMS("sm", "Préparer un SMS", listOf(TaskParam("Numéro"), TaskParam("Message"))),
    ALARM("al", "Créer une alarme", listOf(TaskParam("Heure (HH:MM)", default = "07:00"), TaskParam("Libellé", default = "Réveil"))),
    TIMER("ti", "Lancer un minuteur", listOf(TaskParam("Minutes", default = "5", numeric = true), TaskParam("Libellé", default = "Minuteur"))),
    SETTINGS("st", "Ouvrir un réglage", listOf(TaskParam("Réglage", listOf(
        "wifi" to "Wi-Fi", "internet" to "Internet / données", "bluetooth" to "Bluetooth", "nfc" to "NFC",
        "volume" to "Volume", "location" to "Localisation", "display" to "Affichage", "battery" to "Batterie",
        "hotspot" to "Partage de connexion", "airplane" to "Mode avion", "dnd" to "Ne pas déranger",
    ), "wifi"))),
    BRIGHTNESS("br", "Luminosité de l'écran", listOf(TaskParam("Niveau (0-100)", default = "80", numeric = true))),
    CAMERA("cm", "Ouvrir l'appareil photo", listOf(TaskParam("Mode", listOf("photo" to "Photo", "video" to "Vidéo"), "photo"))),
    VIBRATE("vi", "Vibrer", listOf(TaskParam("Durée (ms)", default = "400", numeric = true))),
    CLIPBOARD("cb", "Copier un texte", listOf(TaskParam("Texte"))),
    NOTIFY("no", "Afficher une notification", listOf(TaskParam("Titre", default = "TagForge"), TaskParam("Texte"))),
    TOAST("to", "Message à l'écran", listOf(TaskParam("Message"))),
    WAIT("wa", "Attendre", listOf(TaskParam("Secondes", default = "2", numeric = true))),
    ;

    companion object {
        fun byCode(c: String) = entries.firstOrNull { it.code == c }
    }
}

val LANGS = listOf(
    "fr-FR" to "Français", "en-US" to "Anglais (US)", "en-GB" to "Anglais (UK)", "es-ES" to "Espagnol",
    "de-DE" to "Allemand", "it-IT" to "Italien", "pt-PT" to "Portugais", "pt-BR" to "Portugais (Brésil)",
    "nl-NL" to "Néerlandais", "ar" to "Arabe", "zh-CN" to "Chinois", "ja-JP" to "Japonais",
    "ko-KR" to "Coréen", "ru-RU" to "Russe", "tr-TR" to "Turc", "pl-PL" to "Polonais",
)

data class TaskItem(val type: TaskType, val args: List<String>) {
    fun arg(i: Int): String = args.getOrNull(i)?.takeIf { it.isNotEmpty() } ?: type.params.getOrNull(i)?.default.orEmpty()

    fun summary(): String {
        val a = type.params.indices.map { i ->
            val p = type.params[i]
            val v = arg(i)
            p.options.firstOrNull { it.first == v }?.second ?: v
        }.filter { it.isNotBlank() }
        return if (a.isEmpty()) type.title else type.title + " : " + a.joinToString(" · ")
    }

    companion object {
        fun new(t: TaskType) = TaskItem(t, t.params.map { it.default })
    }
}

object TaskCodec {
    const val DOMAIN = "superessence.com"
    const val TYPE = "tagforge"
    const val EXTERNAL = "$DOMAIN:$TYPE"

    /** Format compact : [["sp","Bonjour","fr-FR","1.0"],["fl","on"]] (les arguments vides de fin sont omis). */
    fun encode(tasks: List<TaskItem>): String {
        val arr = JSONArray()
        tasks.forEach { t ->
            val a = JSONArray().put(t.type.code)
            val args = t.args.toMutableList()
            // les valeurs par défaut de fin sont retirées pour gagner de la place sur le tag
            while (args.isNotEmpty() && (args.last().isEmpty() || args.last() == t.type.params.getOrNull(args.size - 1)?.default)) args.removeAt(args.size - 1)
            args.forEach { a.put(it) }
            arr.put(a)
        }
        return arr.toString()
    }

    fun decode(s: String): List<TaskItem> = runCatching {
        val arr = JSONArray(s)
        (0 until arr.length()).mapNotNull { i ->
            val a = arr.getJSONArray(i)
            val type = TaskType.byCode(a.getString(0)) ?: return@mapNotNull null
            TaskItem(type, (1 until a.length()).map { a.getString(it) })
        }
    }.getOrDefault(emptyList())
}
