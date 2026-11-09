package com.superessence.noctea

import com.superessence.noctea.data.Catalog
import com.superessence.noctea.data.Discovery
import com.superessence.noctea.data.Seg
import com.superessence.noctea.data.Sounds
import com.superessence.noctea.generator.Ambience
import com.superessence.noctea.generator.DURATIONS
import com.superessence.noctea.generator.GeneratorParams
import com.superessence.noctea.generator.Goal
import com.superessence.noctea.generator.MeditationGenerator
import com.superessence.noctea.generator.MusicIntensity
import com.superessence.noctea.generator.NarrationStyle
import com.superessence.noctea.generator.PhraseBank
import com.superessence.noctea.tts.AudioFiles
import com.superessence.noctea.tts.TextComposer
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.BeforeClass
import org.junit.Test
import java.io.File
import java.time.LocalDate

class ContentTest {
    companion object {
        lateinit var catalog: Catalog
        val assets = File("src/main/assets")

        @BeforeClass @JvmStatic
        fun load() {
            catalog = Catalog.parse(File(assets, "content/catalog.json").readText())
        }
    }

    private fun generator() = MeditationGenerator(PhraseBank { catalog.bank[it].orEmpty() }, catalog.fx)

    @Test fun catalogHasEveryRequestedCategoryAndContent() {
        val cats = catalog.categories.map { it.id }
        listOf("sommeil", "relaxation", "respiration", "meditation", "histoires").forEach { assertTrue(it in cats) }
        assertEquals(6, catalog.inCategory("sommeil").size)
        assertEquals(5, catalog.inCategory("relaxation").size)
        assertEquals(5, catalog.inCategory("respiration").size)
        assertEquals(5, catalog.inCategory("meditation").size)
        assertEquals(6, catalog.inCategory("histoires").size)
    }

    @Test fun everyClipFileExistsAndTimelinesAddUp() {
        catalog.tracks.forEach { t ->
            assertTrue("${t.id} vide", t.segments.isNotEmpty())
            assertEquals("durée ${t.id}", t.durationMs, t.segments.sumOf { it.ms })
            t.segments.forEach { s ->
                when (s) {
                    is Seg.Clip -> assertTrue("clip manquant ${s.id}", File(assets, "voice/${s.id}.ogg").exists())
                    is Seg.Fx -> assertTrue(File(assets, "voice/${s.name}.ogg").exists())
                    else -> {}
                }
            }
            t.ambience.forEach { assertTrue("son inconnu ${it.sound}", Sounds.get(it.sound) != null) }
            assertTrue("${t.id} trop court", t.durationMs > 3 * 60_000)
        }
        assertTrue(File(assets, "voice/silence.ogg").exists())
        Sounds.all.forEach { assertTrue("ambiance manquante ${it.id}", File(assets, "sounds/${it.id}.ogg").exists()) }
    }

    @Test fun breathingBlocksAreConsistent() {
        val coherence = catalog.track("breath_coherence")!!
        assertTrue(coherence.breath.isNotEmpty())
        coherence.breath.forEach { b ->
            assertEquals(10_000, b.cycle)
            assertEquals(b.cycle * b.count, b.end - b.start)
        }
        val t478 = catalog.track("breath_478")!!
        assertEquals(19_000, t478.breath.first().cycle)
    }

    @Test fun generatorHitsExactDurationForAllCombinations() {
        val g = generator()
        var n = 0
        for (goal in Goal.entries) for (amb in Ambience.entries) for (style in NarrationStyle.entries) {
            val minutes = DURATIONS[(goal.ordinal + amb.ordinal + style.ordinal) % DURATIONS.size]
            val m = g.generate(GeneratorParams(goal, minutes, amb, style, MusicIntensity.LIGHT, seed = n.toLong()))
            assertEquals("$goal $amb $style $minutes", minutes * 60_000L, m.durationMs)
            assertTrue(m.segments.none { it.ms < 0 })
            assertTrue("parole insuffisante $goal $style", m.paragraphs.size >= 6)
            assertTrue(m.speechMs < m.durationMs)
            n++
        }
        for (d in DURATIONS) for (style in NarrationStyle.entries) {
            val m = g.generate(GeneratorParams(Goal.SLEEP, d, Ambience.OCEAN, style))
            assertEquals(d * 60_000L, m.durationMs)
        }
    }

    @Test fun generatorIsCoherentAndDeterministic() {
        val g = generator()
        val p = GeneratorParams(Goal.ANXIETY, 15, Ambience.FOREST, NarrationStyle.POETIC, MusicIntensity.MODERATE, 42)
        val a = g.generate(p)
        val b = g.generate(p)
        assertEquals(a.segments, b.segments)
        // commence par l'intention de l'objectif, finit par une conclusion
        assertEquals(catalog.bank["intro.anxiety"]!!.first().text, a.paragraphs.first())
        assertTrue(a.paragraphs.any { t -> catalog.bank["amb.forest"]!!.any { it.text == t } })
        assertTrue(a.paragraphs.any { t -> catalog.bank["poetic"]!!.any { it.text == t } })
        assertTrue(catalog.bank["close.soft"]!!.any { it.text == a.paragraphs.last() })
        assertTrue(a.ambience.any { it.sound == "wind_trees" })
        assertTrue(a.ambience.any { it.sound == "piano" && it.volume == MusicIntensity.MODERATE.volume })
        val sleep = g.generate(GeneratorParams(Goal.SLEEP, 30, Ambience.RAIN, NarrationStyle.MINIMAL))
        assertTrue(catalog.bank["close.sleep"]!!.any { it.text == sleep.paragraphs.last() })
        // guidage minimal : beaucoup de silence
        assertTrue(sleep.speechMs < sleep.durationMs * 0.25)
    }

    @Test fun textComposerCreatesRealPauses() {
        val f = TextComposer.split("Bonsoir... ferme les yeux.\n\nRespire. [pause 10] Voilà ?", 1f)
        assertEquals(listOf("Bonsoir...", "ferme les yeux.", "Respire.", "Voilà ?"), f.map { it.text })
        assertEquals(1250L, f[0].pauseAfterMs)
        assertEquals(2200L + 3500L, f[1].pauseAfterMs)
        assertEquals(2200L + 10_000L, f[2].pauseAfterMs)
    }

    @Test fun searchUnderstandsAccentsAndSynonyms() {
        assertEquals("story_ocean", Discovery.search(catalog, "ocean").first().track.id.let { if (it.contains("ocean")) "story_ocean" else it })
        assertTrue(Discovery.search(catalog, "dormir").take(5).any { it.track.category == "sommeil" })
        assertTrue(Discovery.search(catalog, "stress").any { it.track.id == "breath_antistress" })
        assertTrue(Discovery.search(catalog, "cohérence").first().track.id == "breath_coherence")
        assertTrue(Discovery.search(catalog, "x").isEmpty())
    }

    @Test fun streakIsGentle() {
        val today = LocalDate.of(2026, 11, 9)
        val days = mapOf("2026-11-08" to 120_000L, "2026-11-07" to 90_000L, "2026-11-05" to 600_000L)
        assertEquals(2, Discovery.streak(days, today))
        assertEquals(3, Discovery.streak(days + ("2026-11-09" to 61_000L), today))
    }

    @Test fun wavDurationParsing() {
        val f = File.createTempFile("noctea", ".wav")
        val sr = 22050; val n = sr * 2
        val data = ByteArray(n * 2)
        val header = java.nio.ByteBuffer.allocate(44).order(java.nio.ByteOrder.LITTLE_ENDIAN).apply {
            put("RIFF".toByteArray()); putInt(36 + data.size); put("WAVE".toByteArray())
            put("fmt ".toByteArray()); putInt(16); putShort(1); putShort(1); putInt(sr); putInt(sr * 2); putShort(2); putShort(16)
            put("data".toByteArray()); putInt(data.size)
        }.array()
        f.writeBytes(header + data)
        assertEquals(2000L, AudioFiles.wavDuration(f))
        f.delete()
    }
}
