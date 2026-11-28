package com.superessence.voxea.core

import kotlinx.serialization.Serializable
import kotlin.math.PI
import kotlin.math.abs
import kotlin.math.atan2
import kotlin.math.cos
import kotlin.math.exp
import kotlin.math.ln
import kotlin.math.max
import kotlin.math.min
import kotlin.math.pow
import kotlin.math.roundToInt
import kotlin.math.sin
import kotlin.math.sqrt

/**
 * Réglages d'une voix.
 * @param pitch décalage de hauteur en demi-tons (+ = plus aigu).
 * @param formant facteur appliqué au timbre (enveloppe spectrale) : > 1 = voix plus « petite », < 1 = plus « ample ».
 * @param robot timbre robotique (phase figée).
 * @param vibrato amplitude d'un léger vibrato, en demi-tons (voix âgée).
 */
@Serializable
data class VoiceSettings(
    val pitch: Float,
    val formant: Float,
    val robot: Boolean = false,
    val vibrato: Float = 0f,
)

fun VoiceType.defaultSettings(): VoiceSettings = when (this) {
    VoiceType.ORIGINAL -> VoiceSettings(0f, 1f)
    VoiceType.FEMME -> VoiceSettings(5f, 1.18f)
    VoiceType.HOMME -> VoiceSettings(-5f, 0.86f)
    VoiceType.ENFANT -> VoiceSettings(8f, 1.30f)
    VoiceType.AGE -> VoiceSettings(-2f, 0.94f, vibrato = 0.35f)
    VoiceType.ROBOT -> VoiceSettings(0f, 1f, robot = true)
}

/**
 * Transformation de voix hors ligne : vocodeur de phase (STFT) avec séparation source / filtre.
 * L'enveloppe spectrale (formants) est estimée par lissage cepstral, ce qui permet de décaler
 * la hauteur et le timbre indépendamment (évite l'effet « chipmunk »). Phase verrouillée aux pics
 * (Laroche–Dolson) pour limiter la « phasiness ».
 */
object VoiceDsp {
    private const val N = 2048
    private const val HOP = N / 8
    private const val HALF = N / 2

    fun process(input: FloatArray, sampleRate: Int, s: VoiceSettings, onProgress: (Float) -> Unit = {}): FloatArray {
        if (input.isEmpty()) return input
        val neutral = s.pitch == 0f && s.formant == 1f && !s.robot && s.vibrato == 0f
        if (neutral) return input.copyOf()

        val len = input.size
        val padded = DoubleArray(len + 2 * N)
        for (i in 0 until len) padded[i + N] = input[i].toDouble()
        val out = DoubleArray(padded.size + N)

        val window = DoubleArray(N) { 0.5 - 0.5 * cos(2 * PI * it / N) }
        val lifter = max(8, (sampleRate * 0.0009).roundToInt()) // ~0,9 ms : sous la période fondamentale
        val expct = 2 * PI * HOP / N
        val osamp = N / HOP

        val re = DoubleArray(N); val im = DoubleArray(N)
        val cre = DoubleArray(N); val cim = DoubleArray(N)
        val mag = DoubleArray(HALF + 1); val phaseIn = DoubleArray(HALF + 1)
        val lastPhase = DoubleArray(HALF + 1); val sumPhase = DoubleArray(HALF + 1)
        val trueFreq = DoubleArray(HALF + 1); val env = DoubleArray(HALF + 1)
        val outMag = DoubleArray(HALF + 1); val outFreq = DoubleArray(HALF + 1)
        val srcBin = IntArray(HALF + 1); val outPhase = DoubleArray(HALF + 1)
        val isPeak = BooleanArray(HALF + 1)
        val formant = s.formant.toDouble().coerceIn(0.5, 2.0)

        val frames = (padded.size - N) / HOP + 1
        var frame = 0
        var pos = 0
        while (pos + N <= padded.size) {
            val t = pos.toDouble() / sampleRate
            val semis = s.pitch + s.vibrato * sin(2 * PI * 5.5 * t)
            val r = 2.0.pow(semis / 12.0)

            // --- Analyse ---
            for (i in 0 until N) { re[i] = padded[pos + i] * window[i]; im[i] = 0.0 }
            fft(re, im, false)
            for (k in 0..HALF) {
                val m = sqrt(re[k] * re[k] + im[k] * im[k])
                val p = atan2(im[k], re[k])
                mag[k] = m; phaseIn[k] = p
                var d = p - lastPhase[k] - k * expct
                lastPhase[k] = p
                d -= 2 * PI * Math.round(d / (2 * PI))
                trueFreq[k] = k + d * osamp / (2 * PI)
            }

            // --- Enveloppe spectrale par cepstre ---
            for (k in 0 until N) {
                val kk = if (k <= HALF) k else N - k
                cre[k] = ln(mag[kk] + 1e-9); cim[k] = 0.0
            }
            fft(cre, cim, true)
            for (k in lifter until N - lifter + 1) { cre[k] = 0.0; cim[k] = 0.0 }
            fft(cre, cim, false)
            for (k in 0..HALF) env[k] = exp(cre[k])

            // --- Décalage des bins de l'excitation ---
            java.util.Arrays.fill(outMag, 0.0); java.util.Arrays.fill(outFreq, 0.0); java.util.Arrays.fill(srcBin, -1)
            for (k in 0..HALF) {
                val j = (k * r).roundToInt()
                if (j > HALF) break
                val e = mag[k] / env[k]
                if (srcBin[j] < 0 || e > outMag[j]) { srcBin[j] = k; outFreq[j] = trueFreq[k] * r }
                outMag[j] += e
            }
            for (j in 0..HALF) {
                if (srcBin[j] < 0) {
                    // Trou (montée de hauteur) : on interpole l'excitation au bin source fractionnaire j / r
                    val kf = (j / r).coerceIn(0.0, HALF.toDouble())
                    val k0 = kf.toInt(); val k1 = min(k0 + 1, HALF); val fr = kf - k0
                    srcBin[j] = if (fr < 0.5) k0 else k1
                    outMag[j] = (mag[k0] / env[k0]) * (1 - fr) + (mag[k1] / env[k1]) * fr
                    outFreq[j] = trueFreq[srcBin[j]] * r
                }
                // Enveloppe déplacée selon le facteur de formant
                val pos2 = j / formant
                val i0 = pos2.toInt()
                val envOut = if (i0 >= HALF) env[HALF] * 1e-3 else env[i0] + (env[min(i0 + 1, HALF)] - env[i0]) * (pos2 - i0)
                outMag[j] *= envOut
                sumPhase[j] += 2 * PI * outFreq[j] * HOP / N
            }

            // --- Verrouillage de phase autour des pics ---
            if (s.robot) {
                java.util.Arrays.fill(outPhase, 0.0)
            } else {
                for (j in 0..HALF) {
                    val m = outMag[j]
                    isPeak[j] = m > 0 && (j < 2 || m >= outMag[j - 1] && m >= outMag[j - 2]) &&
                        (j > HALF - 2 || m > outMag[j + 1] && m > outMag[j + 2])
                }
                var lastPeak = -1
                var j = 0
                while (j <= HALF) {
                    // prochain pic
                    var next = j
                    while (next <= HALF && !isPeak[next]) next++
                    for (q in j until min(next, HALF + 1)) {
                        val p = when {
                            lastPeak < 0 && next > HALF -> -1
                            lastPeak < 0 -> next
                            next > HALF -> lastPeak
                            q - lastPeak <= next - q -> lastPeak
                            else -> next
                        }
                        outPhase[q] = if (p < 0) sumPhase[q] else sumPhase[p] + phaseIn[srcBin[q]] - phaseIn[srcBin[p]]
                    }
                    if (next <= HALF) { outPhase[next] = sumPhase[next]; lastPeak = next }
                    j = next + 1
                }
            }

            // --- Synthèse ---
            for (k in 0..HALF) {
                re[k] = outMag[k] * cos(outPhase[k]); im[k] = outMag[k] * sin(outPhase[k])
            }
            im[0] = 0.0; im[HALF] = 0.0
            for (k in HALF + 1 until N) { re[k] = re[N - k]; im[k] = -im[N - k] }
            fft(re, im, true)
            for (i in 0 until N) out[pos + i] += re[i] * window[i]

            pos += HOP
            frame++
            if (frame % 64 == 0) onProgress(frame.toFloat() / frames)
        }

        // Même intensité (RMS) que l'entrée, puis limiteur doux pour éviter toute saturation.
        val result = FloatArray(len)
        var eOut = 0.0; var eIn = 0.0
        for (i in 0 until len) { val v = out[i + N]; eOut += v * v; eIn += input[i].toDouble() * input[i] }
        val g = if (eOut > 1e-12) sqrt(eIn / eOut) else 0.0
        for (i in 0 until len) result[i] = softLimit(out[i + N] * g).toFloat()
        onProgress(1f)
        return result
    }

    /** Linéaire jusqu'à 0,8 puis compression douce vers 0,98. */
    fun softLimit(v: Double): Double {
        val a = abs(v)
        if (a <= 0.8) return v
        val c = 0.8 + 0.18 * kotlin.math.tanh((a - 0.8) / 0.18)
        return if (v < 0) -c else c
    }

    /** FFT radix-2 en place. [inverse] = true applique aussi la division par N. */
    fun fft(re: DoubleArray, im: DoubleArray, inverse: Boolean) {
        val n = re.size
        var j = 0
        for (i in 1 until n) {
            var bit = n shr 1
            while (j and bit != 0) { j = j xor bit; bit = bit shr 1 }
            j = j xor bit
            if (i < j) {
                var tmp = re[i]; re[i] = re[j]; re[j] = tmp
                tmp = im[i]; im[i] = im[j]; im[j] = tmp
            }
        }
        var size = 2
        while (size <= n) {
            val ang = 2 * PI / size * (if (inverse) 1 else -1)
            val wr = cos(ang); val wi = sin(ang)
            var start = 0
            while (start < n) {
                var cr = 1.0; var ci = 0.0
                for (k in 0 until size / 2) {
                    val a = start + k; val b = a + size / 2
                    val xr = re[b] * cr - im[b] * ci
                    val xi = re[b] * ci + im[b] * cr
                    re[b] = re[a] - xr; im[b] = im[a] - xi
                    re[a] += xr; im[a] += xi
                    val ncr = cr * wr - ci * wi
                    ci = cr * wi + ci * wr; cr = ncr
                }
                start += size
            }
            size = size shl 1
        }
        if (inverse) for (i in 0 until n) { re[i] /= n; im[i] /= n }
    }
}
