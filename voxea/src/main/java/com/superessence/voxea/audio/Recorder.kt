package com.superessence.voxea.audio

import android.annotation.SuppressLint
import android.media.AudioFormat
import android.media.AudioRecord
import android.media.MediaRecorder
import android.media.audiofx.NoiseSuppressor
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlin.concurrent.thread
import kotlin.math.abs
import kotlin.math.max

/**
 * Capture micro continue (44,1 kHz mono). Le prompteur découpe le flux en segments, un par réplique,
 * sans couper le micro entre deux répliques.
 */
class Recorder {
    val sampleRate = 44100
    private val _level = MutableStateFlow(0f)
    val level: StateFlow<Float> = _level
    private val _running = MutableStateFlow(false)
    val running: StateFlow<Boolean> = _running

    /** Une session = un AudioRecord + son fil de lecture, avec son propre drapeau d'arrêt. */
    private class Session(val record: AudioRecord, val ns: NoiseSuppressor?) {
        @Volatile var active = true
        var thread: Thread? = null
    }

    private var session: Session? = null
    private val lock = Any()
    private var buf = ShortArray(sampleRate * 30)
    private var size = 0
    private var segStart = 0

    @SuppressLint("MissingPermission")
    fun start(): Boolean {
        if (session != null) return true
        val min = AudioRecord.getMinBufferSize(sampleRate, AudioFormat.CHANNEL_IN_MONO, AudioFormat.ENCODING_PCM_16BIT)
        if (min <= 0) return false
        val r = try {
            AudioRecord(MediaRecorder.AudioSource.VOICE_RECOGNITION, sampleRate, AudioFormat.CHANNEL_IN_MONO,
                AudioFormat.ENCODING_PCM_16BIT, max(min, sampleRate / 2))
        } catch (e: Exception) { return false }
        if (r.state != AudioRecord.STATE_INITIALIZED) { r.release(); return false }
        val ns = if (NoiseSuppressor.isAvailable()) runCatching { NoiseSuppressor.create(r.audioSessionId)?.apply { enabled = true } }.getOrNull() else null
        try { r.startRecording() } catch (e: Exception) { ns?.release(); r.release(); return false }
        val s = Session(r, ns)
        synchronized(lock) { size = 0; segStart = 0 }
        session = s
        _running.value = true
        s.thread = thread(name = "voxea-rec", priority = Thread.MAX_PRIORITY) {
            val tmp = ShortArray(2048)
            while (s.active) {
                val n = r.read(tmp, 0, tmp.size)
                if (n < 0) break // micro perdu (ERROR_DEAD_OBJECT…)
                if (n == 0) continue
                var peak = 0
                synchronized(lock) {
                    if (size + n > buf.size) buf = buf.copyOf(buf.size * 2)
                    System.arraycopy(tmp, 0, buf, size, n); size += n
                }
                for (i in 0 until n) peak = max(peak, abs(tmp[i].toInt()))
                _level.value = peak / 32768f
            }
            // Libération par le fil lui-même : jamais pendant un read() en cours
            runCatching { r.stop() }
            r.release(); s.ns?.release()
        }
        return true
    }

    /** Commence un nouveau segment à partir de maintenant (jette ce qui précède). */
    fun beginSegment() = synchronized(lock) {
        // On garde le tampon compact : on repart de zéro à chaque segment.
        size = 0; segStart = 0
    }

    /** Termine le segment courant et renvoie l'audio capturé depuis [beginSegment]. */
    fun endSegment(): FloatArray = synchronized(lock) {
        val out = FloatArray(size - segStart) { buf[segStart + it] / 32768f }
        size = 0; segStart = 0
        out
    }

    fun stop() {
        val s = session ?: return
        session = null
        s.active = false
        s.thread?.join(500)
        _running.value = false
        _level.value = 0f
    }
}
