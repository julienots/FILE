package com.superessence.pulsar.engine

import kotlin.math.abs
import kotlin.math.ceil
import kotlin.math.exp
import kotlin.math.max
import kotlin.math.min
import kotlin.math.pow

/** Pluie : bruit rose filtré + gouttes aléatoires. */
class RainNoise(seed: Int = 4711) {
    private val n = Noise(seed)
    private val lp = Biquad().apply { lowpass(3200f, 0.6f) }
    private val hp = Biquad().apply { highpass(350f, 0.6f) }
    private var b0 = 0f; private var b1 = 0f; private var b2 = 0f
    private var drop = 0f; private var dropPh = 0f; private var dropF = 3000f
    var amount = 0f

    fun next(): Float {
        if (amount <= 0f) return 0f
        val w = n.next()
        b0 = 0.99765f * b0 + w * 0.0990460f
        b1 = 0.96300f * b1 + w * 0.2965164f
        b2 = 0.57000f * b2 + w * 1.0526913f
        val pink = (b0 + b1 + b2 + w * 0.1848f) * 0.11f
        if (n.next() > 0.9993f) { drop = 0.35f * abs(n.next()); dropF = 1800f + abs(n.next()) * 4000f }
        dropPh += dropF / SRF; if (dropPh > 1f) dropPh -= 1f
        val d = Osc.fastSin(dropPh) * drop
        drop *= 0.996f
        return (hp.process(lp.process(pink)) * 0.6f + d * 0.25f) * amount
    }
}

/** Réglages en direct (modifiables depuis l'interface pendant la lecture). */
class LiveParams {
    @Volatile var bpm: Float = 0f // 0 = tempo du morceau
    @Volatile var masterVolume = 0.9f
    val volume = FloatArray(TrackId.COUNT) { 0.8f }
    val mute = BooleanArray(TrackId.COUNT)
    val solo = BooleanArray(TrackId.COUNT)
    @Volatile var reverb = 1f
    @Volatile var delay = 1f
    /** -1 = passe-bas fermé, 0 = neutre, +1 = passe-haut ouvert. */
    @Volatile var djFilter = 0f
    @Volatile var drive = 1f
    @Volatile var sidechain = 1f
    @Volatile var lofi = 1f
    @Volatile var vinyl = -1f // -1 = valeur du genre
    @Volatile var rain = -1f
    @Volatile var width = 1f
    @Volatile var loopSection = false
}

/**
 * Moteur temps réel : séquenceur à l'échantillon près, voix, bus, effets, mastering.
 * Appeler [render] depuis un seul thread audio. Les autres méthodes sont sûres depuis l'UI.
 */
class Engine(val live: LiveParams = LiveParams()) {

    @Volatile var song: Song? = null; private set
    @Volatile private var pendingSong: Song? = null
    @Volatile private var pendingJumpBar = -1
    /** Morceau suivant (mode radio infinie) : enchaîné à la fin du morceau courant. */
    @Volatile var queuedSong: Song? = null
    @Volatile var loopSong = true
    @Volatile var finished = false; private set

    // position
    @Volatile var barIndex = 0; private set
    @Volatile var stepPos = 0.0; private set
    @Volatile var songsPlayed = 0; private set
    private val trackPtr = IntArray(TrackId.COUNT)

    // visualisation
    val scope = FloatArray(2048)
    @Volatile var scopeIdx = 0; private set
    val meters = FloatArray(TrackId.COUNT)
    @Volatile var masterPeak = 0f; private set
    @Volatile var kickPulse = 0f; private set

    private var genreApplied: Genre? = null
    // statistiques (tests / réglage du mixage)
    var statPrePeak = 0f
    var statPreSum = 0.0
    var statPreCount = 0L

    // voix
    private val voices: Array<Array<Voice>> = Array(TrackId.COUNT) { emptyArray() }
    private val maxBlock = 4096
    private val tl = Array(TrackId.COUNT) { FloatArray(maxBlock) }
    private val tr = Array(TrackId.COUNT) { FloatArray(maxBlock) }

    // effets
    private val reverb = Reverb()
    private val delay = StereoDelay()
    private val limiter = Limiter(0.93f)
    private val wowL = TapeWow(); private val wowR = TapeWow()
    private val vinyl = VinylNoise()
    private val rain = RainNoise()
    private val toneL = Biquad(); private val toneR = Biquad()
    private val autoLpL = Biquad(); private val autoLpR = Biquad()
    private val musicHpL = Biquad(); private val musicHpR = Biquad()
    private val djL = Biquad(); private val djR = Biquad()
    private val lowShelfL = Biquad(); private val lowShelfR = Biquad()
    private val tmp = FloatArray(2)
    private var crushHold = 0; private var crushL = 0f; private var crushR = 0f
    private var sc = 0f; private var scCoef = 0.9995f
    private var lastDj = 99f
    private var ageCounter = 0L

    // sends par piste : réverb, délai
    private val revSend = floatArrayOf(0.05f, 0.25f, 0.1f, 0.2f, 0.02f, 0.45f, 0.4f, 0.4f, 0.5f, 0f)
    private val dlySend = floatArrayOf(0f, 0.06f, 0f, 0.05f, 0f, 0.12f, 0.35f, 0.3f, 0.15f, 0f)
    private val sidechained = booleanArrayOf(false, false, false, false, true, true, true, true, true, false)
    private val trackGain = floatArrayOf(1f, 1f, 1f, 1f, 1f, 1f, 1f, 1f, 1f, 1f)

    init {
        lowShelfL.lowShelf(90f, 1.5f); lowShelfR.lowShelf(90f, 1.5f)
    }

    fun load(s: Song, startBar: Int = 0) {
        pendingSong = s
        pendingJumpBar = startBar
    }

    fun jumpToBar(bar: Int) { pendingJumpBar = bar }

    fun jumpToSection(index: Int) {
        val s = song ?: return
        if (index in s.sections.indices) pendingJumpBar = s.sections[index].startBar
    }

    /** Saute au prochain drop / refrain (bouton « DROP ! »). */
    fun jumpToNextDrop() {
        val s = song ?: return
        val cur = barIndex
        val target = s.sections.firstOrNull { it.startBar > cur && (it.type == SectionType.DROP || it.type == SectionType.CHORUS) }
            ?: s.sections.firstOrNull { it.type == SectionType.DROP || it.type == SectionType.CHORUS }
        if (target != null) {
            // si une montée précède, on la joue d'abord
            val idx = s.sections.indexOf(target)
            val prev = s.sections.getOrNull(idx - 1)
            pendingJumpBar = if (prev != null && prev.type == SectionType.BUILD && prev.startBar > cur) prev.startBar else target.startBar
        }
    }

    val currentSectionIndex: Int get() {
        val s = song ?: return 0
        val b = barIndex
        return s.sections.indexOfLast { it.startBar <= b }.coerceAtLeast(0)
    }

    private fun applyGenre(g: Genre) {
        if (genreApplied === g) {
            return
        }
        genreApplied = g
        fun pool(n: Int, f: () -> Voice) = Array(n) { f() }
        voices[TrackId.KICK.ordinal] = pool(2) { KickVoice(g.kick) }
        voices[TrackId.SNARE.ordinal] = pool(4) { SnareVoice(g.snare) }
        voices[TrackId.HATS.ordinal] = pool(5) { HatVoice(g.hatGain) }
        voices[TrackId.PERC.ordinal] = pool(5) { PercVoice(0.45f) }
        voices[TrackId.BASS.ordinal] = pool(if (g.bass.mono) 1 else 3) { SynthVoice(g.bass) }
        voices[TrackId.CHORDS.ordinal] = pool(14) { SynthVoice(g.chords) }
        voices[TrackId.LEAD.ordinal] = pool(if (g.lead.mono) 1 else 6) { SynthVoice(g.lead) }
        voices[TrackId.ARP.ordinal] = pool(6) { SynthVoice(g.arp) }
        voices[TrackId.FX.ordinal] = pool(4) { FxVoice(0.32f) }
        voices[TrackId.TEXTURE.ordinal] = emptyArray()
        reverb.setSize(g.reverb)
        reverb.damping = if (g.family == Family.CHILL) 0.45f else 0.3f
        toneL.lowpass(g.tone, 0.6f); toneR.lowpass(g.tone, 0.6f)
        wowL.depth = g.wow; wowR.depth = g.wow
        // gains de piste équilibrés par genre
        trackGain[TrackId.BASS.ordinal] = 1f
    }

    private fun samplesPerStep(): Float {
        val s = song ?: return SRF / 8f
        val bpm = if (live.bpm > 0f) live.bpm else s.bpm
        return SRF * 60f / bpm / 4f
    }

    val bpm: Float get() = if (live.bpm > 0f) live.bpm else (song?.bpm ?: 120f)

    /** Rend [n] trames stéréo entrelacées dans [out] (L,R,L,R…). */
    fun render(out: FloatArray, n: Int) {
        var done = 0
        while (done < n) {
            val chunk = min(n - done, maxBlock)
            renderBlock(out, done, chunk)
            done += chunk
        }
    }

    private fun renderBlock(out: FloatArray, outOffset: Int, n: Int) {
        pendingSong?.let { s ->
            pendingSong = null
            song = s
            applyGenre(s.genre)
            finished = false
            songsPlayed++
        }
        val s = song
        if (s == null) { for (i in 0 until n * 2) out[outOffset * 2 + i] = 0f; return }
        if (pendingJumpBar >= 0) {
            barIndex = pendingJumpBar.coerceIn(0, s.bars.size - 1); stepPos = 0.0
            trackPtr.fill(0); pendingJumpBar = -1
        }

        for (t in 0 until TrackId.COUNT) { java.util.Arrays.fill(tl[t], 0, n, 0f); java.util.Arrays.fill(tr[t], 0, n, 0f) }

        var done = 0
        while (done < n) {
            val cur = song ?: break
            val bar = cur.bars[barIndex]
            val sps = samplesPerStep()
            // déclenche les événements échus
            var nextEv = 16.0
            for (t in 0 until TrackId.COUNT) {
                val evs = bar.tracks[t]
                var p = trackPtr[t]
                while (p < evs.size) {
                    val ev = evs[p]
                    val es = effStep(ev.step, cur.swing)
                    if (es <= stepPos + 1e-6) {
                        trigger(t, ev, sps); p++
                    } else { if (es < nextEv) nextEv = es; break }
                }
                trackPtr[t] = p
            }
            val toEvent = ceil((nextEv - stepPos) * sps).toInt().coerceAtLeast(1)
            val chunk = min(min(n - done, toEvent), 128)
            for (t in 0 until TrackId.COUNT) {
                for (v in voices[t]) if (v.active) v.render(tl[t], tr[t], done, chunk)
            }
            stepPos += chunk / sps
            done += chunk
            if (stepPos >= 16.0 - 1e-9) {
                stepPos -= 16.0
                trackPtr.fill(0)
                advanceBar(cur)
            }
        }
        mix(out, outOffset, n)
    }

    private fun effStep(step: Float, swing: Float): Double {
        val i = step.toInt()
        return if (swing > 0f && step == i.toFloat() && i % 2 == 1) step + swing * 0.34 else step.toDouble()
    }

    private fun advanceBar(cur: Song) {
        val secIdx = currentSectionIndex
        val sec = cur.sections[secIdx]
        if (live.loopSection && barIndex + 1 >= sec.startBar + sec.bars) {
            barIndex = sec.startBar; return
        }
        barIndex++
        if (barIndex >= cur.bars.size) {
            val q = queuedSong
            if (q != null) {
                queuedSong = null
                song = q; applyGenre(q.genre); songsPlayed++
                barIndex = 0
            } else if (loopSong) {
                barIndex = 0
            } else {
                barIndex = cur.bars.size - 1
                finished = true
            }
        }
    }

    private fun trigger(track: Int, ev: NoteEv, sps: Float) {
        val pool = voices[track]
        if (pool.isEmpty()) return
        val len = (ev.len * sps).toInt().coerceAtLeast(64)
        if (track == TrackId.KICK.ordinal) { sc = 1f; kickPulse = 1f }
        if (track == TrackId.HATS.ordinal && ev.note == 0) for (v in pool) if (v.active && v.note == 1) v.release()
        var v: Voice? = null
        if (pool.size == 1) v = pool[0]
        else {
            for (c in pool) if (!c.active) { v = c; break }
            if (v == null) { v = pool.minBy { it.age } }
        }
        v!!.age = ageCounter++
        v.start(ev.note, ev.vel, len, ev.slide, ev.accent, ev.mod, sps)
    }

    private fun mix(out: FloatArray, off: Int, n: Int) {
        val s = song ?: return
        val g = s.genre
        val bar = s.bars[barIndex]
        val lv = live
        val anySolo = lv.solo.any { it }
        val gains = FloatArray(TrackId.COUNT)
        for (t in 0 until TrackId.COUNT) {
            val audible = !lv.mute[t] && (!anySolo || lv.solo[t])
            gains[t] = if (audible) lv.volume[t] * 1.25f * trackGain[t] else 0f
        }
        // automations de section
        val x = (stepPos / 16.0).toFloat()
        val lpN = bar.lpFrom + (bar.lpTo - bar.lpFrom) * x
        val hpN = bar.hpFrom + (bar.hpTo - bar.hpFrom) * x
        val lpHz = 150f * (20000f / 150f).pow(lpN.coerceIn(0f, 1f))
        autoLpL.lowpass(lpHz, 0.75f); autoLpR.lowpass(lpHz, 0.75f)
        val hpHz = 20f * (2500f / 20f).pow(hpN.coerceIn(0f, 1f))
        musicHpL.highpass(hpHz, 0.7f); musicHpR.highpass(hpHz, 0.7f)
        val dj = lv.djFilter
        if (dj != lastDj) {
            lastDj = dj
            if (dj < -0.02f) { val f = 18000f * (120f / 18000f).pow(-dj); djL.lowpass(f, 1.1f); djR.lowpass(f, 1.1f) }
            else if (dj > 0.02f) { val f = 20f * (4000f / 20f).pow(dj); djL.highpass(f, 1.1f); djR.highpass(f, 1.1f) }
        }
        val revAmt = g.reverb * lv.reverb
        val dlyAmt = g.delay * lv.delay
        delay.timeSamples = ((60f / bpm) * SRF * (if (g.family == Family.CHILL) 0.75f else 0.5f)).toInt()
        delay.feedback = 0.38f
        val scDepth = (g.sidechain * lv.sidechain).coerceIn(0f, 0.95f)
        scCoef = exp(-1f / ((60f / bpm) * 0.28f * SRF))
        val drive = g.drive * lv.drive
        val driveGain = 1f + drive * 1.3f
        val driveNorm = 1f / softClip(driveGain)
        val crush = g.crush * lv.lofi
        val crushRate = (1 + (crush * 4f).toInt()).coerceAtLeast(1)
        val crushBits = if (crush > 0.01f) 2f.pow(16f - crush * 9f) else 0f
        wowL.depth = g.wow * lv.lofi; wowR.depth = g.wow * lv.lofi
        vinyl.amount = (if (lv.vinyl >= 0f) lv.vinyl else g.vinyl) * gains[TrackId.TEXTURE.ordinal] / 1.0f
        rain.amount = (if (lv.rain >= 0f) lv.rain else g.rain) * gains[TrackId.TEXTURE.ordinal] / 1.0f
        val width = lv.width
        val vol = lv.masterVolume * g.gain
        val toneActive = g.tone < 19000f || lv.lofi != 1f
        if (lv.lofi != 1f) { val tf = (g.tone * (1f / lv.lofi.coerceAtLeast(0.2f))).coerceAtMost(20000f); toneL.lowpass(tf, 0.6f); toneR.lowpass(tf, 0.6f) }

        // compteurs de niveaux
        for (t in 0 until TrackId.COUNT) {
            var pk = 0f
            val a = tl[t]; val b = tr[t]
            for (i in 0 until n step 8) { val v = abs(a[i]) + abs(b[i]); if (v > pk) pk = v }
            meters[t] = max(pk * 0.5f * gains[t], meters[t] * 0.85f)
        }

        var peak = 0f
        for (i in 0 until n) {
            sc *= scCoef
            val scGain = 1f - scDepth * sc
            var dl = 0f; var dr = 0f; var ml = 0f; var mr = 0f
            var revL = 0f; var revR = 0f; var dlyL = 0f; var dlyR = 0f
            for (t in 0 until TrackId.COUNT) {
                val gt = gains[t]
                if (gt == 0f) continue
                var l = tl[t][i] * gt; var r = tr[t][i] * gt
                if (sidechained[t]) { l *= scGain; r *= scGain }
                if (t <= TrackId.PERC.ordinal) { dl += l; dr += r } else { ml += l; mr += r }
                val rs = revSend[t]; if (rs > 0f) { revL += l * rs; revR += r * rs }
                val ds = dlySend[t]; if (ds > 0f) { dlyL += l * ds; dlyR += r * ds }
            }
            // passe-haut des montées sur la musique
            if (hpN > 0.001f) { ml = musicHpL.process(ml); mr = musicHpR.process(mr) }
            reverb.process(revL * revAmt, revR * revAmt, tmp)
            var l = dl + ml + tmp[0] * 1.0f
            var r = dr + mr + tmp[1] * 1.0f
            delay.process(dlyL * dlyAmt, dlyR * dlyAmt, tmp)
            l += tmp[0]; r += tmp[1]
            // largeur stéréo
            if (width != 1f) { val m = (l + r) * 0.5f; val sd = (l - r) * 0.5f * width; l = m + sd; r = m - sd }
            // automation passe-bas
            if (lpN < 0.995f) { l = autoLpL.process(l); r = autoLpR.process(r) }
            if (dj < -0.02f || dj > 0.02f) { l = djL.process(l); r = djR.process(r) }
            // couleur lo-fi
            if (toneActive) { l = toneL.process(l); r = toneR.process(r) }
            if (g.wow > 0f) { l = wowL.process(l); r = wowR.process(r) }
            if (crushBits > 0f) {
                if (crushHold <= 0) { crushL = (l * crushBits).toInt() / crushBits; crushR = (r * crushBits).toInt() / crushBits; crushHold = crushRate }
                crushHold--
                l = l * (1f - crush * 0.6f) + crushL * crush * 0.6f; r = r * (1f - crush * 0.6f) + crushR * crush * 0.6f
            }
            val tex = vinyl.next() + rain.next()
            l += tex; r += tex * 0.92f + rain.next() * 0.0f
            l = lowShelfL.process(l) * MIX_TRIM; r = lowShelfR.process(r) * MIX_TRIM
            if (drive > 0.01f) { l = softClip(l * driveGain) * driveNorm; r = softClip(r * driveGain) * driveNorm }
            val pre = maxOf(abs(l), abs(r)) * vol
            if (pre > statPrePeak) statPrePeak = pre
            statPreSum += pre * pre; statPreCount++
            limiter.process(l * vol, r * vol, tmp)
            l = tmp[0]; r = tmp[1]
            val o = (off + i) * 2
            out[o] = l; out[o + 1] = r
            val a = abs(l); if (a > peak) peak = a
            if ((i and 1) == 0) {
                scope[scopeIdx] = (l + r) * 0.5f
                scopeIdx = (scopeIdx + 1) and 2047
            }
        }
        masterPeak = max(peak, masterPeak * 0.9f)
        kickPulse *= 0.86f
    }

    /** Rendu hors ligne d'un morceau complet (export). [onBlock] reçoit des trames stéréo entrelacées. */
    companion object {
        /** Marge de mixage avant saturation / limiteur. */
        const val MIX_TRIM = 0.5f
        var lastOffline: Engine? = null
        fun renderOffline(song: Song, live: LiveParams, tailSeconds: Float = 3f, onProgress: (Float) -> Unit = {}, onBlock: (FloatArray, Int) -> Unit) {
            val e = Engine(live)
            lastOffline = e
            e.loopSong = false
            e.load(song)
            val block = 2048
            val buf = FloatArray(block * 2)
            val bpm = if (live.bpm > 0f) live.bpm else song.bpm
            val total = (song.bars.size * 4f * 60f / bpm * SRF).toLong()
            var rendered = 0L
            while (rendered < total) {
                val n = min(block.toLong(), total - rendered).toInt()
                e.render(buf, n)
                onBlock(buf, n)
                rendered += n
                if ((rendered / block) % 50 == 0L) onProgress(rendered.toFloat() / total)
            }
            // queue de réverbération : on coupe les événements
            e.pendingSong = null
            val tail = (tailSeconds * SRF).toLong()
            var tr = 0L
            e.song = null
            while (tr < tail) {
                val n = min(block.toLong(), tail - tr).toInt()
                e.renderTail(buf, n, 1f - tr.toFloat() / tail)
                onBlock(buf, n)
                tr += n
            }
            onProgress(1f)
        }
    }

    private fun renderTail(out: FloatArray, n: Int, fade: Float) {
        for (i in 0 until n) {
            reverb.process(0f, 0f, tmp)
            var l = tmp[0]; var r = tmp[1]
            delay.process(0f, 0f, tmp)
            l += tmp[0]; r += tmp[1]
            out[i * 2] = softClip(l) * fade * live.masterVolume; out[i * 2 + 1] = softClip(r) * fade * live.masterVolume
        }
    }
}
