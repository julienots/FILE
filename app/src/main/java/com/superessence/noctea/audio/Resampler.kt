package com.superessence.noctea.audio

import kotlin.math.PI
import kotlin.math.abs
import kotlin.math.floor
import kotlin.math.max
import kotlin.math.min
import kotlin.math.sin
import kotlin.math.sqrt

/**
 * Rééchantillonnage de qualité (sinc fenêtré de Kaiser, table polyphasée), pour l'export .m4a.
 *
 * L'ancienne interpolation linéaire de 24 kHz vers 48 kHz laissait des images spectrales et
 * assombrissait la voix : le fichier exporté ne sonnait pas comme la lecture dans l'application
 * (qui utilise le rééchantillonneur du système). Ici : atténuation des images > 60 dB, gain plat.
 */
object Resampler {
    private const val HALF = 16            // demi-largeur du noyau en échantillons d'entrée
    private const val PHASES = 256         // résolution de la table de noyaux
    private const val BETA = 8.6           // fenêtre de Kaiser (≈ 80 dB d'atténuation)

    private fun bessel0(x: Double): Double {
        var sum = 1.0
        var term = 1.0
        var k = 1
        while (k < 50) {
            term *= (x / (2 * k)) * (x / (2 * k))
            sum += term
            if (term < 1e-12 * sum) break
            k++
        }
        return sum
    }

    private fun kernelTable(cutoff: Double): Array<FloatArray> {
        val norm = bessel0(BETA)
        return Array(PHASES + 1) { p ->
            val frac = p / PHASES.toDouble()
            val row = DoubleArray(2 * HALF) { i ->
                val x = (i - HALF + 1) - frac            // distance au point d'interpolation
                val sinc = if (abs(x) < 1e-9) cutoff else sin(PI * x * cutoff) / (PI * x)
                val w = x / HALF
                val win = if (abs(w) >= 1.0) 0.0 else bessel0(BETA * sqrt(1 - w * w)) / norm
                sinc * win
            }
            val sum = row.sum()
            FloatArray(2 * HALF) { (row[it] / sum).toFloat() }
        }
    }

    fun resample(src: FloatArray, fromRate: Int, toRate: Int): FloatArray {
        if (fromRate == toRate || src.isEmpty()) return src
        val ratio = fromRate.toDouble() / toRate
        val n = (src.size / ratio).toInt()
        val cutoff = min(1.0, toRate.toDouble() / fromRate) * 0.995   // anti-repliement si on réduit
        val table = kernelTable(cutoff)
        val out = FloatArray(n)
        for (i in 0 until n) {
            val t = i * ratio
            val i0 = floor(t).toInt()
            val pf = (t - i0) * PHASES
            val p0 = pf.toInt()
            val w1 = (pf - p0).toFloat()
            val r0 = table[p0]
            val r1 = table[min(p0 + 1, PHASES)]
            var acc = 0f
            val base = i0 - HALF + 1
            for (k in 0 until 2 * HALF) {
                val idx = base + k
                if (idx < 0 || idx >= src.size) continue
                acc += src[idx] * (r0[k] + (r1[k] - r0[k]) * w1)
            }
            out[i] = acc
        }
        return out
    }

    fun resample(src: ShortArray, fromRate: Int, toRate: Int): FloatArray {
        val f = FloatArray(src.size) { src[it] / 32768f }
        return resample(f, fromRate, toRate)
    }

    @Suppress("unused")
    private fun clampIndex(i: Int, size: Int) = max(0, min(size - 1, i))
}
