package com.superessence.voxea.data

import android.content.Context
import com.superessence.voxea.core.VoiceType

/** Préférences locales (la clé API reste sur l'appareil, sauvegarde Android désactivée). */
class Settings(ctx: Context) {
    private val sp = ctx.getSharedPreferences("voxea", Context.MODE_PRIVATE)

    var apiKey: String
        get() = sp.getString("el_key", "") ?: ""
        set(v) = sp.edit().putString("el_key", v.trim()).apply()

    var useAi: Boolean
        get() = sp.getBoolean("use_ai", false) && apiKey.isNotBlank()
        set(v) = sp.edit().putBoolean("use_ai", v).apply()

    var introSeen: Boolean
        get() = sp.getBoolean("intro_seen", false)
        set(v) = sp.edit().putBoolean("intro_seen", v).apply()

    var prompterSize: Float
        get() = sp.getFloat("prompter_size", 30f)
        set(v) = sp.edit().putFloat("prompter_size", v).apply()

    var volumeKeys: Boolean
        get() = sp.getBoolean("volume_keys", true)
        set(v) = sp.edit().putBoolean("volume_keys", v).apply()

    fun voiceId(t: VoiceType): String = sp.getString("el_voice_${t.name}", DEFAULT_VOICES[t] ?: "") ?: ""
    fun setVoiceId(t: VoiceType, id: String) = sp.edit().putString("el_voice_${t.name}", id.trim()).apply()
    fun voiceIds(): Map<VoiceType, String> = VoiceType.entries.associateWith { voiceId(it) }

    companion object {
        /** Voix « premade » publiques d'ElevenLabs ; modifiables dans Réglages. */
        val DEFAULT_VOICES = mapOf(
            VoiceType.FEMME to "21m00Tcm4TlvDq8ikWAM",
            VoiceType.HOMME to "pNInz6obpgDQGcFmaJgB",
            VoiceType.AGE to "VR6AewLTigWG4xSOukaG",
        )
    }
}
