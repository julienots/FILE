package com.superessence.pulsar.engine

import kotlin.math.exp
import kotlin.math.pow

/** Une voix de synthèse. Les voix sont préallouées et réutilisées : aucune allocation pendant la lecture. */
abstract class Voice {
    var active = false
    var note = 0
    var age = 0L
    /** Démarre une note. [len] en échantillons ; [mod] paramètre libre (ex : vitesse du LFO). */
    abstract fun start(note: Int, vel: Float, len: Int, slide: Boolean, accent: Boolean, mod: Float, samplesPerStep: Float)
    /** Ajoute la sortie de la voix dans l/r entre [off] et [off]+[n]. */
    abstract fun render(l: FloatArray, r: FloatArray, off: Int, n: Int)
    open fun release() {}
}

// ───────────────────────────── Batterie ─────────────────────────────

data class KickStyle(
    val startFreq: Float = 160f,
    val endFreq: Float = 48f,
    val pitchDecay: Float = 0.035f,
    val ampDecay: Float = 0.35f,
    val click: Float = 0.25f,
    val drive: Float = 1.2f,
    val gain: Float = 1f,
    /** Kick « tonal » (hardstyle / 808) : la fréquence finale suit la note jouée. */
    val tuned: Boolean = false,
    val distortion: Float = 0f,
)

class KickVoice(var style: KickStyle) : Voice() {
    private var t = 0; private var phase = 0f; private var vel = 1f; private var endF = 48f
    private val n = Noise(91)
    private val post = Biquad()
    private var ampCoef = 0f; private var pitchCoef = 0f; private var pitchEnv = 1f; private var amp = 1f

    override fun start(note: Int, vel: Float, len: Int, slide: Boolean, accent: Boolean, mod: Float, samplesPerStep: Float) {
        active = true; this.note = note; t = 0; phase = 0f; this.vel = vel
        endF = if (style.tuned && note > 0) {
            var f = mtof(note.toFloat())
            while (f > 82f) f *= 0.5f
            while (f < 42f) f *= 2f
            f
        } else style.endFreq
        ampCoef = exp(-1f / (style.ampDecay * SRF)); pitchCoef = exp(-1f / (style.pitchDecay * SRF))
        pitchEnv = 1f; amp = 1f
        post.lowpass(if (style.distortion > 0f) 4200f else 9000f, 0.8f)
    }

    override fun render(l: FloatArray, r: FloatArray, off: Int, n: Int) {
        val st = style
        val norm = 1f / softClip(st.drive.coerceAtLeast(0.01f))
        for (i in off until off + n) {
            val f = endF + (st.startFreq - endF) * pitchEnv
            phase += f / SRF; if (phase > 1f) phase -= 1f
            var s = Osc.fastSin(phase) * amp
            if (t < 400) s += this.n.next() * st.click * (1f - t / 400f)
            s = softClip(s * st.drive) * norm
            if (st.distortion > 0f) s = softClip(s * (1f + st.distortion * 6f)) * 0.9f
            s = post.process(s) * vel * st.gain
            l[i] += s; r[i] += s
            pitchEnv *= pitchCoef; amp *= ampCoef
            t++
        }
        if (amp < 0.0008f) active = false
    }
}

enum class SnareKind { SNARE, CLAP, RIM, SNAP }

data class SnareStyle(
    val kind: SnareKind = SnareKind.SNARE,
    val tone: Float = 190f,
    val toneMix: Float = 0.45f,
    val decay: Float = 0.18f,
    val noiseColor: Float = 1800f,
    val gain: Float = 0.8f,
)

class SnareVoice(var style: SnareStyle) : Voice() {
    private var t = 0; private var vel = 1f; private var phase = 0f
    private val noise = Noise(4242)
    private val hp = Biquad(); private val bp = Biquad()
    private var nCoef = 0f; private var tCoef = 0f; private var nEnv = 1f; private var tEnv = 1f
    private var kind = SnareKind.SNARE

    override fun start(note: Int, vel: Float, len: Int, slide: Boolean, accent: Boolean, mod: Float, samplesPerStep: Float) {
        active = true; t = 0; this.vel = vel; phase = 0f
        // note 1 = variante clap / note 2 = rim sur la piste caisse claire
        kind = when (note) { 1 -> SnareKind.CLAP; 2 -> SnareKind.RIM; else -> style.kind }
        val dec = when (kind) { SnareKind.RIM -> 0.05f; SnareKind.SNAP -> 0.08f; SnareKind.CLAP -> style.decay * 1.1f; else -> style.decay }
        nCoef = exp(-1f / (dec * SRF)); tCoef = exp(-1f / (0.06f * SRF))
        nEnv = 1f; tEnv = 1f
        hp.highpass(style.noiseColor, 0.7f)
        bp.bandpass(if (kind == SnareKind.CLAP) 1150f else 2500f, if (kind == SnareKind.CLAP) 1.4f else 0.6f)
    }

    override fun render(l: FloatArray, r: FloatArray, off: Int, n: Int) {
        val st = style
        for (i in off until off + n) {
            var s: Float
            when (kind) {
                SnareKind.CLAP -> {
                    // trois micro-claquements puis la queue
                    val burst = t < 1700 && (t % 520) < 140
                    val env = if (t < 1700) (if (burst) 1f else 0.25f) else nEnv
                    s = bp.process(noise.next()) * env * 2.2f
                }
                SnareKind.RIM -> {
                    phase += 1650f / SRF; if (phase > 1f) phase -= 1f
                    s = (Osc.tri(phase) * 0.7f + hp.process(noise.next()) * 0.3f) * nEnv
                }
                SnareKind.SNAP -> {
                    s = bp.process(noise.next()) * nEnv * 2f
                }
                SnareKind.SNARE -> {
                    val f = st.tone * (1f + 0.4f * tEnv)
                    phase += f / SRF; if (phase > 1f) phase -= 1f
                    val tone = Osc.tri(phase) * tEnv * st.toneMix
                    val nz = hp.process(noise.next()) * nEnv * (1f - st.toneMix * 0.5f)
                    s = tone + nz
                }
            }
            s *= vel * st.gain
            l[i] += s; r[i] += s
            nEnv *= nCoef; tEnv *= tCoef; t++
        }
        if (nEnv < 0.0005f && t > 2000) active = false
    }
}

class HatVoice(var gain: Float = 0.35f, var closedDecay: Float = 0.045f, var openDecay: Float = 0.32f) : Voice() {
    private val ratios = floatArrayOf(2f, 3f, 4.16f, 5.43f, 6.79f, 8.21f)
    private val phases = FloatArray(6)
    private val bp = Biquad(); private val hp = Biquad()
    private val noise = Noise(1357)
    private var env = 1f; private var coef = 0f; private var vel = 1f; private var pan = 0f

    override fun start(note: Int, vel: Float, len: Int, slide: Boolean, accent: Boolean, mod: Float, samplesPerStep: Float) {
        active = true; this.note = note; this.vel = vel; env = 1f
        val d = when (note) { 1 -> openDecay; 2 -> 0.6f; else -> closedDecay * (0.8f + vel * 0.4f) }
        coef = exp(-1f / (d * SRF))
        bp.bandpass(if (note == 2) 7000f else 10000f, 0.9f); hp.highpass(7000f, 0.7f)
        pan = if (note == 1) 0.2f else -0.15f
    }

    override fun render(l: FloatArray, r: FloatArray, off: Int, n: Int) {
        val base = 40f
        for (i in off until off + n) {
            var m = 0f
            for (k in 0 until 6) {
                phases[k] += base * ratios[k] / SRF; if (phases[k] > 1f) phases[k] -= 1f
                m += if (phases[k] < 0.5f) 1f else -1f
            }
            val s = hp.process(bp.process(m * 0.12f + noise.next() * 0.35f)) * env * vel * gain
            l[i] += s * (1f - pan); r[i] += s * (1f + pan)
            env *= coef
        }
        if (env < 0.0005f) active = false
    }

    override fun release() { coef = exp(-1f / (0.02f * SRF)) } // hat ouvert coupé par le fermé
}

/** Percussions : 0 shaker, 1 cowbell, 2 conga, 3 tom, 4 crash, 5 bongo, 6 woodblock. */
class PercVoice(var gain: Float = 0.5f) : Voice() {
    private var kind = 0; private var t = 0; private var vel = 1f
    private var env = 1f; private var coef = 0f; private var ph1 = 0f; private var ph2 = 0f; private var f0 = 200f; private var pitchEnv = 1f
    private val noise = Noise(8080)
    private val f1 = Biquad(); private val f2 = Biquad()
    private var pan = 0f

    override fun start(note: Int, vel: Float, len: Int, slide: Boolean, accent: Boolean, mod: Float, samplesPerStep: Float) {
        active = true; kind = note; t = 0; this.vel = vel; env = 1f; pitchEnv = 1f; ph1 = 0f; ph2 = 0f
        val d: Float
        when (kind) {
            0 -> { d = 0.06f; f1.bandpass(6500f, 1.2f); pan = 0.35f }
            1 -> { d = 0.25f; f1.bandpass(2600f, 2f); pan = -0.2f }
            2 -> { d = 0.22f; f0 = 210f; pan = 0.3f }
            3 -> { d = 0.35f; f0 = 110f; pan = -0.3f }
            4 -> { d = 1.6f; f1.highpass(5200f, 0.6f); f2.bandpass(8500f, 0.5f); pan = 0.15f }
            5 -> { d = 0.12f; f0 = 340f; pan = -0.4f }
            else -> { d = 0.06f; f0 = 900f; pan = 0.4f }
        }
        coef = exp(-1f / (d * SRF))
    }

    override fun render(l: FloatArray, r: FloatArray, off: Int, n: Int) {
        val pc = exp(-1f / (0.02f * SRF))
        for (i in off until off + n) {
            val s: Float = when (kind) {
                0 -> f1.process(noise.next()) * env * (if (t < 220) t / 220f else 1f) * 1.4f
                1 -> {
                    ph1 += 540f / SRF; if (ph1 > 1f) ph1 -= 1f
                    ph2 += 800f / SRF; if (ph2 > 1f) ph2 -= 1f
                    f1.process((if (ph1 < 0.5f) 1f else -1f) + (if (ph2 < 0.5f) 1f else -1f)) * env * 0.8f
                }
                4 -> (f1.process(noise.next()) * 0.6f + f2.process(noise.next()) * 0.4f) * env * 0.9f
                else -> {
                    val f = f0 * (1f + 0.25f * pitchEnv)
                    ph1 += f / SRF; if (ph1 > 1f) ph1 -= 1f
                    pitchEnv *= pc
                    Osc.fastSin(ph1) * env * (if (kind == 6) 0.7f else 1f)
                }
            }
            val o = s * vel * gain
            l[i] += o * (1f - pan); r[i] += o * (1f + pan)
            env *= coef; t++
        }
        if (env < 0.0005f) active = false
    }
}

// ───────────────────────────── Synthés mélodiques ─────────────────────────────

enum class Wave { SINE, TRI, SAW, SQUARE, SUPERSAW, FM_EP, FM_BELL, PLUCK, ORGAN, PIANO, MARIMBA, COWBELL, SUB, B808, REESE, ACID, WOBBLE, SCREECH, CHOIR }

data class Patch(
    val wave: Wave,
    val a: Float = 0.005f, val d: Float = 0.3f, val s: Float = 0.6f, val r: Float = 0.3f,
    val cutoff: Float = 4000f,
    val res: Float = 0.2f,
    /** Modulation du filtre par l'enveloppe, en octaves. */
    val envAmt: Float = 0f,
    val fd: Float = 0.25f,
    val detune: Float = 0.12f,
    val vibrato: Float = 0f,
    val glide: Float = 0f,
    val drive: Float = 0f,
    val gain: Float = 0.3f,
    val transpose: Int = 0,
    val mono: Boolean = false,
    val width: Float = 0.6f,
)

class SynthVoice(var patch: Patch) : Voice() {
    private val amp = Adsr(); private val fenv = Adsr()
    private val svf = Svf(); private val svf2 = Svf()
    private val phases = FloatArray(7)
    private val detunes = floatArrayOf(0f, -1f, 1f, -0.62f, 0.62f, -0.3f, 0.3f)
    private var freq = 440f; private var target = 440f; private var glideCoef = 0f
    private var vel = 1f; private var remaining = 0
    private var t = 0
    private var lfoPh = 0f; private var lfoInc = 0f; private var vibPh = 0f
    private var accentAmt = 0f
    private val noise = Noise(31337)
    // Karplus-Strong
    private val ks = FloatArray(SR / 25); private var ksLen = 100; private var ksIdx = 0; private var ksLast = 0f
    // FM / additif
    private var modPh = 0f; private var modPh2 = 0f; private var idxEnv = 1f; private var idxCoef = 0f
    private val partials = FloatArray(6); private val partialPh = FloatArray(6); private val partialCoef = FloatArray(6)
    private var pitchEnv = 0f; private var pitchCoef = 0f
    private var counter = 0

    override fun start(note: Int, vel: Float, len: Int, slide: Boolean, accent: Boolean, mod: Float, samplesPerStep: Float) {
        val p = patch
        val n = note + p.transpose
        val newF = mtof(n.toFloat())
        val legato = slide && active && (p.mono || p.glide > 0f)
        this.note = note
        target = newF
        remaining = len
        this.vel = vel
        accentAmt = if (accent) 1f else 0f
        if (mod > 0f) lfoInc = mod / samplesPerStep
        glideCoef = if (p.glide > 0f || legato) exp(-1f / ((if (p.glide > 0f) p.glide else 0.06f) * SRF)) else 0f
        if (legato) return // glissé : on garde l'enveloppe en cours
        freq = if (p.wave == Wave.B808 && slide && active) freq else newF
        active = true; t = 0
        amp.set(p.a, p.d, p.s, p.r); amp.gate()
        fenv.set(0.002f, p.fd, 0f, p.fd); fenv.gate()
        if (!p.mono) for (i in phases.indices) phases[i] = noise.next() * 0.5f + 0.5f
        when (p.wave) {
            Wave.PLUCK -> {
                ksLen = (SRF / newF).toInt().coerceIn(8, ks.size - 1)
                var lp = 0f
                val bright = (p.cutoff / 8000f).coerceIn(0.1f, 1f)
                for (i in 0 until ksLen) { lp += (noise.next() - lp) * bright; ks[i] = lp }
                ksIdx = 0; ksLast = 0f
            }
            Wave.FM_EP, Wave.FM_BELL -> {
                modPh = 0f; modPh2 = 0f; idxEnv = 1f
                idxCoef = exp(-1f / ((if (p.wave == Wave.FM_BELL) 0.9f else 0.35f) * SRF))
            }
            Wave.PIANO, Wave.MARIMBA -> {
                for (k in 0 until 6) {
                    partialPh[k] = 0f
                    partials[k] = if (p.wave == Wave.MARIMBA) (if (k == 0) 1f else if (k == 2) 0.35f else 0f) else 1f / (k + 1f).pow(1.4f)
                    val dec = if (p.wave == Wave.MARIMBA) (if (k == 0) 0.55f else 0.12f) else 2.6f / (1f + k * 0.9f) * (440f / newF).coerceIn(0.4f, 2f)
                    partialCoef[k] = exp(-1f / (dec * SRF))
                }
            }
            Wave.B808 -> { pitchEnv = 1f; pitchCoef = exp(-1f / (0.045f * SRF)) }
            else -> {}
        }
        if (p.wave == Wave.WOBBLE || p.wave == Wave.REESE) lfoPh = 0f
        svf.reset(); svf2.reset()
    }

    override fun release() { amp.noteOff(); fenv.noteOff() }

    override fun render(l: FloatArray, r: FloatArray, off: Int, n: Int) {
        val p = patch
        val w = p.width
        for (i in off until off + n) {
            if (remaining > 0) { remaining--; if (remaining == 0) amp.noteOff() }
            if (glideCoef > 0f) freq = target + (freq - target) * glideCoef else freq = target
            var f = freq
            if (p.vibrato > 0f && t > SR / 5) {
                vibPh += 5.2f / SRF; if (vibPh > 1f) vibPh -= 1f
                f *= 1f + Osc.fastSin(vibPh) * p.vibrato * 0.012f
            }
            val e = amp.next()
            val fe = fenv.next()
            var sl: Float; var sr: Float
            when (p.wave) {
                Wave.SINE, Wave.SUB -> {
                    phases[0] += f / SRF; if (phases[0] > 1f) phases[0] -= 1f
                    sl = Osc.fastSin(phases[0]); if (p.wave == Wave.SUB) sl += Osc.fastSin(phases[0] * 2f) * 0.12f
                    sr = sl
                }
                Wave.TRI -> { phases[0] += f / SRF; if (phases[0] > 1f) phases[0] -= 1f; sl = Osc.tri(phases[0]); sr = sl }
                Wave.SAW, Wave.ACID, Wave.SCREECH -> {
                    val dt = f / SRF
                    phases[0] += dt; if (phases[0] > 1f) phases[0] -= 1f
                    sl = Osc.saw(phases[0], dt)
                    if (p.wave == Wave.SCREECH) {
                        val dt2 = f * 1.006f / SRF
                        phases[1] += dt2; if (phases[1] > 1f) phases[1] -= 1f
                        sl = sl * 0.6f + Osc.square(phases[1], dt2, 0.3f) * 0.5f
                    }
                    sr = sl
                }
                Wave.SQUARE -> { val dt = f / SRF; phases[0] += dt; if (phases[0] > 1f) phases[0] -= 1f; sl = Osc.square(phases[0], dt, 0.5f) * 0.8f; sr = sl }
                Wave.SUPERSAW, Wave.REESE, Wave.CHOIR -> {
                    val voices = if (p.wave == Wave.REESE) 3 else 7
                    sl = 0f; sr = 0f
                    for (k in 0 until voices) {
                        val ff = f * (1f + detunes[k] * p.detune * 0.05f)
                        val dt = ff / SRF
                        phases[k] += dt; if (phases[k] > 1f) phases[k] -= 1f
                        val s = if (p.wave == Wave.CHOIR) Osc.tri(phases[k]) else Osc.saw(phases[k], dt)
                        val pan = if (k == 0) 0.5f else if (k % 2 == 1) 0.5f - w * 0.5f else 0.5f + w * 0.5f
                        sl += s * (1f - pan); sr += s * pan
                    }
                    val norm = if (voices == 3) 0.7f else 0.42f
                    sl *= norm; sr *= norm
                }
                Wave.WOBBLE -> {
                    val dt = f / SRF
                    phases[0] += dt; if (phases[0] > 1f) phases[0] -= 1f
                    phases[1] += dt * 1.005f; if (phases[1] > 1f) phases[1] -= 1f
                    phases[2] += dt * 0.5f; if (phases[2] > 1f) phases[2] -= 1f
                    sl = Osc.saw(phases[0], dt) * 0.6f + Osc.square(phases[1], dt) * 0.5f + Osc.fastSin(phases[2]) * 0.6f
                    sr = sl
                }
                Wave.B808 -> {
                    val ff = f * (1f + pitchEnv * 1.2f)
                    pitchEnv *= pitchCoef
                    phases[0] += ff / SRF; if (phases[0] > 1f) phases[0] -= 1f
                    sl = Osc.fastSin(phases[0]); sr = sl
                }
                Wave.FM_EP -> {
                    modPh += f / SRF; if (modPh > 1f) modPh -= 1f
                    modPh2 += f * 14f / SRF; if (modPh2 > 1f) modPh2 -= 1f
                    phases[0] += f / SRF; if (phases[0] > 1f) phases[0] -= 1f
                    val idx = 0.25f + 1.6f * idxEnv * vel
                    val tine = Osc.fastSin(modPh2) * 0.18f * idxEnv * idxEnv
                    sl = Osc.fastSin(phases[0] + Osc.fastSin(modPh) * idx * 0.16f + tine)
                    // trémolo stéréo doux façon Rhodes
                    vibPh += 4.5f / SRF; if (vibPh > 1f) vibPh -= 1f
                    val trem = Osc.fastSin(vibPh) * 0.18f * w
                    sr = sl * (1f - trem); sl *= (1f + trem)
                    idxEnv *= idxCoef
                }
                Wave.FM_BELL -> {
                    modPh += f * 3.5f / SRF; if (modPh > 1f) modPh -= 1f
                    phases[0] += f / SRF; if (phases[0] > 1f) phases[0] -= 1f
                    sl = Osc.fastSin(phases[0] + Osc.fastSin(modPh) * (0.05f + 0.6f * idxEnv)); sr = sl
                    idxEnv *= idxCoef
                }
                Wave.PLUCK -> {
                    val a = ks[ksIdx]
                    val nxt = ks[(ksIdx + 1) % ksLen]
                    val y = (a + nxt) * 0.5f * 0.996f
                    ks[ksIdx] = y
                    ksIdx++; if (ksIdx >= ksLen) ksIdx = 0
                    sl = a * 1.6f; sr = sl
                }
                Wave.ORGAN -> {
                    phases[0] += f / SRF; if (phases[0] > 1f) phases[0] -= 1f
                    val ph = phases[0]
                    sl = Osc.fastSin(ph) * 0.6f + Osc.fastSin(ph * 2f) * 0.45f + Osc.fastSin(ph * 3f) * 0.25f + Osc.fastSin(ph * 4f) * 0.15f
                    sr = sl
                }
                Wave.PIANO, Wave.MARIMBA -> {
                    sl = 0f
                    val ratio = if (p.wave == Wave.MARIMBA) 3.93f else 1f
                    for (k in 0 until 6) {
                        if (partials[k] < 0.0001f) continue
                        val mult = if (p.wave == Wave.MARIMBA) (if (k == 0) 1f else ratio * k / 2f) else (k + 1f) * (1f + k * 0.0009f)
                        partialPh[k] += f * mult / SRF; if (partialPh[k] > 1f) partialPh[k] -= 1f
                        sl += Osc.fastSin(partialPh[k]) * partials[k]
                        partials[k] *= partialCoef[k]
                    }
                    if (t < 300 && p.wave == Wave.PIANO) sl += noise.next() * 0.05f * (1f - t / 300f)
                    sr = sl
                }
                Wave.COWBELL -> {
                    val dt = f / SRF
                    phases[0] += dt; if (phases[0] > 1f) phases[0] -= 1f
                    phases[1] += dt * 1.48f; if (phases[1] > 1f) phases[1] -= 1f
                    sl = Osc.square(phases[0], dt) * 0.5f + Osc.square(phases[1], dt * 1.48f) * 0.5f; sr = sl
                }
            }

            // Filtre (mis à jour toutes les 16 échantillons)
            val filtered = p.wave != Wave.PLUCK && p.wave != Wave.PIANO && p.wave != Wave.MARIMBA && p.wave != Wave.FM_BELL && p.wave != Wave.B808
            if (filtered) {
                if ((counter++ and 15) == 0) {
                    var oct = p.envAmt * fe * (1f + accentAmt * 0.8f)
                    if (p.wave == Wave.WOBBLE && lfoInc > 0f) {
                        oct += (Osc.tri(lfoPh) * 0.5f + 0.5f) * 4.5f - 1f
                    }
                    if (p.wave == Wave.REESE) oct += Osc.fastSin(lfoPh * 0.125f) * 0.6f
                    val res = if (p.wave == Wave.ACID) (p.res + accentAmt * 0.12f) else p.res
                    val c = p.cutoff * 2f.pow(oct)
                    svf.set(c, res); svf2.set(c, res)
                }
                if (lfoInc > 0f) { lfoPh += lfoInc; if (lfoPh > 1f) lfoPh -= 1f }
                else if (p.wave == Wave.REESE) { lfoPh += 0.3f / SRF; if (lfoPh > 8f) lfoPh -= 8f }
                sl = svf.process(sl)
                sr = if (sr == sl) sl else svf2.process(sr)
            }
            if (p.drive > 0f) {
                val dg = 1f + p.drive * 8f
                val dn = 1f / softClip(dg)
                sl = softClip(sl * dg) * dn; sr = softClip(sr * dg) * dn
            }
            val g = e * vel * p.gain * (1f + accentAmt * 0.3f)
            l[i] += sl * g; r[i] += sr * g
            t++
        }
        if (!amp.isActive) active = false
        if (p.wave == Wave.PLUCK && t > SR * 3) active = false
    }
}

// ───────────────────────────── Effets sonores ─────────────────────────────

/** 0 riser (montée), 1 impact, 2 descente, 3 cymbale inversée, 4 pluie (texture). */
class FxVoice(var gain: Float = 0.35f) : Voice() {
    private var kind = 0; private var t = 0; private var len = 1; private var vel = 1f
    private val noise = Noise(555)
    private val svfL = Svf(); private val svfR = Svf()
    private var ph = 0f; private var env = 1f; private var coef = 0f
    private val noise2 = Noise(999)

    override fun start(note: Int, vel: Float, len: Int, slide: Boolean, accent: Boolean, mod: Float, samplesPerStep: Float) {
        active = true; kind = note; t = 0; this.len = len.coerceAtLeast(1); this.vel = vel; ph = 0f; env = 1f
        coef = exp(-1f / ((if (kind == 1) 1.4f else 0.8f) * SRF))
        svfL.reset(); svfR.reset()
    }

    override fun render(l: FloatArray, r: FloatArray, off: Int, n: Int) {
        for (i in off until off + n) {
            val x = (t.toFloat() / len).coerceIn(0f, 1f)
            var sl = 0f; var sr = 0f
            when (kind) {
                0 -> {
                    if ((t and 15) == 0) { val c = 300f * 2f.pow(x * 5f); svfL.set(c, 0.55f); svfR.set(c * 1.05f, 0.55f) }
                    val a = x * x
                    ph += (110f * 2f.pow(x * 2f)) / SRF; if (ph > 1f) ph -= 1f
                    sl = svfL.process(noise.next()) ; svfL.band.let { sl = it }
                    svfR.process(noise2.next()); sr = svfR.band
                    val tone = Osc.saw(ph, 0.005f) * 0.15f
                    sl = (sl + tone) * a; sr = (sr + tone) * a
                    if (t >= len) { active = false }
                }
                1 -> {
                    ph += (55f + 60f * env * env) / SRF; if (ph > 1f) ph -= 1f
                    val boom = Osc.fastSin(ph) * env * 1.3f
                    val nz = noise.next() * env * env * 0.35f
                    sl = boom + nz; sr = boom + noise2.next() * env * env * 0.35f
                    env *= coef
                    if (env < 0.001f) active = false
                }
                2 -> {
                    if ((t and 15) == 0) { val c = 9000f * 2f.pow(-x * 5f); svfL.set(c, 0.3f); svfR.set(c, 0.3f) }
                    val a = (1f - x)
                    sl = svfL.process(noise.next()) * a; sr = svfR.process(noise2.next()) * a
                    if (t >= len) active = false
                }
                3 -> {
                    if ((t and 15) == 0) { svfL.set(6000f, 0.2f); svfR.set(6500f, 0.2f) }
                    svfL.process(noise.next()); svfR.process(noise2.next())
                    val a = x.pow(3f)
                    sl = svfL.high * a; sr = svfR.high * a
                    if (t >= len) active = false
                }
            }
            l[i] += sl * gain * vel; r[i] += sr * gain * vel
            t++
        }
    }
}
