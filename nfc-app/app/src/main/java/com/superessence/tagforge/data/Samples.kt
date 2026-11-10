package com.superessence.tagforge.data

import com.superessence.tagforge.nfc.RecordItem
import com.superessence.tagforge.tasks.TaskCodec
import com.superessence.tagforge.tasks.TaskItem
import com.superessence.tagforge.tasks.TaskType

/** Modèles prêts à l'emploi proposés au premier lancement. */
object Samples {
    private fun t(name: String, vararg items: RecordItem) = Template(name, items.toList(), 0)
    private fun r(type: String, vararg kv: Pair<String, String>) = RecordItem(type, kv.toMap())

    val templates = listOf(
        t("🔊 Accueil vocal (voix Google, sans appli)", r("google_voice", "text" to "Bienvenue ! Le Wi-Fi est sur la porte du frigo.", "lang" to "fr")),
        t("📶 Wi-Fi invités", r("wifi", "ssid" to "MaBox-Invites", "auth" to "WPA2", "pass" to "motdepasse123")),
        t("👤 Ma carte de visite", r("vcard", "first" to "Prénom", "last" to "Nom", "org" to "Mon entreprise", "phone" to "+33600000000", "email" to "contact@exemple.fr")),
        t("⭐ Laisser un avis Google", r("google_review", "place" to "")),
        t("🛏️ Table de nuit", r("tasks", "tasks" to TaskCodec.encode(listOf(
            TaskItem(TaskType.RINGER, listOf("vibrate")),
            TaskItem(TaskType.BRIGHTNESS, listOf("10")),
            TaskItem(TaskType.ALARM, listOf("07:00", "Réveil")),
            TaskItem(TaskType.SPEAK, listOf("Bonne nuit. Réveil programmé à sept heures.", "fr-FR", "0.7")),
        )))),
        t("🚗 Voiture", r("tasks", "tasks" to TaskCodec.encode(listOf(
            TaskItem(TaskType.VOLUME, listOf("80", "music")),
            TaskItem(TaskType.SPEAK, listOf("Bonne route !", "fr-FR", "1.0")),
            TaskItem(TaskType.OPEN_APP, listOf("com.google.android.apps.maps")),
        )))),
        t("🏠 Retour à la maison", r("tasks", "tasks" to TaskCodec.encode(listOf(
            TaskItem(TaskType.RINGER, listOf("normal")),
            TaskItem(TaskType.SETTINGS, listOf("wifi")),
            TaskItem(TaskType.SPEAK, listOf("Bienvenue à la maison.", "fr-FR", "1.0")),
        )))),
        t("🎙️ Assistant Google", r("assistant")),
        t("📱 Réseaux sociaux", r("social", "p" to "instagram", "u" to "moncompte")),
        t("🍳 Minuteur cuisine 10 min", r("timer", "m" to "10", "l" to "Cuisine")),
    )
}
