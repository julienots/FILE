package com.superessence.noctea.data

import android.content.Context
import android.content.SharedPreferences
import com.superessence.noctea.generator.GeneratorParams
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import kotlinx.serialization.Serializable
import kotlinx.serialization.encodeToString
import kotlinx.serialization.json.Json
import java.time.LocalDate

@Serializable
data class Settings(
    val userName: String = "",
    val theme: String = "nuit",               // nuit | aurore | ocean
    val autoNight: Boolean = true,            // atténuation automatique la nuit
    val minimalPlayer: Boolean = false,
    val voiceVolume: Float = 1f,
    val musicVolume: Float = 0.8f,
    val playbackSpeed: Float = 1f,
    val defaultSleepTimerMin: Int = 0,
    val onboardingDone: Boolean = false,
    val ttsProvider: String = "system",       // system | remote
    val systemVoiceName: String = "",
    val ttsRate: Float = 0.82f,
    val ttsPitch: Float = 0.96f,
    val remoteUrl: String = "",
    val remoteApiKey: String = "",            // stocké localement par l'utilisateur, jamais dans le code
    val remoteVoice: String = "",
    val notifyQuotes: Boolean = false,
)

@Serializable
data class HistoryEntry(val mediaId: String, val title: String, val at: Long, val listenedMs: Long = 0)

@Serializable
data class CustomMeditation(
    val id: String,
    val createdAt: Long,
    val title: String,
    val params: GeneratorParams,
    val durationMs: Long,
)

@Serializable
data class TextClip(val path: String, val ms: Long, val pauseAfterMs: Long)

@Serializable
data class TextMeditation(
    val id: String,
    val createdAt: Long,
    val title: String,
    val text: String,
    val provider: String,
    val clips: List<TextClip>,
    val ambience: String = "",
    val ambienceVolume: Float = 0.4f,
    val durationMs: Long,
)

@Serializable
data class JournalEntry(val id: String, val date: String, val at: Long, val mood: Int, val text: String, val gratitude: String = "")

@Serializable
data class RitualStep(val type: String, val refId: String, val minutes: Int) // type: track | custom | mixer

@Serializable
data class Ritual(
    val bedtimeMinutes: Int = 23 * 60,
    val reminderEnabled: Boolean = false,
    val reminderOffsetMin: Int = 30,
    val steps: List<RitualStep> = listOf(
        RitualStep("track", "breath_478", 4),
        RitualStep("track", "relax_calm", 5),
        RitualStep("track", "sleep_deep", 15),
        RitualStep("mixer", "rain_soft", 30),
    ),
)

@Serializable
data class Alarm(val enabled: Boolean = false, val minutes: Int = 7 * 60, val trackId: String = "wake_gentle", val days: Set<Int> = setOf(1, 2, 3, 4, 5, 6, 7))

@Serializable
data class MixPreset(val id: String, val name: String, val layers: Map<String, Float>)

@Serializable
data class Snapshot(
    val settings: Settings = Settings(),
    val favorites: Set<String> = emptySet(),
    val history: List<HistoryEntry> = emptyList(),
    val positions: Map<String, Long> = emptyMap(),
    val customs: List<CustomMeditation> = emptyList(),
    val texts: List<TextMeditation> = emptyList(),
    val journal: List<JournalEntry> = emptyList(),
    val ritual: Ritual = Ritual(),
    val alarm: Alarm = Alarm(),
    val practiceDays: Map<String, Long> = emptyMap(),   // date ISO → ms écoutées
    val mixPresets: List<MixPreset> = emptyList(),
    val lastMix: Map<String, Float> = mapOf("rain_soft" to 0.6f),
    val ritualDone: Set<String> = emptySet(),
    val generationsByDay: Map<String, Int> = emptyMap(),
)

/**
 * Persistance locale (SharedPreferences + JSON). Toutes les données restent sur l'appareil.
 * L'export/import JSON sert de sauvegarde et de base à une future synchronisation de compte.
 */
class UserStore(context: Context) {
    private val prefs: SharedPreferences = context.getSharedPreferences("noctea_user", Context.MODE_PRIVATE)
    private val json = Json { ignoreUnknownKeys = true; encodeDefaults = true }
    private val _state = MutableStateFlow(load())
    val state: StateFlow<Snapshot> = _state.asStateFlow()
    val value: Snapshot get() = _state.value

    private fun load(): Snapshot = try {
        prefs.getString(KEY, null)?.let { json.decodeFromString<Snapshot>(it) } ?: Snapshot()
    } catch (e: Exception) {
        Snapshot()
    }

    @Synchronized
    fun edit(block: (Snapshot) -> Snapshot) {
        _state.update(block)
        prefs.edit().putString(KEY, json.encodeToString(_state.value)).apply()
    }

    fun settings(block: (Settings) -> Settings) = edit { it.copy(settings = block(it.settings)) }

    fun toggleFavorite(id: String) = edit {
        it.copy(favorites = if (id in it.favorites) it.favorites - id else it.favorites + id)
    }

    fun addHistory(mediaId: String, title: String) = edit {
        val list = listOf(HistoryEntry(mediaId, title, System.currentTimeMillis())) + it.history
        it.copy(history = list.take(200))
    }

    fun addListened(mediaId: String, ms: Long) = edit { s ->
        val today = LocalDate.now().toString()
        val days = s.practiceDays + (today to ((s.practiceDays[today] ?: 0L) + ms))
        val hist = s.history.toMutableList()
        val idx = hist.indexOfFirst { it.mediaId == mediaId }
        if (idx >= 0) hist[idx] = hist[idx].copy(listenedMs = hist[idx].listenedMs + ms)
        s.copy(practiceDays = days, history = hist)
    }

    fun savePosition(mediaId: String, pos: Long) = edit {
        it.copy(positions = if (pos <= 0) it.positions - mediaId else it.positions + (mediaId to pos))
    }

    fun countGeneration() = edit {
        val today = LocalDate.now().toString()
        it.copy(generationsByDay = mapOf(today to ((it.generationsByDay[today] ?: 0) + 1)))
    }

    fun generationsToday(): Int = value.generationsByDay[LocalDate.now().toString()] ?: 0

    fun exportJson(): String = json.encodeToString(value)

    fun importJson(text: String): Boolean = try {
        val snap = json.decodeFromString<Snapshot>(text)
        edit { snap }
        true
    } catch (e: Exception) {
        false
    }

    companion object {
        private const val KEY = "snapshot_v1"
    }
}
