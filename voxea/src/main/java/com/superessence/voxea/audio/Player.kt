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

    fun play(f: File, onEnd: () -> Unit = {}) {
        stop()
        if (!f.exists()) return
        mp = MediaPlayer().apply {
            setDataSource(f.absolutePath)
            setOnCompletionListener { this@Player.stop(); onEnd() }
            prepare(); start()
        }
        _playing.value = f.absolutePath
    }

    fun toggle(f: File) { if (_playing.value == f.absolutePath) stop() else play(f) }

    fun stop() {
        mp?.runCatching { stop() }
        mp?.release(); mp = null
        _playing.value = null
    }
}
