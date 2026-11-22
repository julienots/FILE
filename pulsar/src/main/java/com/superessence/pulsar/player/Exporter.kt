package com.superessence.pulsar.player

import android.content.ContentValues
import android.content.Context
import android.os.Build
import android.os.Environment
import android.provider.MediaStore
import com.superessence.pulsar.engine.Engine
import com.superessence.pulsar.engine.LiveParams
import com.superessence.pulsar.engine.SR
import com.superessence.pulsar.engine.Song
import java.io.BufferedOutputStream
import java.io.File
import java.io.FileOutputStream
import java.io.OutputStream
import java.nio.ByteBuffer
import java.nio.ByteOrder

/** Export du morceau complet en WAV 16 bits / 44,1 kHz dans Musique/Pulsar. */
object Exporter {
    private const val TAIL = 3f

    fun exportWav(context: Context, song: Song, liveNow: LiveParams, onProgress: (Float) -> Unit): String? {
        // copie des réglages (le mixage actuel est conservé, sans les fondus du minuteur)
        val live = LiveParams().also {
            it.bpm = liveNow.bpm; it.masterVolume = liveNow.masterVolume.coerceAtLeast(0.6f)
            liveNow.volume.copyInto(it.volume); liveNow.mute.copyInto(it.mute); liveNow.solo.copyInto(it.solo)
            it.reverb = liveNow.reverb; it.delay = liveNow.delay; it.djFilter = liveNow.djFilter; it.drive = liveNow.drive
            it.sidechain = liveNow.sidechain; it.lofi = liveNow.lofi; it.vinyl = liveNow.vinyl; it.rain = liveNow.rain; it.width = liveNow.width
        }
        val bpm = if (live.bpm > 0f) live.bpm else song.bpm
        val frames = (song.bars.size * 4f * 60f / bpm * SR).toLong() + (TAIL * SR).toLong()
        val dataBytes = frames * 4
        val safeTitle = song.title.replace(Regex("[^\\p{L}\\p{N} _-]"), "").trim()
        val fileName = "PULSAR - $safeTitle (${song.genre.name}) ${song.code}.wav"

        var out: OutputStream? = null
        var uri: android.net.Uri? = null
        val shownPath: String
        try {
            if (Build.VERSION.SDK_INT >= 29) {
                val values = ContentValues().apply {
                    put(MediaStore.Audio.Media.DISPLAY_NAME, fileName)
                    put(MediaStore.Audio.Media.MIME_TYPE, "audio/wav")
                    put(MediaStore.Audio.Media.RELATIVE_PATH, Environment.DIRECTORY_MUSIC + "/Pulsar")
                    put(MediaStore.Audio.Media.TITLE, song.title)
                    put(MediaStore.Audio.Media.ARTIST, "PULSAR")
                    put(MediaStore.Audio.Media.ALBUM, song.genre.name)
                    put(MediaStore.Audio.Media.IS_PENDING, 1)
                }
                uri = context.contentResolver.insert(MediaStore.Audio.Media.EXTERNAL_CONTENT_URI, values) ?: return null
                out = context.contentResolver.openOutputStream(uri) ?: return null
                shownPath = "Musique/Pulsar/$fileName"
            } else {
                val dir = File(context.getExternalFilesDir(Environment.DIRECTORY_MUSIC), "Pulsar").apply { mkdirs() }
                val f = File(dir, fileName)
                out = FileOutputStream(f)
                shownPath = f.absolutePath
            }
            val bos = BufferedOutputStream(out, 1 shl 16)
            bos.write(header(dataBytes))
            val bb = ByteBuffer.allocate(4096 * 4).order(ByteOrder.LITTLE_ENDIAN)
            var written = 0L
            Engine.renderOffline(song, live, TAIL, onProgress) { buf, n ->
                bb.clear()
                val take = minOf(n.toLong(), frames - written).toInt()
                for (i in 0 until take * 2) bb.putShort((buf[i].coerceIn(-1f, 1f) * 32767f).toInt().toShort())
                bos.write(bb.array(), 0, bb.position())
                written += take
            }
            // complète si besoin (arrondis)
            while (written < frames) { bos.write(ByteArray(4)); written++ }
            bos.flush(); bos.close()
            if (uri != null && Build.VERSION.SDK_INT >= 29) {
                context.contentResolver.update(uri, ContentValues().apply { put(MediaStore.Audio.Media.IS_PENDING, 0) }, null, null)
            }
            return shownPath
        } catch (t: Throwable) {
            runCatching { out?.close() }
            if (uri != null) runCatching { context.contentResolver.delete(uri, null, null) }
            return null
        }
    }

    private fun header(dataBytes: Long): ByteArray {
        val h = ByteBuffer.allocate(44).order(ByteOrder.LITTLE_ENDIAN)
        h.put("RIFF".toByteArray()); h.putInt((36 + dataBytes).toInt()); h.put("WAVE".toByteArray())
        h.put("fmt ".toByteArray()); h.putInt(16); h.putShort(1); h.putShort(2); h.putInt(SR); h.putInt(SR * 4); h.putShort(4); h.putShort(16)
        h.put("data".toByteArray()); h.putInt(dataBytes.toInt())
        return h.array()
    }
}
