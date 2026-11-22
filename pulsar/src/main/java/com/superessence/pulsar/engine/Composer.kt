package com.superessence.pulsar.engine

import kotlin.math.max
import kotlin.math.min
import kotlin.math.roundToInt

/**
 * Compositeur procédural : transforme un genre + une graine en morceau complet
 * (structure, harmonie, batterie, basse, accords, mélodie, arpèges, effets, automations).
 * Entièrement déterministe : même graine + mêmes paramètres = même morceau.
 */
object Composer {

    private val CHILL_A = listOf("Nuages", "Pluie", "Néons", "Minuit", "Café", "Lune", "Brume", "Velours", "Sakura", "Océan", "Vinyle", "Coton", "Lucioles", "Aurore", "Tokyo", "Polaroid", "Rêverie", "Nostalgie", "Cerisier", "Marée")
    private val CHILL_B = listOf("de velours", "endormie", "sur la ville", "à 3h du mat'", "au ralenti", "violette", "dorée", "en hiver", "d'été", "lointaine", "sous la pluie", "de minuit", "en apesanteur", "pastel", "tranquille", "suspendue")
    private val MAD_A = listOf("Séisme", "Ignition", "Chaos", "Turbo", "Fièvre", "Impact", "Tornade", "Voltage", "Nitro", "Rage", "Overdrive", "Zénith", "Fusion", "Inferno", "Tsunami", "Blitz", "Psychose", "Adrénaline", "Supernova", "Uppercut")
    private val MAD_B = listOf("Total", "Max", "Ultime", "Nocturne", "X", "Infernal", "Supersonique", "2099", "Brutal", "Sauvage", "Interdit", "Nucléaire", "Turbo", "de Malade", "Sans Limite", "Mode Démon")

    fun title(g: Genre, rng: Rng): String {
        val a = if (g.family == Family.CHILL) CHILL_A else MAD_A
        val b = if (g.family == Family.CHILL) CHILL_B else MAD_B
        return "${rng.pick(a)} ${rng.pick(b)}"
    }

    fun compose(p: GenParams): Song {
        val g = Genres.byId(p.genreId)
        val rng = Rng(p.seed)
        val bpm = p.bpm ?: rng.range(g.bpmMin, g.bpmMax).toFloat()
        val scale = p.scale ?: rng.pick(g.scales)
        // tonalités confortables : on évite les basses trop graves / aiguës
        val rootPc = p.root ?: rng.pick(intArrayOf(0, 2, 3, 5, 7, 8, 9, 10, 1, 4, 6, 11))
        val swing = p.swing ?: g.swing
        val title = title(g, Rng(p.seed * 31 + 7))

        val progA = rng.pick(g.progressions)
        val progB = if (rng.chance(0.5f)) progA else rng.pick(g.progressions)
        val chordBars = if (g.halfTime || bpm >= 160 || g.id == "techno") 2 else 1

        val sections = structure(g, rng, p.length)
        val bars = ArrayList<Bar>()
        var prevVoicing: IntArray? = null
        var barIndex = 0
        val arpBySection = HashMap<SectionType, Boolean>()
        for ((si, sec) in sections.withIndex()) {
            val prog = if (sec.type == SectionType.CHORUS || sec.type == SectionType.DROP || sec.type == SectionType.BREAK) progB else progA
            val arpOn = arpBySection.getOrPut(sec.type) { Rng(p.seed + sec.type.ordinal * 977L).chance(g.arpChance * (0.6f + p.energy * 0.6f)) }
            for (b in 0 until sec.bars) {
                val chordIdx = (b / chordBars) % prog.size
                val degree = prog[chordIdx]
                val nextDegree = prog[((b / chordBars) + 1) % prog.size]
                val raw = Chords.build(48 + rootPc, scale, degree, g.chordExt)
                val voiced = Chords.voiceLead(raw, prevVoicing, 52, 74)
                val newChord = b % chordBars == 0
                if (newChord) prevVoicing = voiced
                val ctx = Ctx(
                    g = g, p = p, scale = scale, rootPc = rootPc, section = sec.type, sectionIndex = si,
                    bar = b, bars = sec.bars, degree = degree, nextDegree = nextDegree,
                    chordRoot = scale.note(36 + rootPc, degree).let { if (it > 47) it - 12 else it },
                    nextRoot = scale.note(36 + rootPc, nextDegree).let { if (it > 47) it - 12 else it },
                    voicing = prevVoicing!!, newChord = newChord, chordBars = chordBars, arpOn = arpOn,
                    isFirstSection = si == 0, isLastSection = si == sections.lastIndex,
                )
                bars += buildBar(ctx, Chords.name(raw))
                barIndex++
            }
        }
        val infos = ArrayList<SectionInfo>()
        var start = 0
        for (s in sections) { infos += SectionInfo(s.type, start, s.bars); start += s.bars }
        return Song(g, p.seed, title, bpm, swing, rootPc, scale, bars, infos, p)
    }

    private class Sec(val type: SectionType, val bars: Int)

    private fun structure(g: Genre, rng: Rng, length: Float): List<Sec> {
        fun n(b: Int): Int = max(2, ((b * length) / 2f).roundToInt() * 2)
        return when (g.structure) {
            Structure.CHILL -> listOf(
                Sec(SectionType.INTRO, 4), Sec(SectionType.VERSE, n(8)), Sec(SectionType.CHORUS, n(8)),
                Sec(SectionType.BREAK, 4), Sec(SectionType.VERSE, n(8)), Sec(SectionType.CHORUS, n(8)), Sec(SectionType.OUTRO, 4),
            )
            Structure.AMBIENT -> listOf(
                Sec(SectionType.INTRO, 4), Sec(SectionType.VERSE, n(8)), Sec(SectionType.CHORUS, n(8)),
                Sec(SectionType.BREAK, n(8)), Sec(SectionType.CHORUS, n(8)), Sec(SectionType.OUTRO, 4),
            )
            Structure.CLUB -> listOf(
                Sec(SectionType.INTRO, n(8)), Sec(SectionType.VERSE, n(8)), Sec(SectionType.BUILD, 4), Sec(SectionType.DROP, n(16)),
                Sec(SectionType.BREAK, n(8)), Sec(SectionType.BUILD, 4), Sec(SectionType.DROP, n(16)), Sec(SectionType.OUTRO, n(8)),
            )
            Structure.RAP -> listOf(
                Sec(SectionType.INTRO, 4), Sec(SectionType.CHORUS, n(8)), Sec(SectionType.VERSE, n(16)), Sec(SectionType.CHORUS, n(8)),
                Sec(SectionType.VERSE, n(8)), Sec(SectionType.CHORUS, n(8)), Sec(SectionType.OUTRO, 4),
            )
            Structure.FESTIVAL -> listOf(
                Sec(SectionType.INTRO, n(8)), Sec(SectionType.VERSE, n(8)), Sec(SectionType.BUILD, if (rng.chance(0.5f)) 8 else 4), Sec(SectionType.DROP, n(16)),
                Sec(SectionType.BREAK, n(8)), Sec(SectionType.BUILD, 8), Sec(SectionType.DROP, n(16)), Sec(SectionType.OUTRO, 4),
            )
        }
    }

    private class Ctx(
        val g: Genre, val p: GenParams, val scale: Scale, val rootPc: Int,
        val section: SectionType, val sectionIndex: Int, val bar: Int, val bars: Int,
        val degree: Int, val nextDegree: Int, val chordRoot: Int, val nextRoot: Int,
        val voicing: IntArray, val newChord: Boolean, val chordBars: Int, val arpOn: Boolean,
        val isFirstSection: Boolean, val isLastSection: Boolean,
    ) {
        val lastBar get() = bar == bars - 1
        val progress get() = bar.toFloat() / bars
        /** Graine stable par (section, piste, position dans le motif) : répétitions cohérentes. */
        fun rng(track: TrackId, period: Int = 2, salt: Int = 0): Rng {
            val pos = if (lastBar && bars > 2) 99 else bar % period
            return Rng(p.seed * 1_000_003L + section.ordinal * 7919L + track.ordinal * 104729L + pos * 31L + salt)
        }
        fun note(deg: Int, base: Int): Int = scale.note(base + rootPc, degree + deg)
    }

    private fun buildBar(c: Ctx, chordName: String): Bar {
        val tracks = Array<MutableList<NoteEv>>(TrackId.COUNT) { ArrayList() }
        val act = activeTracks(c)

        drums(c, act, tracks)
        if (TrackId.BASS in act) bass(c, tracks)
        if (TrackId.CHORDS in act) chords(c, tracks)
        if (TrackId.LEAD in act) lead(c, tracks)
        if (TrackId.ARP in act) arp(c, tracks)
        fx(c, tracks)

        for (t in tracks) t.sortBy { it.step }

        var lpF = 1f; var lpT = 1f; var hpF = 0f; var hpT = 0f
        val x0 = c.progress; val x1 = (c.bar + 1f) / c.bars
        when (c.section) {
            SectionType.INTRO -> { lpF = 0.42f + 0.5f * x0; lpT = 0.42f + 0.5f * x1 }
            SectionType.OUTRO -> { if (c.isLastSection) { lpF = 1f - 0.6f * x0; lpT = 1f - 0.6f * x1 } }
            SectionType.BUILD -> { hpF = 0.45f * x0 * x0; hpT = 0.45f * x1 * x1 }
            SectionType.BREAK -> if (c.g.family == Family.MALADE) { lpF = 0.62f; lpT = 0.62f }
            else -> {}
        }
        return Bar(c.section, c.sectionIndex, c.bar, c.bars, c.voicing, chordName, Array(TrackId.COUNT) { tracks[it] }, lpF, lpT, hpF, hpT)
    }

    private fun activeTracks(c: Ctx): Set<TrackId> {
        val g = c.g; val e = c.p.energy; val cx = c.p.complexity
        val s = HashSet<TrackId>()
        val drums = g.groove != Groove.NONE
        val half = c.bar >= c.bars / 2
        when (c.section) {
            SectionType.INTRO -> {
                s += TrackId.CHORDS
                if (drums && (half || g.family == Family.MALADE)) s += TrackId.HATS
                if (drums && g.family == Family.MALADE && half && g.structure != Structure.RAP) s += TrackId.KICK
                if (g.family == Family.CHILL && half && drums) { s += TrackId.KICK; s += TrackId.BASS }
                if (!drums && half) s += TrackId.BASS
            }
            SectionType.VERSE -> {
                s += listOf(TrackId.BASS, TrackId.CHORDS)
                if (drums) s += listOf(TrackId.KICK, TrackId.SNARE, TrackId.HATS)
                if (e > 0.55f && drums) s += TrackId.PERC
                if (cx > 0.6f || g.structure == Structure.CHILL) s += TrackId.LEAD
                if (c.arpOn && e > 0.65f) s += TrackId.ARP
            }
            SectionType.CHORUS -> {
                s += listOf(TrackId.BASS, TrackId.CHORDS, TrackId.LEAD)
                if (drums) s += listOf(TrackId.KICK, TrackId.SNARE, TrackId.HATS)
                if (drums && e > 0.35f) s += TrackId.PERC
                if (!drums && e > 0.5f) s += TrackId.PERC
                if (c.arpOn) s += TrackId.ARP
            }
            SectionType.BUILD -> {
                s += listOf(TrackId.CHORDS, TrackId.SNARE, TrackId.HATS)
                if (!c.lastBar) s += TrackId.KICK
                if (c.arpOn || e > 0.6f) s += TrackId.ARP
                if (c.bar < c.bars / 2) s += TrackId.BASS
                if (cx > 0.5f) s += TrackId.LEAD
            }
            SectionType.DROP -> {
                s += listOf(TrackId.KICK, TrackId.SNARE, TrackId.HATS, TrackId.BASS, TrackId.CHORDS, TrackId.LEAD)
                if (e > 0.3f) s += TrackId.PERC
                if (c.arpOn) s += TrackId.ARP
            }
            SectionType.BREAK -> {
                s += listOf(TrackId.CHORDS, TrackId.LEAD)
                if (c.arpOn) s += TrackId.ARP
                if (g.family == Family.CHILL && drums) s += TrackId.HATS
                if (!drums) s += TrackId.BASS
            }
            SectionType.OUTRO -> {
                s += TrackId.CHORDS
                if (!half) { if (drums) { s += TrackId.KICK; s += TrackId.HATS }; s += TrackId.BASS }
                if (!drums) s += TrackId.BASS
            }
        }
        if (!c.p.withLead) s -= TrackId.LEAD
        if (!drums) { s -= TrackId.KICK; s -= TrackId.SNARE; s -= TrackId.HATS }
        // Hardstyle / hardcore : la basse inversée ne joue qu'avec le kick
        if (g.bassStyle == BassStyle.REVERSE && TrackId.KICK !in s) s -= TrackId.BASS
        return s
    }

    // ─────────────────────────────── Batterie ───────────────────────────────

    private fun hit(list: MutableList<NoteEv>, step: Float, vel: Float, note: Int = 0, len: Float = 1f) {
        if (step < 16f) list += NoteEv(step, note, vel.coerceIn(0.05f, 1f), len)
    }

    private fun pattern(list: MutableList<NoteEv>, pat: String, vel: Float, note: Int = 0, rng: Rng? = null, humanize: Float = 0f) {
        for (i in pat.indices) {
            val ch = pat[i]
            if (ch == '.') continue
            val v = when (ch) { 'X' -> vel; 'x' -> vel * 0.8f; 'o' -> vel * 0.45f; else -> vel * 0.6f }
            hit(list, i.toFloat(), v + (rng?.bipolar() ?: 0f) * humanize, note)
        }
    }

    private fun drums(c: Ctx, act: Set<TrackId>, t: Array<MutableList<NoteEv>>) {
        val g = c.g
        val kick = t[TrackId.KICK.ordinal]; val snr = t[TrackId.SNARE.ordinal]; val hat = t[TrackId.HATS.ordinal]; val prc = t[TrackId.PERC.ordinal]
        val rk = c.rng(TrackId.KICK, 4); val rs = c.rng(TrackId.SNARE, 2); val rh = c.rng(TrackId.HATS, 2); val rp = c.rng(TrackId.PERC, 2)
        val cx = c.p.complexity; val e = c.p.energy
        val fill = c.lastBar && c.bars >= 4 && c.section != SectionType.OUTRO

        if (c.section == SectionType.BUILD) {
            // Montée : roulement de caisse claire qui accélère
            val frac = (c.bar + 1f) / c.bars
            val interval = when { frac <= 0.25f -> 4f; frac <= 0.5f -> 2f; frac < 1f -> 1f; else -> 0.5f }
            var s = 0f
            while (s < 16f) {
                val step = s
                val v = 0.35f + 0.6f * ((c.bar + step / 16f) / c.bars)
                if (c.lastBar && step >= 8f) hit(snr, step, v, 0) else hit(snr, step, v, if (g.snare.kind == SnareKind.CLAP) 1 else 0)
                s += if (c.lastBar && step >= 8f) 0.5f else interval
            }
            if (TrackId.KICK in act) {
                val kInt = if (frac > 0.75f && g.family == Family.MALADE) 2f else 4f
                var k = 0f; while (k < 16f) { hit(kick, k, 0.9f, c.chordRoot); k += kInt }
            }
            var h = 0f; while (h < 16f) { hit(hat, h, 0.25f + 0.4f * frac); h += if (frac > 0.5f) 1f else 2f }
            return
        }

        if (TrackId.KICK in act) {
            when (g.groove) {
                Groove.BOOMBAP -> pattern(kick, rk.pick(listOf("X.........x.....", "X......x..x.....", "X.....x...x..x..", "X..x......x.....", "X.......X.x.....")), 0.95f, 0, rk, 0.05f)
                Groove.HOUSE, Groove.TECHNO, Groove.REGGAETON -> pattern(kick, "X...X...X...X...", 0.95f)
                Groove.SYNTHWAVE -> pattern(kick, if (c.section == SectionType.CHORUS || c.section == SectionType.DROP) "X...X...X...X..." else "X.......X.x.....", 0.95f)
                Groove.TRAP -> pattern(kick, rk.pick(listOf("X.....x...x.....", "X......x.x......", "X.........x..x..", "X..x.......x....", "X.....x.......x.")), 0.95f)
                Groove.DRILL -> pattern(kick, rk.pick(listOf("X.....x..x......", "X......x...x....", "X.........x.x...")), 0.95f)
                Groove.PHONK -> pattern(kick, rk.pick(listOf("X.....x.x.....x.", "X..x..x...x.....", "X.....X...x..x..")), 0.95f)
                Groove.DNB -> pattern(kick, rk.pick(listOf("X.........X.....", "X.........x..x..", "X......x..X.....", "X.x.......X.....")), 0.95f)
                Groove.DUBSTEP -> pattern(kick, rk.pick(listOf("X...............", "X.........x.....", "X..x............", "X.....x...x.....")), 1f)
                Groove.HARDSTYLE, Groove.HARDCORE -> {
                    for (b in 0 until 4) hit(kick, b * 4f, 1f, c.chordRoot)
                    if (fill && g.groove == Groove.HARDCORE) { hit(kick, 14f, 0.9f, c.chordRoot) }
                }
                Groove.NONE -> {}
            }
            if (fill && g.groove in listOf(Groove.HOUSE, Groove.TECHNO) && rk.chance(0.4f)) kick.removeAll { it.step >= 12f }
        }

        if (TrackId.SNARE in act) {
            val clap = if (g.snare.kind == SnareKind.CLAP) 0 else 1
            when (g.groove) {
                Groove.BOOMBAP -> {
                    hit(snr, 4f, 0.9f); hit(snr, 12f, 0.9f)
                    for (gs in listOf(7f, 9f, 15f, 10f)) if (rs.chance(0.18f + cx * 0.3f)) hit(snr, gs, 0.18f + rs.nextFloat() * 0.12f)
                    if (rs.chance(0.3f)) hit(snr, 8f + 2f, 0.3f, 2)
                }
                Groove.HOUSE, Groove.TECHNO -> {
                    if (c.section != SectionType.INTRO && (g.groove == Groove.HOUSE || c.section == SectionType.DROP || e > 0.6f)) { hit(snr, 4f, 0.85f); hit(snr, 12f, 0.85f) }
                    if (fill) { hit(snr, 14f, 0.5f); hit(snr, 15f, 0.7f) }
                }
                Groove.SYNTHWAVE -> { hit(snr, 4f, 0.95f); hit(snr, 12f, 0.95f); if (fill) for (s in 12..15) hit(snr, s.toFloat(), 0.5f + (s - 12) * 0.12f) }
                Groove.TRAP, Groove.PHONK -> {
                    hit(snr, 8f, 0.95f)
                    if (g.groove == Groove.PHONK && (c.section == SectionType.CHORUS || c.section == SectionType.DROP)) { hit(snr, 4f, 0.6f, clap); hit(snr, 12f, 0.6f, clap) }
                    if (fill && rs.chance(0.6f)) { hit(snr, 14f, 0.6f); hit(snr, 15f, 0.75f) }
                }
                Groove.DRILL -> {
                    hit(snr, 8f, 0.95f)
                    if (rs.chance(0.55f)) hit(snr, rs.pick(listOf(3f, 14f, 15f, 11f)), 0.7f)
                    if (fill) hit(snr, 13f, 0.6f)
                }
                Groove.DNB -> {
                    hit(snr, 4f, 1f); hit(snr, 12f, 1f)
                    for (gs in listOf(7f, 9f, 14f, 15f)) if (rs.chance(0.15f + cx * 0.35f)) hit(snr, gs, 0.22f)
                    if (fill) for (s in listOf(13f, 14f, 15f, 15.5f)) hit(snr, s, 0.7f)
                }
                Groove.DUBSTEP -> {
                    hit(snr, 8f, 1f)
                    if (fill) { for (s in 12..15) hit(snr, s.toFloat(), 0.55f + (s - 12) * 0.12f) }
                }
                Groove.HARDSTYLE, Groove.HARDCORE -> {
                    if (c.section == SectionType.DROP || c.section == SectionType.CHORUS) { hit(snr, 4f, 0.7f); hit(snr, 12f, 0.7f) }
                }
                Groove.REGGAETON -> {
                    // Dembow : 3-3-2
                    for (s in listOf(3f, 6f, 11f, 14f)) hit(snr, s, if (s == 3f || s == 11f) 0.8f else 0.7f)
                }
                Groove.NONE -> {}
            }
        }

        if (TrackId.HATS in act) {
            when (g.groove) {
                Groove.BOOMBAP, Groove.SYNTHWAVE -> {
                    for (s in 0 until 16 step 2) hit(hat, s.toFloat(), (if (s % 4 == 0) 0.7f else 0.5f) + rh.bipolar() * 0.08f)
                    for (s in 1 until 16 step 2) if (rh.chance(cx * 0.25f)) hit(hat, s.toFloat(), 0.25f)
                    if (rh.chance(0.35f)) { hat.removeAll { it.step == 14f }; hit(hat, 14f, 0.45f, 1) }
                }
                Groove.HOUSE, Groove.REGGAETON -> {
                    for (s in 0 until 16) {
                        if (s % 4 == 2) hit(hat, s.toFloat(), 0.6f, if (g.groove == Groove.HOUSE) 1 else 0)
                        else if (s % 2 == 1 || cx > 0.5f) hit(hat, s.toFloat(), 0.22f + rh.nextFloat() * 0.12f)
                    }
                }
                Groove.TECHNO -> {
                    for (s in 0 until 16) {
                        if (s % 4 == 2) hit(hat, s.toFloat(), 0.65f, 1) else hit(hat, s.toFloat(), 0.2f + (if (s % 2 == 1) 0.1f else 0f))
                    }
                    if (c.section == SectionType.DROP) for (s in 0 until 16 step 4) hit(hat, s.toFloat() + 0.01f, 0.3f, 2)
                }
                Groove.TRAP, Groove.PHONK -> {
                    val base = if (cx > 0.4f || g.groove == Groove.PHONK) 1f else 2f
                    var s = 0f
                    val rollAt = if (rh.chance(0.4f + cx * 0.5f)) rh.pick(listOf(6f, 10f, 12f, 14f)) else -1f
                    val rollType = rh.nextInt(3)
                    while (s < 16f) {
                        if (rollAt >= 0f && s >= rollAt && s < rollAt + 2f) {
                            val div = when (rollType) { 0 -> 0.5f; 1 -> 2f / 3f; else -> 0.25f }
                            var r = s
                            while (r < rollAt + 2f - 0.001f) { hit(hat, r, 0.35f + 0.4f * ((r - rollAt) / 2f)); r += div }
                            s = rollAt + 2f
                            continue
                        }
                        hit(hat, s, if (s % 4f == 0f) 0.65f else 0.45f)
                        s += base
                    }
                    if (rh.chance(0.25f)) hit(hat, 7f, 0.4f, 1)
                }
                Groove.DRILL -> {
                    // triolets « skippy »
                    val skip = rh.nextInt(4)
                    for (i in 0 until 12) {
                        if (i % 4 == skip && rh.chance(0.7f)) continue
                        hit(hat, i * 4f / 3f, if (i % 3 == 0) 0.6f else 0.38f)
                    }
                    if (rh.chance(0.4f)) { var r = 12f; while (r < 14f) { hit(hat, r, 0.4f); r += 0.5f } }
                }
                Groove.DNB -> {
                    for (s in 0 until 16 step 2) hit(hat, s.toFloat() + 1f, 0.5f)
                    for (s in 0 until 16 step 2) if (rh.chance(0.3f + cx * 0.4f)) hit(hat, s.toFloat(), 0.25f)
                    if (c.section == SectionType.DROP) for (s in 0 until 16 step 4) hit(hat, s.toFloat() + 0.01f, 0.28f, 2)
                }
                Groove.DUBSTEP -> {
                    for (s in 0 until 16 step 2) hit(hat, s.toFloat(), if (s % 4 == 0) 0.55f else 0.4f)
                    if (rh.chance(0.5f)) { var r = 12f; while (r < 16f - 0.01f) { hit(hat, r, 0.35f); r += 2f / 3f } ; hat.removeAll { it.step == 12f || it.step == 14f } }
                }
                Groove.HARDSTYLE, Groove.HARDCORE -> {
                    for (b in 0 until 4) hit(hat, b * 4f + 2f, 0.6f, 1)
                    if (cx > 0.5f) for (s in 0 until 16) if (s % 2 == 1) hit(hat, s.toFloat(), 0.2f)
                }
                Groove.NONE -> {}
            }
        }

        if (TrackId.PERC in act) {
            when (g.groove) {
                Groove.BOOMBAP -> for (s in 0 until 16) if (s % 2 == 1 && rp.chance(0.4f)) hit(prc, s.toFloat(), 0.3f, 0)
                Groove.HOUSE, Groove.TECHNO -> {
                    for (s in 0 until 16) if (rp.chance(0.12f + cx * 0.18f)) hit(prc, s.toFloat(), 0.45f, rp.pick(intArrayOf(2, 5, 6, 0)))
                    for (s in 0 until 16) hit(prc, s.toFloat() + 0.02f, if (s % 2 == 0) 0.18f else 0.28f, 0)
                }
                Groove.SYNTHWAVE -> for (s in 0 until 16 step 4) hit(prc, s.toFloat() + 2f, 0.4f, 0)
                Groove.REGGAETON -> for (s in 0 until 16) if (rp.chance(0.3f)) hit(prc, s.toFloat(), 0.5f, rp.pick(intArrayOf(2, 5)))
                Groove.DNB -> for (s in 0 until 16) hit(prc, s.toFloat(), if (s % 2 == 1) 0.3f else 0.18f, 0)
                Groove.NONE -> for (s in 0 until 16 step 2) if (rp.chance(0.5f)) hit(prc, s.toFloat(), 0.15f, 0)
                else -> if (rp.chance(0.5f)) hit(prc, rp.pick(listOf(3f, 7f, 11f, 15f)), 0.35f, rp.pick(intArrayOf(6, 5)))
            }
        }

        // Crash au début des sections fortes
        val strong = c.section == SectionType.DROP || (c.section == SectionType.CHORUS && g.family == Family.MALADE)
        if (strong && c.bar == 0 && g.groove != Groove.NONE) hit(prc, 0f, 0.8f, 4)
    }

    // ─────────────────────────────── Basse ───────────────────────────────

    private fun bass(c: Ctx, t: Array<MutableList<NoteEv>>) {
        val g = c.g
        val out = t[TrackId.BASS.ordinal]
        val r = c.rng(TrackId.BASS, 2)
        val root = c.chordRoot
        val fifth = c.scale.note(36 + c.rootPc, c.degree + 4).let { if (it - root > 12) it - 12 else it }
        val cx = c.p.complexity
        fun n(step: Float, note: Int, len: Float, vel: Float = 0.85f, slide: Boolean = false, accent: Boolean = false, mod: Float = 0f) {
            if (step < 16f) out += NoteEv(step, note, vel, len, slide, accent, mod)
        }
        val kicks = t[TrackId.KICK.ordinal].map { it.step }

        when (g.bassStyle) {
            BassStyle.DRONE -> if (c.newChord) n(0f, root, 16f * c.chordBars - 0.5f, 0.6f)
            BassStyle.LOFI -> {
                val pat = r.pick(listOf(listOf(0f to 6f, 10f to 5f), listOf(0f to 3f, 3f to 3f, 10f to 6f), listOf(0f to 7f, 7f to 2f, 10f to 4f), listOf(0f to 10f, 14f to 2f)))
                for ((i, pr) in pat.withIndex()) {
                    val note = if (i == pat.lastIndex && pat.size > 2 && r.chance(0.4f)) fifth else root
                    n(pr.first, note, pr.second, 0.8f)
                }
            }
            BassStyle.WALKING -> {
                val tones = listOf(root, c.scale.note(36 + c.rootPc, c.degree + 2), fifth)
                n(0f, root, 3.5f, 0.85f)
                n(4f, r.pick(tones), 3.5f, 0.7f)
                n(8f, r.pick(tones), 3.5f, 0.75f)
                val target = c.nextRoot
                val approach = target + (if (r.chance(0.5f)) -1 else 1) * (if (c.scale.contains(c.rootPc, target - 1)) 1 else 2)
                n(12f, if (cx > 0.4f) approach else r.pick(tones), 3.5f, 0.7f)
                if (cx > 0.6f && r.chance(0.5f)) n(10f, fifth, 1.5f, 0.45f)
            }
            BassStyle.OFFBEAT -> {
                for (s in listOf(2f, 6f, 10f, 14f)) n(s, if (r.chance(0.15f + cx * 0.2f)) root + 12 else root, 1.6f, 0.85f)
                if (cx > 0.55f) { n(7f, root, 0.8f, 0.5f); n(15f, root + 12, 0.8f, 0.5f) }
            }
            BassStyle.EIGHTHS -> for (s in 0 until 16 step 2) n(s.toFloat(), if (s == 14 && r.chance(0.4f)) root + 12 else root, 1.6f, if (s % 4 == 0) 0.9f else 0.7f)
            BassStyle.ROLLING -> for (s in 0 until 16) if (s % 4 != 0) n(s.toFloat(), root, 0.9f, 0.75f)
            BassStyle.ACID -> {
                val ar = c.rng(TrackId.BASS, 4, 11)
                val pool = intArrayOf(0, 0, 0, 12, 0, 7, 3, 10, 12, 15).map { root + it }.filter { c.scale.contains(c.rootPc, it) || it == root }
                var lastOn = false
                for (s in 0 until 16) {
                    if (!ar.chance(0.68f + cx * 0.2f)) { lastOn = false; continue }
                    val slide = lastOn && ar.chance(0.22f)
                    n(s.toFloat(), ar.pick(pool), if (ar.chance(0.2f)) 2f else 0.9f, 0.85f, slide = slide, accent = ar.chance(0.28f))
                    lastOn = true
                }
            }
            BassStyle.B808 -> {
                val ks = if (kicks.isEmpty()) listOf(0f, 10f) else kicks
                val slides = g.groove == Groove.DRILL || cx > 0.5f
                for ((i, s) in ks.withIndex()) {
                    val next = if (i + 1 < ks.size) ks[i + 1] else 16f
                    var note = root
                    var slide = false
                    if (i > 0 && slides && r.chance(if (g.groove == Groove.DRILL) 0.45f else 0.25f)) {
                        note = root + r.pick(intArrayOf(12, 7, -5, 3, 10)).let { if (c.scale.contains(c.rootPc, root + it)) it else 12 }
                        slide = true
                    }
                    n(s, note, (next - s).coerceAtLeast(1f) - 0.1f, 0.95f, slide = slide)
                }
                if (slides && r.chance(0.3f)) n(14f, root + 12, 2f, 0.85f, slide = true)
            }
            BassStyle.REESE -> {
                n(0f, root, 7.8f, 0.9f)
                n(8f, if (r.chance(0.3f)) fifth else root, 5.8f, 0.85f, slide = true)
                if (cx > 0.4f) n(14f, root + 12, 1.8f, 0.8f, slide = true)
            }
            BassStyle.WOBBLE -> {
                if (c.section == SectionType.DROP) {
                    val wr = c.rng(TrackId.BASS, 2, 5)
                    var s = 0f
                    val rates = floatArrayOf(0.25f, 0.5f, 1f, 0.375f, 0.125f, 0.75f)
                    while (s < 16f) {
                        val len = wr.pick(floatArrayOf(2f, 4f, 4f, 2f, 6f, 1f).toTypedArray())
                        if (wr.chance(0.12f) && s > 0f) { s += 1f; continue }
                        val note = if (wr.chance(0.25f)) root + wr.pick(intArrayOf(1, 3, 7, 12, -2)) else root
                        n(s, note, min(len, 16f - s) - 0.05f, 0.95f, mod = wr.pick(rates.toTypedArray()))
                        s += len
                    }
                } else if (c.newChord) n(0f, root, 16f * c.chordBars - 0.3f, 0.6f, mod = 0.0625f)
            }
            BassStyle.REVERSE -> for (b in 0 until 4) n(b * 4f + 1.2f, root, 2.6f, 0.9f)
            BassStyle.DEMBOW -> {
                n(0f, root, 2.8f, 0.9f); n(3f, root, 1.5f, 0.7f); n(6f, fifth, 1.8f, 0.75f)
                n(8f, root, 2.8f, 0.9f); n(11f, root, 1.5f, 0.7f); n(14f, if (r.chance(0.5f)) root + 12 else fifth, 1.8f, 0.75f)
            }
        }
    }

    // ─────────────────────────────── Accords ───────────────────────────────

    private fun chords(c: Ctx, t: Array<MutableList<NoteEv>>) {
        val out = t[TrackId.CHORDS.ordinal]
        val r = c.rng(TrackId.CHORDS, 2)
        val v = c.voicing
        fun chord(step: Float, len: Float, vel: Float, strum: Float = 0f) {
            for ((i, nn) in v.withIndex()) if (step + i * strum < 16f) out += NoteEv(step + i * strum, nn, vel * (1f - i * 0.04f), len)
        }
        val style = when {
            c.g.chordStyle == ChordStyle.PUMP && c.section != SectionType.DROP && c.section != SectionType.CHORUS -> ChordStyle.STABS
            else -> c.g.chordStyle
        }
        when (style) {
            ChordStyle.SUSTAIN -> if (c.newChord) chord(0f, 16f * c.chordBars - 0.3f, 0.7f)
            ChordStyle.PUMP -> chord(0f, 15.7f, 0.75f)
            ChordStyle.LOFI_COMP -> {
                val pat = r.pick(listOf(listOf(0f to 9f, 10f to 5.5f), listOf(0f to 15f), listOf(0f to 6f, 6f to 3f, 11f to 4.5f), listOf(-0.5f to 8f, 8f to 7.5f)))
                for ((s, len) in pat) chord(max(0f, s), len, 0.62f + r.nextFloat() * 0.12f, 0.12f)
            }
            ChordStyle.STABS -> {
                val pat = r.pick(listOf("..x...x...x...x.", "x..x..x...x..x..", "..x..x....x..x..", "...x..x....x..x.", "x.....x.....x..."))
                for (i in pat.indices) if (pat[i] == 'x') chord(i.toFloat(), 0.9f, 0.7f)
            }
            ChordStyle.PLUCKS -> {
                val pat = r.pick(listOf("x..x..x.x..x..x.", "x..x..x...x.x...", "..x..x..x..x..x."))
                for (i in pat.indices) if (pat[i] == 'x') chord(i.toFloat(), 1.2f, 0.65f, 0.03f)
            }
            ChordStyle.BROKEN -> {
                out += NoteEv(0f, v[0] - 12, 0.55f, 16f)
                val order = r.pick(listOf(intArrayOf(0, 1, 2, 1, 2, 3, 2, 1), intArrayOf(0, 2, 1, 3, 2, 1, 2, 3), intArrayOf(0, 1, 2, 3, 2, 3, 1, 2)))
                for (k in 0 until 8) {
                    if (k > 0 && r.chance(0.12f)) continue
                    val nn = v[order[k] % v.size] + if (order[k] >= v.size) 12 else 0
                    out += NoteEv(k * 2f, nn, 0.42f + r.nextFloat() * 0.12f, 6f)
                }
            }
        }
    }

    // ─────────────────────────────── Mélodie ───────────────────────────────

    private val FESTIVAL_RHYTHMS = listOf("x..x..x...x..x..", "x..x..x.x..x..x.", "x.x..x..x.x..x..", "x...x.x...x.x...", "x..x...x..x.x...")

    private fun lead(c: Ctx, t: Array<MutableList<NoteEv>>) {
        val g = c.g
        val out = t[TrackId.LEAD.ordinal]
        // La pause reprend le motif du drop / refrain (même graine) : effet « teaser »
        val motifSection = if (c.section == SectionType.BREAK || c.section == SectionType.BUILD) (if (g.structure == Structure.CLUB || g.structure == Structure.FESTIVAL) SectionType.DROP else SectionType.CHORUS) else c.section
        val period = if (c.p.complexity > 0.5f) 4 else 2
        val pos = if (c.lastBar && c.bars > 2 && c.section != SectionType.BREAK) 99 else c.bar % period
        val r = Rng(c.p.seed * 7_919L + motifSection.ordinal * 104_729L + pos * 131L + 3)
        val soft = c.section == SectionType.BREAK || c.section == SectionType.BUILD || c.section == SectionType.VERSE
        val sectionDensity = when (c.section) { SectionType.VERSE -> 0.55f; SectionType.BREAK -> 0.6f; SectionType.BUILD -> 0.5f; else -> 1f }
        val density = (g.leadDensity * (0.6f + 0.8f * c.p.complexity) * sectionDensity).coerceIn(0.05f, 0.95f)
        val velBase = if (soft) 0.55f else 0.8f
        val pentatonic = g.family == Family.CHILL
        fun okDeg(d: Int): Boolean {
            if (!pentatonic) return true
            val abs = Math.floorMod(c.degree + d, 7)
            return !(c.scale.minor && (abs == 1 || abs == 5)) && !(!c.scale.minor && (abs == 3 || abs == 6))
        }

        val festival = g.structure == Structure.FESTIVAL || g.id == "edm"
        if (festival && (c.section == SectionType.DROP || c.section == SectionType.BREAK)) {
            val rhythm = Rng(c.p.seed * 3 + 17).pick(FESTIVAL_RHYTHMS)
            val hits = rhythm.indices.filter { rhythm[it] == 'x' }
            val contour = IntArray(hits.size) { r.pick(intArrayOf(0, 2, 4, 4, 7, 2, 5)) }
            for ((k, s) in hits.withIndex()) {
                val next = if (k + 1 < hits.size) hits[k + 1] else 16
                out += NoteEv(s.toFloat(), c.note(contour[k], 60), velBase, (next - s).toFloat() - 0.15f, slide = false)
            }
            return
        }

        val grid = if (density > 0.45f || g.id == "phonk") 1f else 2f
        var deg = r.pick(intArrayOf(0, 2, 4, 7))
        var s = 0f
        var lastStart = -1f
        while (s < 16f) {
            val strong = s % 4f == 0f
            if (r.chance(density * (if (strong) 1.25f else 0.8f))) {
                deg += r.pick(intArrayOf(-2, -1, -1, 0, 1, 1, 2, 3, -3, 1))
                if (deg < -2) deg += 4; if (deg > 9) deg -= 4
                if (strong) {
                    val tones = intArrayOf(0, 2, 4, 7, -3, 9)
                    deg = tones.minBy { kotlin.math.abs(it - deg) }
                }
                var tries = 0
                while (!okDeg(deg) && tries < 4) { deg += 1; tries++ }
                val maxLen = if (grid == 1f) r.pick(floatArrayOf(1f, 1f, 2f, 3f).toTypedArray()) else r.pick(floatArrayOf(2f, 2f, 4f, 3f).toTypedArray())
                out += NoteEv(s, c.note(deg, 60), velBase + r.bipolar() * 0.08f, maxLen * 0.9f, slide = lastStart >= 0f && r.chance(0.15f))
                lastStart = s
            }
            s += grid
        }
        // fin de phrase : note longue sur un ton de l'accord
        if ((c.bar % 2 == 1 || c.lastBar) && out.isNotEmpty() && r.chance(0.7f)) {
            out.removeAll { it.step >= 12f }
            out += NoteEv(12f, c.note(r.pick(intArrayOf(0, 2, 4)), 60), velBase, 3.6f)
        }
    }

    private fun arp(c: Ctx, t: Array<MutableList<NoteEv>>) {
        val out = t[TrackId.ARP.ordinal]
        val r = Rng(c.p.seed * 13 + 5)
        val mode = r.nextInt(4)
        val rate = if (c.g.family == Family.CHILL && c.g.bpmMax < 100) 2f else if (c.g.bpmMin >= 165) 2f else 1f
        val v = c.voicing
        val notes = (v.toList() + v.map { it + 12 }).sorted()
        val seq = when (mode) {
            0 -> notes
            1 -> notes.reversed()
            2 -> notes + notes.reversed().drop(1).dropLast(1)
            else -> notes.indices.map { notes[(it * 3) % notes.size] }
        }
        var i = 0; var s = 0f
        while (s < 16f) {
            out += NoteEv(s, seq[i % seq.size], if (s % 4f == 0f) 0.75f else 0.55f, rate * 0.85f)
            i++; s += rate
        }
    }

    // ─────────────────────────────── Effets ───────────────────────────────

    private fun fx(c: Ctx, t: Array<MutableList<NoteEv>>) {
        val out = t[TrackId.FX.ordinal]
        val g = c.g
        when (c.section) {
            SectionType.BUILD -> {
                if (c.bar == 0) out += NoteEv(0f, 0, 0.9f, 16f * c.bars)
                if (c.lastBar) out += NoteEv(0f, 3, 0.8f, 16f)
            }
            SectionType.DROP -> if (c.bar == 0) out += NoteEv(0f, 1, 1f, 16f)
            SectionType.CHORUS -> {
                if (c.bar == 0 && !c.isFirstSection) out += NoteEv(0f, 1, if (g.family == Family.MALADE) 0.8f else 0.35f, 16f)
                if (c.lastBar && g.family == Family.MALADE) out += NoteEv(8f, 3, 0.6f, 8f)
            }
            SectionType.BREAK -> if (c.bar == 0) out += NoteEv(0f, 2, 0.6f, 32f)
            SectionType.VERSE -> if (c.lastBar && g.family == Family.MALADE) out += NoteEv(8f, 3, 0.5f, 8f)
            else -> {}
        }
    }
}
