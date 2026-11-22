package com.superessence.pulsar

import com.superessence.pulsar.engine.*
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.File
import java.io.RandomAccessFile
import kotlin.math.abs
import kotlin.math.sqrt

class EngineRenderTest {

    @Test
    fun everyGenreComposesAndRendersCleanly() {
        val outDir = System.getProperty("pulsar.previews").orEmpty()
        for (g in Genres.all) {
            for (seed in listOf(1L, 42L)) {
                val song = Composer.compose(GenParams(g.id, seed))
                assertTrue(song.bars.isNotEmpty())
                val live = LiveParams()
                var peak = 0f; var sum = 0.0; var count = 0L; var nan = false
                val writer = if (outDir.isNotEmpty() && seed == 42L) WavWriter(File(outDir, "${g.id}.wav")) else null
                // on rend les 40 premières secondes pour les stats (ou tout pour l'aperçu)
                val maxFrames = if (writer != null) Long.MAX_VALUE else (40 * SR).toLong()
                var frames = 0L
                val t0 = System.nanoTime()
                try {
                    Engine.renderOffline(song, live, 2f) { buf, n ->
                        if (frames >= maxFrames) throw StopRender()
                        for (i in 0 until n * 2) {
                            val v = buf[i]
                            if (v.isNaN() || v.isInfinite()) nan = true
                            val a = abs(v); if (a > peak) peak = a
                            sum += v * v; count++
                        }
                        writer?.write(buf, n)
                        frames += n
                    }
                } catch (_: StopRender) {}
                writer?.close()
                val ms = (System.nanoTime() - t0) / 1e6
                val rms = sqrt(sum / count)
                val e = Engine.lastOffline!!
                val preRms = sqrt(e.statPreSum / e.statPreCount)
                println("    pré-limiteur: crête=%.2f rms=%.3f".format(e.statPrePeak, preRms))
                val dur = frames.toFloat() / SR
                println("%-10s seed=%-3d bars=%3d bpm=%5.1f key=%-14s rms=%.3f peak=%.3f  rendu x%.0f temps réel  « %s »".format(
                    g.id, seed, song.bars.size, song.bpm, song.keyName, rms, peak, dur * 1000 / ms, song.title))
                assertTrue("${g.id}: NaN", !nan)
                assertTrue("${g.id}: peak $peak", peak <= 1.0f)
                assertTrue("${g.id}: trop faible rms=$rms", rms > 0.02)
            }
        }
    }

    private class StopRender : RuntimeException()

    private class WavWriter(f: File) {
        private val raf = RandomAccessFile(f, "rw").apply { setLength(0); write(ByteArray(44)) }
        private var bytes = 0L
        private val bb = java.nio.ByteBuffer.allocate(4096 * 4).order(java.nio.ByteOrder.LITTLE_ENDIAN)
        fun write(buf: FloatArray, n: Int) {
            bb.clear()
            for (i in 0 until n * 2) bb.putShort((buf[i].coerceIn(-1f, 1f) * 32767).toInt().toShort())
            raf.write(bb.array(), 0, bb.position()); bytes += bb.position()
        }
        fun close() {
            val h = java.nio.ByteBuffer.allocate(44).order(java.nio.ByteOrder.LITTLE_ENDIAN)
            h.put("RIFF".toByteArray()); h.putInt((36 + bytes).toInt()); h.put("WAVE".toByteArray())
            h.put("fmt ".toByteArray()); h.putInt(16); h.putShort(1); h.putShort(2); h.putInt(SR); h.putInt(SR * 4); h.putShort(4); h.putShort(16)
            h.put("data".toByteArray()); h.putInt(bytes.toInt())
            raf.seek(0); raf.write(h.array()); raf.close()
        }
    }
}
