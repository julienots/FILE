package com.superessence.noctea.audio

import com.superessence.noctea.data.AmbienceLayer
import com.superessence.noctea.data.BreathBlock
import com.superessence.noctea.data.Catalog
import com.superessence.noctea.data.Seg
import com.superessence.noctea.data.Sounds
import com.superessence.noctea.data.UserStore
import com.superessence.noctea.generator.GeneratedMeditation
import com.superessence.noctea.generator.MeditationGenerator

enum class PlayableKind { TRACK, CUSTOM, TEXT, MIXER }

data class Playable(
    val mediaId: String,
    val kind: PlayableKind,
    val title: String,
    val subtitle: String,
    val art: String,
    val durationMs: Long,
    val segments: List<Seg>,
    val ambience: List<AmbienceLayer>,
    val breath: List<BreathBlock> = emptyList(),
    val text: String = "",
)

object MediaIds {
    fun track(id: String) = "track:$id"
    fun custom(id: String) = "custom:$id"
    fun text(id: String) = "text:$id"
    /** Mixeur nocturne : un silence porteur (pour l'écran verrouillé) + les ambiances. */
    fun mixer(minutes: Int, sound: String = "") = "mixer:$minutes:$sound"
}

/** Résout un identifiant de média en timeline jouable. */
class PlayableResolver(
    private val catalog: Catalog,
    private val store: UserStore,
    private val generator: MeditationGenerator,
) {
    private val cache = object : LinkedHashMap<String, GeneratedMeditation>(8, 0.75f, true) {
        override fun removeEldestEntry(eldest: MutableMap.MutableEntry<String, GeneratedMeditation>?) = size > 6
    }

    fun generated(customId: String): GeneratedMeditation? = synchronized(cache) {
        cache[customId] ?: store.value.customs.firstOrNull { it.id == customId }?.let { c ->
            generator.generate(c.params).also { cache[customId] = it }
        }
    }

    fun resolve(mediaId: String): Playable? {
        val type = mediaId.substringBefore(':')
        val rest = mediaId.substringAfter(':')
        return when (type) {
            "track" -> catalog.track(rest)?.let { t ->
                Playable(
                    mediaId, PlayableKind.TRACK, t.title, catalog.category(t.category)?.title ?: "",
                    t.art, t.durationMs, t.segments, t.ambience, t.breath, t.text,
                )
            }
            "custom" -> {
                val meta = store.value.customs.firstOrNull { it.id == rest } ?: return null
                val g = generated(rest) ?: return null
                Playable(
                    mediaId, PlayableKind.CUSTOM, meta.title, "Ma méditation · ${meta.params.minutes} min",
                    g.art, g.durationMs, g.segments, g.ambience, emptyList(), g.paragraphs.joinToString("\n"),
                )
            }
            "text" -> {
                val t = store.value.texts.firstOrNull { it.id == rest } ?: return null
                val segs = buildList {
                    add(Seg.Silence(1500))
                    t.clips.forEach { c ->
                        add(Seg.File(c.path, c.ms))
                        if (c.pauseAfterMs > 0) add(Seg.Silence(c.pauseAfterMs))
                    }
                    add(Seg.Silence(4000))
                }
                val amb = if (t.ambience.isNotEmpty()) listOf(AmbienceLayer(t.ambience, t.ambienceVolume)) else emptyList()
                Playable(mediaId, PlayableKind.TEXT, t.title, "Mon texte", "moon", segs.sumOf { it.ms }, segs, amb, text = t.text)
            }
            "mixer" -> {
                val minutes = rest.substringBefore(':').toIntOrNull() ?: 60
                val sound = rest.substringAfter(':', "")
                val layers = if (sound.isNotEmpty() && Sounds.get(sound) != null) {
                    listOf(AmbienceLayer(sound, 0.6f))
                } else {
                    store.value.lastMix.filter { it.value > 0f }.map { AmbienceLayer(it.key, it.value) }
                }
                val names = layers.mapNotNull { Sounds.get(it.sound)?.name }.joinToString(" · ")
                Playable(
                    mediaId, PlayableKind.MIXER, "Mixeur nocturne", names.ifEmpty { "Ambiance" }, "stars",
                    minutes * 60_000L, listOf(Seg.Silence(minutes * 60_000L)), layers,
                )
            }
            else -> null
        }
    }
}
