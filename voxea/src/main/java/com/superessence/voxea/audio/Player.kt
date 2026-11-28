package com.superessence.voxea.audio

import android.media.MediaPlayer
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import java.io.File

class Player {
    private var mp: MediaPlayer? = null
    private val _playing = MutableStateFlow<String?>(null)
    /** Chemin du fichier en cours de lecture. */
    val playing: StateFlow<String?> = _playing

    /** @return false si le fichier ne peut pas être lu (absent, en cours d'écriture, abîmé). */
    fun play(f: File, onEnd: () -> Unit = {}): Boolean {
        stop()
        if (!f.exists()) return false
        val m = MediaPlayer()
        return try {
            m.setDataSource(f.absolutePath)
            m.setOnCompletionListener { this@Player.stop(); onEnd() }
            m.prepare(); m.start()
            mp = m
            _playing.value = f.absolutePath
            true
        } catch (e: Exception) {
            m.release(); false
        }
    }

    fun toggle(f: File): Boolean { if (_playing.value == f.absolutePath) { stop(); return true }; return play(f) }

    fun stop() {
        mp?.runCatching { stop() }
        mp?.release(); mp = null
        _playing.value = null
    }
}
