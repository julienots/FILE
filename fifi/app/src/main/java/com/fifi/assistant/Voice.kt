package com.fifi.assistant

import android.content.Context
import android.media.AudioAttributes
import android.media.MediaPlayer
import org.json.JSONObject

/** Charge assets/voice/index.json (généré par tools/build_voice.py). */
fun loadVoiceBank(ctx: Context): VoiceBank {
    val json = ctx.assets.open("voice/index.json").bufferedReader().use { it.readText() }
    val root = JSONObject(json)
    val map = HashMap<String, List<Line>>()
    for (cat in root.keys()) {
        val arr = root.getJSONArray(cat)
        map[cat] = (0 until arr.length()).map {
            val o = arr.getJSONObject(it)
            Line(o.getString("f"), o.getString("t"))
        }
    }
    return VoiceBank(map)
}

/** Joue une suite de clips (la voix de Fifi) les uns après les autres. */
class VoicePlayer(private val ctx: Context) {
    private var player: MediaPlayer? = null
    private var queue = ArrayDeque<String>()
    private var onDone: (() -> Unit)? = null

    val isPlaying get() = player != null

    fun play(clips: List<String>, done: () -> Unit) {
        stop()
        queue = ArrayDeque(clips.filter { it.isNotEmpty() })
        onDone = done
        next()
    }

    private fun next() {
        player?.release(); player = null
        val f = queue.removeFirstOrNull()
        if (f == null) {
            val d = onDone; onDone = null; d?.invoke(); return
        }
        try {
            val afd = ctx.assets.openFd("voice/$f")
            val mp = MediaPlayer()
            mp.setAudioAttributes(
                AudioAttributes.Builder()
                    .setUsage(AudioAttributes.USAGE_ASSISTANT)
                    .setContentType(AudioAttributes.CONTENT_TYPE_SPEECH).build()
            )
            mp.setDataSource(afd.fileDescriptor, afd.startOffset, afd.length)
            afd.close()
            mp.setOnCompletionListener { next() }
            mp.setOnErrorListener { _, _, _ -> next(); true }
            mp.prepare()
            player = mp
            mp.start()
        } catch (e: Exception) {
            next()
        }
    }

    fun stop() {
        player?.let { runCatching { it.stop() }; it.release() }
        player = null
        queue.clear()
        onDone = null
    }
}
