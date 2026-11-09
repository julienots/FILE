package com.superessence.noctea.audio

import android.content.Context
import androidx.media3.common.AudioAttributes
import androidx.media3.common.C
import androidx.media3.common.MediaItem
import androidx.media3.common.Player
import androidx.media3.exoplayer.ExoPlayer
import com.superessence.noctea.data.AmbienceLayer
import com.superessence.noctea.data.Sounds
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch

/**
 * Mixeur d'ambiances : une boucle par son (lecture sans coupure), volumes indépendants,
 * fondus doux. Suit l'état du lecteur principal (lecture/pause/minuteur).
 */
class AmbienceMixer(private val context: Context, private val scope: CoroutineScope) {

    private class Layer(val player: ExoPlayer, var target: Float, var current: Float)

    private val layers = linkedMapOf<String, Layer>()
    private val _levels = MutableStateFlow<Map<String, Float>>(emptyMap())
    val levels: StateFlow<Map<String, Float>> = _levels.asStateFlow()

    /** Volume général de la musique/ambiance (réglage utilisateur). */
    var master: Float = 0.8f
        set(value) { field = value.coerceIn(0f, 1f); applyVolumes() }

    /** Facteur de fondu global (minuteur de sommeil, pause). */
    private var gate = 0f
    private var playing = false
    private var fadeJob: Job? = null
    private var gateJob: Job? = null

    private val attrs = AudioAttributes.Builder()
        .setUsage(C.USAGE_MEDIA)
        .setContentType(C.AUDIO_CONTENT_TYPE_MUSIC)
        .build()

    private fun newPlayer(sound: String): ExoPlayer = ExoPlayer.Builder(context)
        .setAudioAttributes(attrs, false)
        .build().apply {
            repeatMode = Player.REPEAT_MODE_ONE
            setMediaItem(MediaItem.fromUri(Sounds.assetUri(sound)))
            volume = 0f
            prepare()
        }

    /** Remplace l'ensemble des couches (fondu enchaîné). */
    fun setLayers(newLayers: List<AmbienceLayer>) {
        val wanted = newLayers.filter { Sounds.get(it.sound) != null }.take(MAX_LAYERS).associate { it.sound to it.volume }
        layers.keys.filter { it !in wanted }.forEach { setLevel(it, 0f) }
        wanted.forEach { (id, v) -> setLevel(id, v) }
    }

    fun setLevel(sound: String, volume: Float) {
        val v = volume.coerceIn(0f, 1f)
        val layer = layers[sound]
        if (layer == null) {
            if (v <= 0f) return
            if (layers.size >= MAX_LAYERS) return
            val p = newPlayer(sound)
            layers[sound] = Layer(p, v, 0f)
            if (playing) p.play()
        } else {
            layer.target = v
        }
        publish()
        startFadeLoop()
    }

    fun clear() {
        layers.values.forEach { it.target = 0f }
        publish()
        startFadeLoop()
    }

    fun play() {
        playing = true
        layers.values.forEach { it.player.play() }
        fadeGate(1f, 1800)
    }

    fun pause(fadeMs: Long = 900) {
        playing = false
        fadeGate(0f, fadeMs)
    }

    /** Fondu global (minuteur de sommeil). */
    fun fadeGate(to: Float, ms: Long) {
        val from = gate
        gateJob?.cancel()
        gateJob = scope.launch {
            val steps = (ms / 50).coerceAtLeast(1)
            for (i in 1..steps) {
                gate = from + (to - from) * i / steps
                applyVolumes()
                delay(50)
            }
            gate = to
            applyVolumes()
            if (to == 0f && !playing) layers.values.forEach { it.player.pause() }
        }
    }

    private fun startFadeLoop() {
        if (fadeJob?.isActive == true) return
        fadeJob = scope.launch {
            while (true) {
                var moving = false
                val toRemove = mutableListOf<String>()
                for ((id, l) in layers) {
                    val d = l.target - l.current
                    if (kotlin.math.abs(d) > 0.004f) {
                        l.current += d.coerceIn(-0.025f, 0.025f)
                        moving = true
                    } else {
                        l.current = l.target
                        if (l.target == 0f) toRemove += id
                    }
                }
                applyVolumes()
                toRemove.forEach { id -> layers.remove(id)?.player?.release() }
                if (toRemove.isNotEmpty()) publish()
                if (!moving) break
                delay(40)
            }
        }
    }

    private fun applyVolumes() {
        layers.values.forEach { it.player.volume = (it.current * master * gate).coerceIn(0f, 1f) }
    }

    private fun publish() {
        _levels.value = layers.filter { it.value.target > 0f }.mapValues { it.value.target }
    }

    fun release() {
        layers.values.forEach { it.player.release() }
        layers.clear()
        publish()
    }

    companion object {
        const val MAX_LAYERS = 6
    }
}
