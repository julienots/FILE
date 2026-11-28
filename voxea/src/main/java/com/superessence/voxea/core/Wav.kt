package com.superessence.voxea.core

import java.io.DataInputStream
import java.io.File
import java.io.FileInputStream
import java.io.BufferedInputStream
import java.io.OutputStream
import java.nio.ByteBuffer
import java.nio.ByteOrder

/** Lecture / écriture WAV PCM 16 bits mono. */
object Wav {
    fun write(file: File, pcm: FloatArray, sampleRate: Int) {
        file.parentFile?.mkdirs()
        file.outputStream().buffered().use { write(it, pcm, sampleRate) }
    }

    fun write(out: OutputStream, pcm: FloatArray, sampleRate: Int) {
        val dataLen = pcm.size * 2
        val h = ByteBuffer.allocate(44).order(ByteOrder.LITTLE_ENDIAN)
        h.put("RIFF".toByteArray()); h.putInt(36 + dataLen); h.put("WAVE".toByteArray())
        h.put("fmt ".toByteArray()); h.putInt(16); h.putShort(1); h.putShort(1)
        h.putInt(sampleRate); h.putInt(sampleRate * 2); h.putShort(2); h.putShort(16)
        h.put("data".toByteArray()); h.putInt(dataLen)
        out.write(h.array())
        val buf = ByteBuffer.allocate(dataLen).order(ByteOrder.LITTLE_ENDIAN)
        for (v in pcm) buf.putShort((v.coerceIn(-1f, 1f) * 32767f).toInt().toShort())
        out.write(buf.array())
    }

    fun read(file: File): Pair<FloatArray, Int> {
        DataInputStream(BufferedInputStream(FileInputStream(file))).use { inp ->
            val all = inp.readBytes()
            val bb = ByteBuffer.wrap(all).order(ByteOrder.LITTLE_ENDIAN)
            var sr = 44100; var channels = 1; var bits = 16
            var p = 12
            while (p + 8 <= all.size) {
                val id = String(all, p, 4)
                val size = bb.getInt(p + 4)
                if (id == "fmt ") {
                    channels = bb.getShort(p + 10).toInt(); sr = bb.getInt(p + 12); bits = bb.getShort(p + 22).toInt()
                } else if (id == "data") {
                    val end = minOf(all.size, p + 8 + if (size <= 0) all.size else size)
                    require(bits == 16) { "WAV 16 bits attendu" }
                    val frames = (end - p - 8) / (2 * channels)
                    val out = FloatArray(frames)
                    var q = p + 8
                    for (i in 0 until frames) {
                        var s = 0f
                        for (c in 0 until channels) { s += bb.getShort(q) / 32768f; q += 2 }
                        out[i] = s / channels
                    }
                    return out to sr
                }
                p += 8 + size + (size and 1)
            }
            return FloatArray(0) to sr
        }
    }
}
