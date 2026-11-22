package com.superessence.pulsar.engine

import kotlin.math.PI
import kotlin.math.abs
import kotlin.math.exp
import kotlin.math.pow
import kotlin.math.sin
import kotlin.math.sqrt
import kotlin.math.tan

const val SR = 44100
const val SRF = 44100f
const val TWO_PI_F = (2.0 * PI).toFloat()

fun mtof(note: Float): Float = 440f * 2f.pow((note - 69f) / 12f)

/** Saturation douce (approximation rationnelle de tanh, bornée). */
fun softClip(x: Float): Float {
    if (x > 3f) return 1f
    if (x < -3f) return -1f
    val x2 = x * x
    return x * (27f + x2) / (27f + 9f * x2)
}

fun hardClip(x: Float, t: Float = 1f): Float = if (x > t) t else if (x < -t) -t else x

fun dbToGain(db: Float): Float = 10f.pow(db / 20f)

/** Générateur pseudo-aléatoire déterministe (xorshift64*), rapide et sans allocation. */
class Rng(seed: Long) {
    private var s: Long = if (seed == 0L) 0x9E3779B97F4A7C15uL.toLong() else seed xor 0x5DEECE66DL

    init { repeat(4) { nextLong() } }

    fun nextLong(): Long {
        var x = s
        x = x xor (x ushr 12)
        x = x xor (x shl 25)
        x = x xor (x ushr 27)
        s = x
        return x * 2685821657736338717L
    }

    fun nextFloat(): Float = ((nextLong() ushr 40).toFloat() / (1L shl 24).toFloat())
    fun bipolar(): Float = nextFloat() * 2f - 1f
    fun nextInt(n: Int): Int = if (n <= 1) 0 else ((nextLong() ushr 33) % n).toInt()
    fun range(a: Int, b: Int): Int = a + nextInt(b - a + 1)
    fun range(a: Float, b: Float): Float = a + (b - a) * nextFloat()
    fun chance(p: Float): Boolean = nextFloat() < p
    fun <T> pick(list: List<T>): T = list[nextInt(list.size)]
    fun <T> pick(arr: Array<T>): T = arr[nextInt(arr.size)]
    fun pick(arr: IntArray): Int = arr[nextInt(arr.size)]
}

/** Bruit blanc ultra léger pour l'audio temps réel. */
class Noise(seed: Int = 22222) {
    private var x = seed
    fun next(): Float {
        x = x * 1664525 + 1013904223
        return (x shr 8).toFloat() / 8388608f
    }
}

/** Filtre biquad RBJ (forme directe transposée II). */
class Biquad {
    private var b0 = 1f; private var b1 = 0f; private var b2 = 0f; private var a1 = 0f; private var a2 = 0f
    private var z1 = 0f; private var z2 = 0f

    fun lowpass(freq: Float, q: Float = 0.707f) = set(0, freq, q, 0f)
    fun highpass(freq: Float, q: Float = 0.707f) = set(1, freq, q, 0f)
    fun bandpass(freq: Float, q: Float = 1f) = set(2, freq, q, 0f)
    fun peak(freq: Float, q: Float, gainDb: Float) = set(3, freq, q, gainDb)
    fun lowShelf(freq: Float, gainDb: Float) = set(4, freq, 0.707f, gainDb)
    fun highShelf(freq: Float, gainDb: Float) = set(5, freq, 0.707f, gainDb)

    fun set(type: Int, freq: Float, q: Float, gainDb: Float) {
        val f = freq.coerceIn(10f, SRF * 0.49f)
        val w0 = TWO_PI_F * f / SRF
        val cosw = kotlin.math.cos(w0)
        val sinw = sin(w0)
        val alpha = sinw / (2f * q.coerceAtLeast(0.05f))
        val a = 10f.pow(gainDb / 40f)
        var nb0: Float; var nb1: Float; var nb2: Float; var na0: Float; var na1: Float; var na2: Float
        when (type) {
            0 -> { nb0 = (1 - cosw) / 2; nb1 = 1 - cosw; nb2 = (1 - cosw) / 2; na0 = 1 + alpha; na1 = -2 * cosw; na2 = 1 - alpha }
            1 -> { nb0 = (1 + cosw) / 2; nb1 = -(1 + cosw); nb2 = (1 + cosw) / 2; na0 = 1 + alpha; na1 = -2 * cosw; na2 = 1 - alpha }
            2 -> { nb0 = alpha; nb1 = 0f; nb2 = -alpha; na0 = 1 + alpha; na1 = -2 * cosw; na2 = 1 - alpha }
            3 -> { nb0 = 1 + alpha * a; nb1 = -2 * cosw; nb2 = 1 - alpha * a; na0 = 1 + alpha / a; na1 = -2 * cosw; na2 = 1 - alpha / a }
            4 -> {
                val sq = 2 * sqrt(a) * alpha
                nb0 = a * ((a + 1) - (a - 1) * cosw + sq); nb1 = 2 * a * ((a - 1) - (a + 1) * cosw); nb2 = a * ((a + 1) - (a - 1) * cosw - sq)
                na0 = (a + 1) + (a - 1) * cosw + sq; na1 = -2 * ((a - 1) + (a + 1) * cosw); na2 = (a + 1) + (a - 1) * cosw - sq
            }
            else -> {
                val sq = 2 * sqrt(a) * alpha
                nb0 = a * ((a + 1) + (a - 1) * cosw + sq); nb1 = -2 * a * ((a - 1) + (a + 1) * cosw); nb2 = a * ((a + 1) + (a - 1) * cosw - sq)
                na0 = (a + 1) - (a - 1) * cosw + sq; na1 = 2 * ((a - 1) - (a + 1) * cosw); na2 = (a + 1) - (a - 1) * cosw - sq
            }
        }
        b0 = nb0 / na0; b1 = nb1 / na0; b2 = nb2 / na0; a1 = na1 / na0; a2 = na2 / na0
    }

    fun process(x: Float): Float {
        val y = b0 * x + z1
        z1 = b1 * x - a1 * y + z2
        z2 = b2 * x - a2 * y
        return y
    }

    fun reset() { z1 = 0f; z2 = 0f }
}

/**
 * Filtre à variables d'état « TPT » (Zavalishin) : stable même modulé très vite,
 * résonance musicale. Sorties passe-bas / passe-haut / passe-bande.
 */
class Svf {
    private var g = 0.1f; private var k = 1.4f; private var a1 = 0f; private var a2 = 0f; private var a3 = 0f
    private var ic1 = 0f; private var ic2 = 0f
    var low = 0f; private set
    var band = 0f; private set
    var high = 0f; private set

    fun set(cutoff: Float, resonance: Float) {
        val fc = cutoff.coerceIn(20f, SRF * 0.45f)
        g = tan(PI.toFloat() * fc / SRF)
        k = 2f - 1.96f * resonance.coerceIn(0f, 1f)
        a1 = 1f / (1f + g * (g + k)); a2 = g * a1; a3 = g * a2
    }

    fun process(x: Float): Float {
        val v3 = x - ic2
        val v1 = a1 * ic1 + a2 * v3
        val v2 = ic2 + a2 * ic1 + a3 * v3
        ic1 = 2f * v1 - ic1
        ic2 = 2f * v2 - ic2
        low = v2; band = v1; high = x - k * v1 - v2
        return v2
    }

    fun reset() { ic1 = 0f; ic2 = 0f }
}

class OnePole {
    private var a = 0.5f; private var z = 0f
    fun set(freq: Float) { a = exp(-TWO_PI_F * freq.coerceIn(5f, SRF * 0.49f) / SRF) }
    fun lp(x: Float): Float { z = x * (1f - a) + z * a; return z }
    fun hp(x: Float): Float = x - lp(x)
    fun reset() { z = 0f }
}

/** Enveloppe ADSR exponentielle (temps en secondes). */
class Adsr {
    var attack = 0.005f; var decay = 0.2f; var sustain = 0.7f; var release = 0.3f
    var value = 0f; private set
    private var stage = 0 // 0 off, 1 a, 2 d, 3 s, 4 r
    private var attInc = 0f; private var decCoef = 0f; private var relCoef = 0f

    fun set(a: Float, d: Float, s: Float, r: Float) {
        attack = a; decay = d; sustain = s; release = r
        attInc = 1f / (a.coerceAtLeast(0.0005f) * SRF)
        decCoef = exp(-4.6f / (d.coerceAtLeast(0.001f) * SRF))
        relCoef = exp(-4.6f / (r.coerceAtLeast(0.001f) * SRF))
    }

    fun gate() { stage = 1 }
    fun noteOff() { if (stage != 0) stage = 4 }
    fun hardReset() { stage = 0; value = 0f }
    val isActive get() = stage != 0

    fun next(): Float {
        when (stage) {
            1 -> { value += attInc; if (value >= 1f) { value = 1f; stage = 2 } }
            2 -> { value = sustain + (value - sustain) * decCoef; if (value - sustain < 0.0005f) { value = sustain; stage = 3 } }
            3 -> value = sustain
            4 -> { value *= relCoef; if (value < 0.0002f) { value = 0f; stage = 0 } }
        }
        return value
    }
}

/** Oscillateur à table de phase avec saw/square anti-repliement (PolyBLEP). */
object Osc {
    fun polyBlep(t: Float, dt: Float): Float {
        if (t < dt) { val x = t / dt; return x + x - x * x - 1f }
        if (t > 1f - dt) { val x = (t - 1f) / dt; return x * x + x + x + 1f }
        return 0f
    }
    fun saw(phase: Float, dt: Float): Float = 2f * phase - 1f - polyBlep(phase, dt)
    fun square(phase: Float, dt: Float, pw: Float = 0.5f): Float {
        var v = if (phase < pw) 1f else -1f
        v += polyBlep(phase, dt)
        var p2 = phase + 1f - pw; if (p2 >= 1f) p2 -= 1f
        v -= polyBlep(p2, dt)
        return v
    }
    fun tri(phase: Float): Float = if (phase < 0.5f) 4f * phase - 1f else 3f - 4f * phase
    fun sine(phase: Float): Float = fastSin(phase)

    private const val TBL = 4096
    private val sinTable = FloatArray(TBL + 1) { sin(2.0 * PI * it / TBL).toFloat() }

    /** sin(2π·phase) par table interpolée, phase quelconque. */
    fun fastSin(phase: Float): Float {
        var p = phase - phase.toInt().toFloat()
        if (p < 0f) p += 1f
        val f = p * TBL
        var i = f.toInt()
        if (i >= TBL) i = TBL - 1
        val fr = f - i
        return sinTable[i] + (sinTable[i + 1] - sinTable[i]) * fr
    }
}

/** Délai stéréo ping-pong synchronisé, filtré dans la boucle. */
class StereoDelay(maxSeconds: Float = 2f) {
    private val size = (maxSeconds * SRF).toInt()
    private val bl = FloatArray(size); private val br = FloatArray(size)
    private var w = 0
    var timeSamples = (0.375f * SRF).toInt()
    var feedback = 0.35f
    private val lpL = OnePole().apply { set(3500f) }; private val lpR = OnePole().apply { set(3500f) }
    private val hpL = OnePole().apply { set(250f) }; private val hpR = OnePole().apply { set(250f) }

    fun setTone(lp: Float) { lpL.set(lp); lpR.set(lp) }

    /** Écrit l'entrée (déjà envoyée) et ajoute la sortie mouillée dans outL/outR. */
    fun process(inL: Float, inR: Float, out: FloatArray) {
        var r = w - timeSamples.coerceIn(1, size - 1); if (r < 0) r += size
        val dl = bl[r]; val dr = br[r]
        // ping-pong : l'entrée mono va à gauche, la gauche nourrit la droite
        val fbL = hpL.hp(lpL.lp(dr * feedback))
        val fbR = hpR.hp(lpR.lp(dl * feedback))
        bl[w] = (inL + inR) * 0.5f + fbL
        br[w] = fbR
        w++; if (w >= size) w = 0
        out[0] = dl; out[1] = dr
    }

    fun clear() { bl.fill(0f); br.fill(0f) }
}

/** Réverbération type Freeverb (8 filtres en peigne + 4 passe-tout par canal). */
class Reverb {
    private class Comb(len: Int) {
        val buf = FloatArray(len); var idx = 0; var store = 0f
        fun process(x: Float, fb: Float, damp: Float): Float {
            val out = buf[idx]
            store = out * (1f - damp) + store * damp
            buf[idx] = x + store * fb
            idx++; if (idx >= buf.size) idx = 0
            return out
        }
    }
    private class AllPass(len: Int) {
        val buf = FloatArray(len); var idx = 0
        fun process(x: Float): Float {
            val b = buf[idx]
            val out = -x + b
            buf[idx] = x + b * 0.5f
            idx++; if (idx >= buf.size) idx = 0
            return out
        }
    }

    private val combTun = intArrayOf(1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617)
    private val apTun = intArrayOf(556, 441, 341, 225)
    private val spread = 23
    private val combL = combTun.map { Comb(it) }
    private val combR = combTun.map { Comb(it + spread) }
    private val apL = apTun.map { AllPass(it) }
    private val apR = apTun.map { AllPass(it + spread) }
    private val pre = FloatArray(4410); private var preIdx = 0
    private val hp = OnePole().apply { set(180f) }

    var roomSize = 0.84f
    var damping = 0.35f
    var width = 1f
    var preDelay = 600

    fun setSize(size01: Float) { roomSize = 0.7f + 0.28f * size01.coerceIn(0f, 1f) }

    fun process(inL: Float, inR: Float, out: FloatArray) {
        val input = hp.hp((inL + inR) * 0.015f)
        pre[preIdx] = input
        var r = preIdx - preDelay.coerceIn(0, pre.size - 1); if (r < 0) r += pre.size
        val x = pre[r]
        preIdx++; if (preIdx >= pre.size) preIdx = 0
        var l = 0f; var rr = 0f
        for (i in 0 until 8) { l += combL[i].process(x, roomSize, damping); rr += combR[i].process(x, roomSize, damping) }
        for (i in 0 until 4) { l = apL[i].process(l); rr = apR[i].process(rr) }
        val w1 = 0.5f * (1f + width); val w2 = 0.5f * (1f - width)
        out[0] = l * w1 + rr * w2
        out[1] = rr * w1 + l * w2
    }
}

/** Compresseur / limiteur crête simple, stéréo lié. */
class Limiter(var ceiling: Float = 0.95f) {
    private var env = 0f
    private val att = exp(-1f / (0.0008f * SRF))
    private val rel = exp(-1f / (0.12f * SRF))
    var gainReduction = 1f; private set

    fun process(l: Float, r: Float, out: FloatArray) {
        val peak = maxOf(abs(l), abs(r))
        env = if (peak > env) att * env + (1 - att) * peak else rel * env + (1 - rel) * peak
        val g = if (env > ceiling) ceiling / env else 1f
        gainReduction = g
        out[0] = hardClip(l * g, ceiling)
        out[1] = hardClip(r * g, ceiling)
    }
}

/** Instabilité de bande (wow & flutter) : délai modulé très court. */
class TapeWow {
    private val buf = FloatArray(2048); private var w = 0
    private var ph1 = 0f; private var ph2 = 0f
    var depth = 0f

    fun process(x: Float): Float {
        buf[w] = x
        ph1 += 0.55f / SRF; if (ph1 > 1f) ph1 -= 1f
        ph2 += 6.1f / SRF; if (ph2 > 1f) ph2 -= 1f
        val mod = (Osc.fastSin(ph1) * 0.8f + Osc.fastSin(ph2) * 0.2f + 1f) * 0.5f
        val d = 40f + mod * depth * 260f
        var rp = w - d; if (rp < 0) rp += buf.size
        val i = rp.toInt(); val fr = rp - i
        val a = buf[i % buf.size]; val b = buf[(i + 1) % buf.size]
        w++; if (w >= buf.size) w = 0
        return a + (b - a) * fr
    }
}

/** Craquements de vinyle et souffle. */
class VinylNoise(seed: Int = 777) {
    private val n = Noise(seed)
    private val hiss = OnePole().apply { set(5000f) }
    private val crackleLp = OnePole().apply { set(2500f) }
    private var pop = 0f
    var amount = 0f

    fun next(): Float {
        if (amount <= 0f) return 0f
        val r = n.next()
        if (abs(r) > 0.9996f) pop = r * 0.9f
        pop *= 0.93f
        val c = crackleLp.lp(pop + if (abs(r) > 0.985f) r * 0.06f else 0f)
        val h = hiss.lp(n.next()) * 0.025f
        return (c + h) * amount
    }
}
