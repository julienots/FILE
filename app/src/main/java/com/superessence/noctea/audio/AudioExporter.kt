package com.superessence.noctea.audio

import android.content.ContentValues
import android.content.Context
import android.media.MediaCodec
import android.media.MediaCodecInfo
import android.media.MediaExtractor
import android.media.MediaFormat
import android.media.MediaMuxer
import android.net.Uri
import android.os.Build
import android.os.Environment
import android.provider.MediaStore
import com.superessence.noctea.data.AmbienceLayer
import com.superessence.noctea.data.Seg
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.ensureActive
import kotlinx.coroutines.withContext
import java.io.File
import java.nio.ByteOrder
import kotlin.coroutines.coroutineContext

/**
 * Exporte une séance (voix + ambiance) en fichier audio AAC (.m4a) écoutable partout,
 * même en dehors de l'application. Rendu 48 kHz mono, 96 kb/s.
 */
class AudioExporter(private val context: Context) {

    data class Result(val uri: Uri, val displayPath: String)

    private val sr = 48_000

    suspend fun export(
        playable: Playable,
        ambience: List<AmbienceLayer>,
        musicVolume: Float,
        onProgress: (Float) -> Unit,
    ): Result = withContext(Dispatchers.Default) {
        val name = sanitize(playable.title) + ".m4a"
        val (uri, fd, path) = openOutput(name)
        try {
            val muxer = MediaMuxer(fd.fileDescriptor, MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4)
            val encoder = createEncoder()
            val beds = ambience.mapNotNull { layer ->
                runCatching { decodeAsset("sounds/${layer.sound}.ogg") }.getOrNull()?.let { it to layer.volume * musicVolume * 0.85f }
            }
            val totalSamples = playable.durationMs * sr / 1000
            var written = 0L
            var trackIndex = -1
            var muxing = false
            val info = MediaCodec.BufferInfo()

            fun drain(endOfStream: Boolean) {
                while (true) {
                    val idx = encoder.dequeueOutputBuffer(info, if (endOfStream) 10_000 else 0)
                    when {
                        idx == MediaCodec.INFO_TRY_AGAIN_LATER -> if (!endOfStream) return
                        idx == MediaCodec.INFO_OUTPUT_FORMAT_CHANGED -> {
                            trackIndex = muxer.addTrack(encoder.outputFormat); muxer.start(); muxing = true
                        }
                        idx >= 0 -> {
                            val buf = encoder.getOutputBuffer(idx)!!
                            if (info.flags and MediaCodec.BUFFER_FLAG_CODEC_CONFIG != 0) info.size = 0
                            if (info.size > 0 && muxing) {
                                buf.position(info.offset); buf.limit(info.offset + info.size)
                                muxer.writeSampleData(trackIndex, buf, info)
                            }
                            encoder.releaseOutputBuffer(idx, false)
                            if (info.flags and MediaCodec.BUFFER_FLAG_END_OF_STREAM != 0) return
                        }
                    }
                }
            }

            fun feed(samples: FloatArray, count: Int, eos: Boolean = false) {
                var off = 0
                while (off < count || (eos && off == count)) {
                    val inIdx = encoder.dequeueInputBuffer(10_000)
                    if (inIdx < 0) { drain(false); continue }
                    val inBuf = encoder.getInputBuffer(inIdx)!!
                    inBuf.clear()
                    val n = minOf(count - off, inBuf.capacity() / 2)
                    val sb = inBuf.order(ByteOrder.nativeOrder()).asShortBuffer()
                    for (i in 0 until n) {
                        val pos = written + i
                        var v = samples[off + i]
                        for ((bed, vol) in beds) v += bed[(pos % bed.size).toInt()] * vol * fadeAt(pos, totalSamples)
                        sb.put((softClip(v) * 32767f).toInt().toShort())
                    }
                    val pts = written * 1_000_000L / sr
                    val last = eos && off + n >= count
                    encoder.queueInputBuffer(inIdx, 0, n * 2, pts, if (last) MediaCodec.BUFFER_FLAG_END_OF_STREAM else 0)
                    off += n
                    written += n
                    drain(false)
                    if (last) break
                }
            }

            val chunk = FloatArray(sr)
            for (seg in playable.segments) {
                coroutineContext.ensureActive()
                when (seg) {
                    is Seg.Silence -> {
                        var left = seg.ms * sr / 1000
                        while (left > 0) {
                            val n = minOf(left, chunk.size.toLong()).toInt()
                            java.util.Arrays.fill(chunk, 0, n, 0f)
                            feed(chunk, n)
                            left -= n
                        }
                    }
                    is Seg.Clip -> decodeAsset("voice/${seg.id}.ogg").let { feed(it, it.size) }
                    is Seg.Fx -> decodeAsset("voice/${seg.name}.ogg").let { feed(it, it.size) }
                    is Seg.File -> runCatching { decodeFile(seg.path) }.getOrNull()?.let { feed(it, it.size) }
                }
                onProgress((written.toFloat() / totalSamples).coerceIn(0f, 1f))
            }
            feed(FloatArray(0), 0, eos = true)
            drain(true)
            encoder.stop(); encoder.release()
            if (muxing) muxer.stop()
            muxer.release()
            fd.close()
            finish(uri)
            Result(uri, path)
        } catch (e: Throwable) {
            runCatching { fd.close() }
            runCatching { context.contentResolver.delete(uri, null, null) }
            throw e
        }
    }

    private fun fadeAt(pos: Long, total: Long): Float {
        val fade = sr * 4L
        return when {
            pos < fade -> pos / fade.toFloat()
            pos > total - fade -> ((total - pos).coerceAtLeast(0) / fade.toFloat())
            else -> 1f
        }
    }

    private fun softClip(x: Float) = if (x > 0.95f || x < -0.95f) kotlin.math.tanh(x) else x

    private fun createEncoder(): MediaCodec {
        val fmt = MediaFormat.createAudioFormat(MediaFormat.MIMETYPE_AUDIO_AAC, sr, 1).apply {
            setInteger(MediaFormat.KEY_AAC_PROFILE, MediaCodecInfo.CodecProfileLevel.AACObjectLC)
            setInteger(MediaFormat.KEY_BIT_RATE, 96_000)
            setInteger(MediaFormat.KEY_MAX_INPUT_SIZE, 16_384)
        }
        return MediaCodec.createEncoderByType(MediaFormat.MIMETYPE_AUDIO_AAC).apply {
            configure(fmt, null, null, MediaCodec.CONFIGURE_FLAG_ENCODE); start()
        }
    }

    private fun decodeAsset(path: String): FloatArray {
        val afd = context.assets.openFd(path)
        val ex = MediaExtractor()
        ex.setDataSource(afd.fileDescriptor, afd.startOffset, afd.length)
        return try { decode(ex) } finally { ex.release(); afd.close() }
    }

    private fun decodeFile(path: String): FloatArray {
        val ex = MediaExtractor()
        ex.setDataSource(path)
        return try { decode(ex) } finally { ex.release() }
    }

    /** Décode la première piste audio en PCM mono 48 kHz (flottant). */
    private fun decode(ex: MediaExtractor): FloatArray {
        val track = (0 until ex.trackCount).first { ex.getTrackFormat(it).getString(MediaFormat.KEY_MIME)!!.startsWith("audio/") }
        ex.selectTrack(track)
        val fmt = ex.getTrackFormat(track)
        val codec = MediaCodec.createDecoderByType(fmt.getString(MediaFormat.KEY_MIME)!!)
        codec.configure(fmt, null, null, 0)
        codec.start()
        var outRate = fmt.getInteger(MediaFormat.KEY_SAMPLE_RATE)
        var channels = fmt.getInteger(MediaFormat.KEY_CHANNEL_COUNT)
        val pcm = ShortArrayBuilder()
        val info = MediaCodec.BufferInfo()
        var inputDone = false
        var outputDone = false
        while (!outputDone) {
            if (!inputDone) {
                val inIdx = codec.dequeueInputBuffer(5_000)
                if (inIdx >= 0) {
                    val buf = codec.getInputBuffer(inIdx)!!
                    val n = ex.readSampleData(buf, 0)
                    if (n < 0) {
                        codec.queueInputBuffer(inIdx, 0, 0, 0, MediaCodec.BUFFER_FLAG_END_OF_STREAM); inputDone = true
                    } else {
                        codec.queueInputBuffer(inIdx, 0, n, ex.sampleTime, 0); ex.advance()
                    }
                }
            }
            val outIdx = codec.dequeueOutputBuffer(info, 5_000)
            when {
                outIdx == MediaCodec.INFO_OUTPUT_FORMAT_CHANGED -> {
                    outRate = codec.outputFormat.getInteger(MediaFormat.KEY_SAMPLE_RATE)
                    channels = codec.outputFormat.getInteger(MediaFormat.KEY_CHANNEL_COUNT)
                }
                outIdx >= 0 -> {
                    val buf = codec.getOutputBuffer(outIdx)!!
                    buf.position(info.offset); buf.limit(info.offset + info.size)
                    val sb = buf.order(ByteOrder.nativeOrder()).asShortBuffer()
                    val frames = sb.remaining() / channels
                    for (f in 0 until frames) {
                        var acc = 0
                        for (c in 0 until channels) acc += sb.get()
                        pcm.add((acc / channels).toShort())
                    }
                    codec.releaseOutputBuffer(outIdx, false)
                    if (info.flags and MediaCodec.BUFFER_FLAG_END_OF_STREAM != 0) outputDone = true
                }
            }
        }
        codec.stop(); codec.release()
        val mono = pcm.toArray()
        return resample(mono, outRate)
    }

    private fun resample(src: ShortArray, rate: Int): FloatArray {
        if (rate == sr) return FloatArray(src.size) { src[it] / 32768f }
        val ratio = rate.toDouble() / sr
        val n = (src.size / ratio).toInt()
        return FloatArray(n) { i ->
            val x = i * ratio
            val i0 = x.toInt().coerceAtMost(src.size - 1)
            val i1 = (i0 + 1).coerceAtMost(src.size - 1)
            val t = (x - i0).toFloat()
            (src[i0] * (1 - t) + src[i1] * t) / 32768f
        }
    }

    private class ShortArrayBuilder {
        private var data = ShortArray(1 shl 16)
        private var size = 0
        fun add(v: Short) {
            if (size == data.size) data = data.copyOf(size * 2)
            data[size++] = v
        }
        fun toArray(): ShortArray = data.copyOf(size)
    }

    private data class Output(val uri: Uri, val fd: android.os.ParcelFileDescriptor, val path: String)

    private fun openOutput(name: String): Output {
        return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            val values = ContentValues().apply {
                put(MediaStore.Audio.Media.DISPLAY_NAME, name)
                put(MediaStore.Audio.Media.MIME_TYPE, "audio/mp4")
                put(MediaStore.Audio.Media.RELATIVE_PATH, Environment.DIRECTORY_MUSIC + "/NOCTEA")
                put(MediaStore.Audio.Media.ARTIST, "NOCTÉA — Superessence Studio")
                put(MediaStore.Audio.Media.IS_PENDING, 1)
            }
            val uri = context.contentResolver.insert(MediaStore.Audio.Media.EXTERNAL_CONTENT_URI, values)
                ?: error("Impossible de créer le fichier")
            val fd = context.contentResolver.openFileDescriptor(uri, "rw") ?: error("Fichier inaccessible")
            Output(uri, fd, "Musique/NOCTEA/$name")
        } else {
            val dir = File(context.getExternalFilesDir(Environment.DIRECTORY_MUSIC), "NOCTEA").apply { mkdirs() }
            val file = File(dir, name)
            val fd = android.os.ParcelFileDescriptor.open(
                file,
                android.os.ParcelFileDescriptor.MODE_CREATE or android.os.ParcelFileDescriptor.MODE_READ_WRITE or
                    android.os.ParcelFileDescriptor.MODE_TRUNCATE,
            )
            Output(Uri.fromFile(file), fd, file.absolutePath)
        }
    }

    private fun finish(uri: Uri) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            context.contentResolver.update(uri, ContentValues().apply { put(MediaStore.Audio.Media.IS_PENDING, 0) }, null, null)
        }
    }

    private fun sanitize(s: String) = "NOCTEA - " + s.replace(Regex("""[\\/:*?"<>|]"""), " ").trim()
}
