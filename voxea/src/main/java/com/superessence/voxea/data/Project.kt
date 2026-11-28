package com.superessence.voxea.data

import com.superessence.voxea.core.Line
import com.superessence.voxea.core.VoiceSettings
import com.superessence.voxea.core.VoiceType
import com.superessence.voxea.core.defaultSettings
import kotlinx.serialization.Serializable
import kotlinx.serialization.encodeToString
import kotlinx.serialization.json.Json
import java.util.UUID

@Serializable
data class Project(
    val id: String,
    val name: String,
    val createdAt: Long,
    val rawScript: String,
    val lines: List<Line>,
    /** Voix choisie par personnage. */
    val castType: Map<String, VoiceType> = emptyMap(),
    /** Réglages fins par personnage (sinon réglages par défaut du type). */
    val castSettings: Map<String, VoiceSettings> = emptyMap(),
    /** Voix imposée pour une réplique précise (index → type). */
    val lineVoice: Map<Int, VoiceType> = emptyMap(),
) {
    val characters: List<String> get() = lines.map { it.character }.distinct()

    fun voiceTypeFor(line: Line): VoiceType =
        lineVoice[line.index] ?: castType[line.character] ?: VoiceType.ORIGINAL

    fun settingsFor(line: Line): VoiceSettings {
        lineVoice[line.index]?.let { if (it != castType[line.character]) return it.defaultSettings() }
        return castSettings[line.character] ?: voiceTypeFor(line).defaultSettings()
    }

    fun settingsForCharacter(c: String): VoiceSettings =
        castSettings[c] ?: (castType[c] ?: VoiceType.ORIGINAL).defaultSettings()

    fun withCharacterVoice(c: String, t: VoiceType) =
        copy(castType = castType + (c to t), castSettings = castSettings - c)

    fun withCharacterSettings(c: String, s: VoiceSettings) = copy(castSettings = castSettings + (c to s))

    fun withLineVoice(index: Int, t: VoiceType?) =
        copy(lineVoice = if (t == null) lineVoice - index else lineVoice + (index to t))

    fun toJson(): String = json.encodeToString(this)

    companion object {
        private val json = Json { ignoreUnknownKeys = true; encodeDefaults = true }

        fun fromJson(s: String): Project = json.decodeFromString(serializer(), s)

        fun create(name: String, raw: String, lines: List<Line>, now: Long = System.currentTimeMillis()): Project {
            val cast = LinkedHashMap<String, VoiceType>()
            for (l in lines) if (l.character !in cast || (cast[l.character] == VoiceType.ORIGINAL && l.voiceHint != null)) {
                cast[l.character] = l.voiceHint ?: VoiceType.ORIGINAL
            }
            return Project(UUID.randomUUID().toString(), name, now, raw, lines, castType = cast)
        }
    }
}
