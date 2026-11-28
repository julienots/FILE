package com.superessence.voxea

import com.superessence.voxea.core.VoiceDsp
import com.superessence.voxea.core.VoiceSettings
import com.superessence.voxea.core.VoiceType
import com.superessence.voxea.core.defaultSettings
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import kotlin.math.PI
import kotlin.math.abs
import kotlin.math.sin
import kotlin.math.sqrt

class DspTest {
    private val sr = 44100

    private fun sine(f: Double, sec: Double) = FloatArray((sr * sec).toInt()) { (0.5 * sin(2 * PI * f * it / sr)).toFloat() }

    /** Fréquence dominante par comptage des passages par zéro sur la partie centrale. */
    private fun freq(x: FloatArray): Double {
        val a = x.size / 4; val b = x.size * 3 / 4
        var c = 0
        for (i in a + 1 until b) if ((x[i - 1] < 0f) != (x[i] < 0f)) c++
        return c / 2.0 / ((b - a).toDouble() / sr)
    }

    @Test fun octaveUpDoublesFrequency() {
        val out = VoiceDsp.process(sine(200.0, 1.0), sr, VoiceSettings(pitch = 12f, formant = 1f))
        val f = freq(out)
        assertTrue("freq=$f", abs(f - 400) < 20)
    }

    @Test fun fifthDown() {
        val out = VoiceDsp.process(sine(300.0, 1.0), sr, VoiceSettings(pitch = -7f, formant = 1f))
        val f = freq(out); val expected = 300 * Math.pow(2.0, -7 / 12.0)
        assertTrue("freq=$f exp=$expected", abs(f - expected) < expected * 0.05)
    }

    @Test fun lengthPreserved() {
        val x = sine(220.0, 1.3)
        val out = VoiceDsp.process(x, sr, VoiceType.FEMME.defaultSettings())
        assertEquals(x.size.toDouble(), out.size.toDouble(), x.size * 0.01)
    }

    @Test fun neutralIsNearlyIdentity() {
        val x = sine(180.0, 1.0)
        val out = VoiceDsp.process(x, sr, VoiceSettings(0f, 1f))
        var num = 0.0; var dx = 0.0; var dy = 0.0
        for (i in 4096 until x.size - 4096) { num += x[i] * out[i]; dx += x[i] * x[i]; dy += out[i] * out[i] }
        assertTrue(num / sqrt(dx * dy) > 0.95)
    }

    @Test fun tinyAndEmptyInputsDoNotCrash() {
        VoiceDsp.process(FloatArray(0), sr, VoiceType.HOMME.defaultSettings())
        val out = VoiceDsp.process(sine(200.0, 0.1), sr, VoiceType.ENFANT.defaultSettings())
        assertEquals(4410, out.size)
        VoiceDsp.process(FloatArray(sr), sr, VoiceType.ROBOT.defaultSettings()) // silence
    }

    @Test fun outputIsNormalizedAndFinite() {
        val out = VoiceDsp.process(sine(150.0, 1.0), sr, VoiceType.AGE.defaultSettings())
        assertTrue(out.all { it.isFinite() && abs(it) <= 1f })
    }

    @Test fun presets() {
        assertEquals(VoiceSettings(5f, 1.18f), VoiceType.FEMME.defaultSettings().copy(vibrato = 0f))
        assertTrue(VoiceType.HOMME.defaultSettings().pitch < 0)
        assertTrue(VoiceType.ROBOT.defaultSettings().robot)
    }

    private fun rms(x: FloatArray) = sqrt(x.sumOf { (it * it).toDouble() } / x.size)

    @Test fun loudnessMatchesInput() {
        // signal riche (impulsions) pour éviter un cas trop facile
        val x = FloatArray(sr) { i -> (0.25 * sin(2 * PI * 140 * i / sr) + 0.1 * sin(2 * PI * 280 * i / sr) + if (i % 315 == 0) 0.6 else 0.0).toFloat() }
        for (t in VoiceType.entries) {
            val y = VoiceDsp.process(x, sr, t.defaultSettings())
            val db = 20 * Math.log10(rms(y) / rms(x))
            assertTrue("${t.name} écart ${db} dB", abs(db) < 2.0)
            assertTrue(y.all { abs(it) <= 0.99f })
        }
    }
}
