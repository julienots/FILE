package com.superessence.noctea.tts

import com.superessence.noctea.audio.Resampler
import java.io.File
import java.io.RandomAccessFile
import java.nio.ByteBuffer
import java.nio.ByteOrder
import kotlin.math.PI
import kotlin.math.abs
import kotlin.math.cos
import kotlin.math.log10
import kotlin.math.max
import kotlin.math.min
import kotlin.math.pow
import kotlin.math.sin
import kotlin.math.sqrt
import kotlin.math.tanh

/**
 * Finition des voix synthétisées sur l'appareil (« Mon texte »), pour qu'elles se tiennent comme la voix
 * de la bibliothèque NOCTÉA :
 *
 *  - silences d'attaque et de fin ramenés à des valeurs fixes (un moteur vocal laisse 0,1 à 0,4 s de
 *    silence variable : le rythme des pauses devenait irrégulier d'un fragment à l'autre) ;
 *  - même égalisation que le studio (passe-haut 70 Hz, chaleur +1,6 dB vers 190 Hz, présence −1 dB
 *    vers 3 kHz, aigus −4 dB au-dessus de 6 kHz et −3 dB vers 7,5 kHz : les voix de synthèse sont plus
 *    brillantes et sifflantes qu'une parole réelle) ;
 *  - même niveau de parole active (−20,5 dBFS) pour TOUS les fragments : fini les fragments plus forts
 *    ou plus faibles que les autres ;
 *  - limiteur à genou doux, fondus d'entrée et de sortie ;
 *  - toujours en 24 kHz mono, comme la bibliothèque : le lecteur n'a plus à reconfigurer sa piste audio
 *    entre un fragment et un silence.
 *
 * Elle ne change ni le timbre ni la hauteur de la voix du moteur : pour obtenir exactement la voix de la
 * bibliothèque dans les textes libres, il faut un serveur vocal NOCTÉA (tools/noctea_voice_server.py).
 */
object VoiceFinisher {
    const val TARGET_DB = -20.5

    /**
     * Fréquence de sortie : celle de la voix et des silences de la bibliothèque (Opus 24 kHz mono).
     * Un fragment à 22,05 ou 16 kHz entre deux silences à 24 kHz obligeait le lecteur à recréer sa piste
     * audio à chaque fragment (micro-coupures, claquements selon l'appareil).
     */
    const val OUTPUT_RATE = 24_000
    private const val LEAD_MS = 40
    private const val TAIL_MS = 110
    private const val GATE_DB = -45.0
    private const val LIMIT_THRESHOLD = 0.72

    class Wav(val sampleRate: Int, val samples: FloatArray)

    /** Traite le fichier sur place. Retourne false (fichier intact) si ce n'est pas un WAV PCM 16 bits exploitable. */
    fun finish(file: File): Boolean {
        val wav = readWav(file) ?: return false
        if (wav.samples.size < wav.sampleRate / 10) return false
        val samples = if (wav.sampleRate == OUTPUT_RATE) wav.samples
        else Resampler.resample(wav.samples, wav.sampleRate, OUTPUT_RATE)
        val out = process(samples, OUTPUT_RATE) ?: return false
        writeWav(file, Wav(OUTPUT_RATE, out))
        return true
    }

    fun process(input: FloatArray, sampleRate: Int): FloatArray? {
        var x = trim(input, sampleRate) ?: return null
        x = equalize(x, sampleRate)
        x = level(x, sampleRate)
        fades(x, sampleRate)
        return x
    }

    // ------------------------------------------------------------------ silences
    private fun frameDb(x: FloatArray, win: Int): DoubleArray {
        val n = x.size / win
        return DoubleArray(n) { f ->
            var acc = 0.0
            for (i in f * win until (f + 1) * win) acc += x[i].toDouble() * x[i]
            10 * log10(acc / win + 1e-12)
        }
    }

    private fun trim(x: FloatArray, sr: Int): FloatArray? {
        val win = max(1, sr / 100)
        val db = frameDb(x, win)
        if (db.isEmpty()) return null
        val ref = db.sortedArray()[(db.size * 0.9).toInt().coerceAtMost(db.size - 1)]
        val thr = max(ref + GATE_DB, -70.0)
        val first = db.indexOfFirst { it > thr }
        val last = db.indexOfLast { it > thr }
        if (first < 0 || ref < -60.0) return null
        val start = max(0, first * win - LEAD_MS * sr / 1000)
        val end = min(x.size, (last + 1) * win + TAIL_MS * sr / 1000)
        return x.copyOfRange(start, end)
    }

    // ------------------------------------------------------------------ égalisation (biquads RBJ)
    private class Biquad(val b0: Double, val b1: Double, val b2: Double, val a1: Double, val a2: Double) {
        private var z1 = 0.0
        private var z2 = 0.0
        fun run(x: FloatArray) {
            for (i in x.indices) {
                val v = x[i].toDouble()
                val y = b0 * v + z1
                z1 = b1 * v - a1 * y + z2
                z2 = b2 * v - a2 * y
                x[i] = y.toFloat()
            }
        }
    }

    private fun coeffs(kind: String, f0: Double, gainDb: Double, q: Double, sr: Int): Biquad {
        val a = 10.0.pow(gainDb / 40)
        val w0 = 2 * PI * f0 / sr
        val cw = cos(w0)
        val sw = sin(w0)
        val alpha = sw / (2 * q)
        val b: DoubleArray
        val d: DoubleArray
        when (kind) {
            "highpass" -> {
                b = doubleArrayOf((1 + cw) / 2, -(1 + cw), (1 + cw) / 2)
                d = doubleArrayOf(1 + alpha, -2 * cw, 1 - alpha)
            }
            "peak" -> {
                b = doubleArrayOf(1 + alpha * a, -2 * cw, 1 - alpha * a)
                d = doubleArrayOf(1 + alpha / a, -2 * cw, 1 - alpha / a)
            }
            "lowshelf" -> {
                val sq = 2 * sqrt(a) * alpha
                b = doubleArrayOf(a * ((a + 1) - (a - 1) * cw + sq), 2 * a * ((a - 1) - (a + 1) * cw), a * ((a + 1) - (a - 1) * cw - sq))
                d = doubleArrayOf((a + 1) + (a - 1) * cw + sq, -2 * ((a - 1) + (a + 1) * cw), (a + 1) + (a - 1) * cw - sq)
            }
            else -> { // highshelf
                val sq = 2 * sqrt(a) * alpha
                b = doubleArrayOf(a * ((a + 1) + (a - 1) * cw + sq), -2 * a * ((a - 1) + (a + 1) * cw), a * ((a + 1) + (a - 1) * cw - sq))
                d = doubleArrayOf((a + 1) - (a - 1) * cw + sq, 2 * ((a - 1) - (a + 1) * cw), (a + 1) - (a - 1) * cw - sq)
            }
        }
        return Biquad(b[0] / d[0], b[1] / d[0], b[2] / d[0], d[1] / d[0], d[2] / d[0])
    }

    internal fun equalize(x: FloatArray, sr: Int): FloatArray {
        val y = x.copyOf()
        val nyq = sr / 2.0
        coeffs("highpass", 70.0, 0.0, 0.7071, sr).run(y)
        coeffs("lowshelf", 190.0, 1.6, 0.7, sr).run(y)
        if (3000.0 < nyq * 0.8) coeffs("peak", 3000.0, -1.0, 1.0, sr).run(y)
        if (6000.0 < nyq * 0.92) coeffs("highshelf", 6000.0, -4.0, 0.7, sr).run(y)
        if (7500.0 < nyq * 0.8) coeffs("peak", 7500.0, -3.0, 1.2, sr).run(y)
        return y
    }

    // ------------------------------------------------------------------ niveau
    /** Niveau RMS (dBFS) des trames de 20 ms actives (à moins de 45 dB de la plus forte). */
    fun activeLevelDb(x: FloatArray, sr: Int): Double {
        val win = max(1, sr / 50)
        val db = frameDb(x, win)
        if (db.isEmpty()) return -90.0
        val gate = db.max() + GATE_DB
        val act = db.filter { it > gate && it > -60.0 }
        if (act.isEmpty()) return -90.0
        // moyenne en énergie
        val e = act.map { 10.0.pow(it / 10) }.average()
        return 10 * log10(e + 1e-12)
    }

    private fun level(x: FloatArray, sr: Int): FloatArray {
        val cur = activeLevelDb(x, sr)
        val gain = 10.0.pow((TARGET_DB - cur).coerceIn(-24.0, 24.0) / 20).toFloat()
        val k = 1.0 - LIMIT_THRESHOLD
        return FloatArray(x.size) { i ->
            val v = x[i] * gain
            val av = abs(v)
            if (av <= LIMIT_THRESHOLD) v
            else (kotlin.math.sign(v) * (LIMIT_THRESHOLD + k * tanh((av - LIMIT_THRESHOLD) / k))).toFloat()
        }
    }

    private fun fades(x: FloatArray, sr: Int) {
        val nIn = min(x.size / 4, 6 * sr / 1000)
        for (i in 0 until nIn) {
            val g = sin(PI / 2 * i / nIn)
            x[i] = (x[i] * g * g).toFloat()
        }
        val nOut = min(x.size / 2, 60 * sr / 1000)
        for (i in 0 until nOut) {
            val g = cos(PI / 2 * i / nOut)
            x[x.size - nOut + i] = (x[x.size - nOut + i] * g * g).toFloat()
        }
    }

    // ------------------------------------------------------------------ WAV
    fun readWav(file: File): Wav? = try {
        RandomAccessFile(file, "r").use { f ->
            if (f.length() < 44) return null
            val head = ByteArray(12)
            f.readFully(head)
            if (String(head, 0, 4) != "RIFF" || String(head, 8, 4) != "WAVE") return null
            var channels = 0
            var sr = 0
            var bits = 0
            var format = 0
            while (f.filePointer + 8 <= f.length()) {
                val idb = ByteArray(4)
                f.readFully(idb)
                val size = Integer.reverseBytes(f.readInt()).toLong() and 0xffffffffL
                val id = String(idb)
                if (id == "fmt ") {
                    val fmt = ByteArray(size.toInt())
                    f.readFully(fmt)
                    val bb = ByteBuffer.wrap(fmt).order(ByteOrder.LITTLE_ENDIAN)
                    format = bb.getShort(0).toInt() and 0xffff
                    channels = bb.getShort(2).toInt()
                    sr = bb.getInt(4)
                    bits = bb.getShort(14).toInt()
                    if (size and 1L == 1L) f.skipBytes(1)
                } else if (id == "data") {
                    if ((format != 1 && format != 0xFFFE) || bits != 16 || channels !in 1..2 || sr < 8000) return null
                    val remaining = f.length() - f.filePointer
                    val real = if (size == 0L || size > remaining) remaining else size
                    val bytes = ByteArray(real.toInt() / (2 * channels) * 2 * channels)
                    f.readFully(bytes)
                    val sb = ByteBuffer.wrap(bytes).order(ByteOrder.LITTLE_ENDIAN).asShortBuffer()
                    val frames = bytes.size / (2 * channels)
                    val out = FloatArray(frames)
                    for (i in 0 until frames) {
                        var acc = 0
                        for (c in 0 until channels) acc += sb.get()
                        out[i] = acc / channels / 32768f
                    }
                    return Wav(sr, out)
                } else {
                    f.seek(f.filePointer + size + (size and 1))
                }
            }
            null
        }
    } catch (e: Exception) {
        null
    }

    fun writeWav(file: File, wav: Wav) {
        val n = wav.samples.size
        val data = ByteBuffer.allocate(n * 2).order(ByteOrder.LITTLE_ENDIAN)
        for (v in wav.samples) data.putShort((v.coerceIn(-1f, 1f) * 32767f).toInt().toShort())
        val header = ByteBuffer.allocate(44).order(ByteOrder.LITTLE_ENDIAN).apply {
            put("RIFF".toByteArray()); putInt(36 + n * 2); put("WAVE".toByteArray())
            put("fmt ".toByteArray()); putInt(16); putShort(1); putShort(1)
            putInt(wav.sampleRate); putInt(wav.sampleRate * 2); putShort(2); putShort(16)
            put("data".toByteArray()); putInt(n * 2)
        }.array()
        file.outputStream().use { it.write(header); it.write(data.array()) }
    }
}
