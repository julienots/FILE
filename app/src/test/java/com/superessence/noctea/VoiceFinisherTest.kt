package com.superessence.noctea

import com.superessence.noctea.audio.Resampler
import com.superessence.noctea.tts.AudioFiles
import com.superessence.noctea.tts.TextComposer
import com.superessence.noctea.tts.VoiceFinisher
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.File
import kotlin.math.PI
import kotlin.math.abs
import kotlin.math.cos
import kotlin.math.log10
import kotlin.math.sin
import kotlin.math.sqrt

/** Voix de l'appareil et export : mêmes niveaux, mêmes silences, aucun défaut de rééchantillonnage. */
class VoiceFinisherTest {

    /** Signal « parole » synthétique : silence variable + bruit modulé + harmoniques, à un niveau donné. */
    private fun speechLike(sr: Int, leadMs: Int, speechMs: Int, tailMs: Int, peak: Float): FloatArray {
        val lead = sr * leadMs / 1000
        val sp = sr * speechMs / 1000
        val tail = sr * tailMs / 1000
        val rnd = java.util.Random(7)
        val out = FloatArray(lead + sp + tail)
        for (i in 0 until sp) {
            val t = i / sr.toDouble()
            val env = 0.55 + 0.45 * sin(2 * PI * 3.1 * t)      // syllabes
            val v = 0.6 * sin(2 * PI * 190 * t) + 0.3 * sin(2 * PI * 380 * t) + 0.2 * sin(2 * PI * 1900 * t) + 0.04 * rnd.nextGaussian()
            out[lead + i] = (peak * env * v).toFloat()
        }
        return out
    }

    private fun tmpWav(sr: Int, samples: FloatArray): File {
        val f = File.createTempFile("noctea_vf", ".wav")
        VoiceFinisher.writeWav(f, VoiceFinisher.Wav(sr, samples))
        return f
    }

    @Test fun fragmentsEndUpWithTheSameLevelAndFixedSilences() {
        val sr = 22050
        val quiet = tmpWav(sr, speechLike(sr, 310, 1400, 250, 0.05f))
        val loud = tmpWav(sr, speechLike(sr, 120, 2200, 600, 0.8f))
        val levels = mutableListOf<Double>()
        val lengths = mutableListOf<Long>()
        for ((f, speechMs) in listOf(quiet to 1400L, loud to 2200L)) {
            assertTrue(VoiceFinisher.finish(f))
            val w = VoiceFinisher.readWav(f)
            assertNotNull(w)
            assertEquals("toujours 24 kHz, comme la bibliothèque", VoiceFinisher.OUTPUT_RATE, w!!.sampleRate)
            levels += VoiceFinisher.activeLevelDb(w.samples, sr)
            lengths += AudioFiles.wavDuration(f) ?: 0L
            // silence d'attaque ≈ 40 ms, de fin ≈ 110 ms : la durée tient à ±60 ms de parole + 150 ms
            assertEquals((speechMs + 150).toDouble(), (AudioFiles.wavDuration(f) ?: 0L).toDouble(), 80.0)
            assertTrue("crête < 0,95", w.samples.maxOf { abs(it) } < 0.95f)
            f.delete()
        }
        assertEquals("même niveau de parole pour un fragment faible et un fragment fort", levels[0], levels[1], 0.7)
        assertEquals("niveau cible", VoiceFinisher.TARGET_DB, levels[0], 0.8)
    }

    @Test fun finishedFileIsMono16BitWav() {
        val sr = 24000
        val f = tmpWav(sr, speechLike(sr, 200, 1000, 300, 0.3f))
        assertTrue(VoiceFinisher.finish(f))
        val head = f.readBytes().copyOfRange(0, 44)
        assertEquals("RIFF", String(head, 0, 4))
        assertEquals("WAVE", String(head, 8, 4))
        assertEquals(1, head[20].toInt())     // PCM
        assertEquals(1, head[22].toInt())     // mono
        assertEquals(16, head[34].toInt())    // 16 bits
        f.delete()
    }

    @Test fun nonWavOrSilentFilesAreLeftUntouched() {
        val mp3 = File.createTempFile("noctea_vf", ".audio").apply { writeBytes(ByteArray(4000) { (it % 251).toByte() }) }
        val before = mp3.readBytes()
        assertFalse(VoiceFinisher.finish(mp3))
        assertTrue(before.contentEquals(mp3.readBytes()))
        mp3.delete()
        val silent = tmpWav(22050, FloatArray(22050))
        val sb = silent.readBytes()
        assertFalse(VoiceFinisher.finish(silent))
        assertTrue(sb.contentEquals(silent.readBytes()))
        silent.delete()
    }

    @Test fun equalizationMatchesTheStudioCurve() {
        val sr = 24000
        fun gainDb(freq: Double): Double {
            val x = FloatArray(sr * 2) { (0.2 * sin(2 * PI * freq * it / sr)).toFloat() }
            val y = VoiceFinisher.equalize(x, sr)
            fun rms(v: FloatArray) = sqrt((sr until sr * 2).sumOf { (v[it] * v[it]).toDouble() } / sr)
            return 20 * log10(rms(y) / rms(x))
        }
        assertTrue("grondement coupé (40 Hz)", gainDb(40.0) < -6.0)
        assertEquals("chaleur de proximité (190 Hz)", 0.7, gainDb(190.0), 0.9)
        assertEquals("médium neutre (1 kHz)", -0.1, gainDb(1000.0), 0.5)
        assertEquals("présence adoucie (3 kHz)", -1.3, gainDb(3000.0), 0.6)
        assertEquals("aigus ramenés à une parole naturelle (6 kHz)", -3.7, gainDb(6000.0), 0.9)
        assertEquals("sifflantes adoucies (10 kHz)", -4.4, gainDb(10000.0), 0.9)
    }

    @Test fun resamplerKeepsLevelAndRejectsImages() {
        val from = 24000
        val to = 48000
        val sine = FloatArray(from) { (0.5 * sin(2 * PI * 1000 * it / from)).toFloat() }
        val up = Resampler.resample(sine, from, to)
        assertEquals(to, up.size)
        fun rms(x: FloatArray, a: Int, b: Int): Double = sqrt((a until b).sumOf { (x[it] * x[it]).toDouble() } / (b - a))
        assertEquals("niveau conservé", 0.5 / sqrt(2.0), rms(up, 4000, 40000), 0.01)
        // énergie à 23 kHz (première image de 1 kHz) : au moins 50 dB sous la fondamentale
        fun goertzel(x: FloatArray, f: Double, sr: Int): Double {
            val w = 2 * PI * f / sr
            var s1 = 0.0; var s2 = 0.0
            val c = 2 * cos(w)
            for (i in 4000 until 40000) { val s = x[i] + c * s1 - s2; s2 = s1; s1 = s }
            return sqrt(s1 * s1 + s2 * s2 - c * s1 * s2) / 36000
        }
        val fund = goertzel(up, 1000.0, to)
        val image = goertzel(up, 23000.0, to)
        assertTrue("image à 23 kHz : ${20 * log10(image / fund)} dB", 20 * log10(image / fund) < -50)
    }

    @Test fun resamplerDownsamplesWithoutAliasing() {
        val from = 48000
        val to = 24000
        val tone = FloatArray(from) { (0.4 * sin(2 * PI * 20000 * it / from)).toFloat() }   // au-dessus de Nyquist cible
        val down = Resampler.resample(tone, from, to)
        val e = sqrt(down.drop(2000).take(16000).sumOf { (it * it).toDouble() } / 16000)
        assertTrue("repliement atténué (RMS $e)", e < 0.01)
    }

    @Test fun textFragmentsStayAlignedWithTheirPauses() {
        val f = TextComposer.split("Bonsoir... ferme les yeux.", 1f)
        assertEquals(2, f.size)
        assertTrue(f.all { it.pauseAfterMs > 0 })
    }
}
