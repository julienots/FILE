package com.superessence.voxea

import com.superessence.voxea.core.Line
import com.superessence.voxea.core.Mp3
import com.superessence.voxea.core.Wav
import com.superessence.voxea.core.fileNameFor
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.ByteArrayOutputStream
import java.io.File
import kotlin.math.PI
import kotlin.math.abs
import kotlin.math.sin

class CodecTest {
    @Test fun wavRoundTrip() {
        val x = FloatArray(1000) { (0.3 * sin(2 * PI * it / 50)).toFloat() }
        val f = File.createTempFile("voxea", ".wav")
        Wav.write(f, x, 44100)
        val (y, sr) = Wav.read(f)
        assertEquals(44100, sr); assertEquals(x.size, y.size)
        for (i in x.indices) assertTrue(abs(x[i] - y[i]) < 1e-4)
    }

    @Test fun mp3HasFrameSync() {
        val x = FloatArray(44100) { (0.3 * sin(2 * PI * 440 * it / 44100)).toFloat() }
        val bos = ByteArrayOutputStream()
        Mp3.encode(x, 44100, bos)
        val b = bos.toByteArray()
        assertTrue("size=${b.size}", b.size in 8000..40000) // ~16 ko pour 1 s à 128 kb/s
        val idx = (0 until b.size - 1).first { (b[it].toInt() and 0xFF) == 0xFF && (b[it + 1].toInt() and 0xE0) == 0xE0 }
        assertTrue(idx < 2000)
    }

    @Test fun fileNames() {
        assertEquals("04_LEA.mp3", fileNameFor(Line(3, "Léa", "x")))
        assertEquals("12_JEAN_PIERRE.mp3", fileNameFor(Line(11, "Jean-Pierre", "x")))
        assertEquals("01_REPLIQUE.mp3", fileNameFor(Line(0, "!!", "x")))
    }
}
