package com.superessence.voxea

import com.superessence.voxea.core.Line
import com.superessence.voxea.core.VoiceSettings
import com.superessence.voxea.core.VoiceType
import com.superessence.voxea.data.Project
import org.junit.Assert.assertEquals
import org.junit.Test

class ProjectTest {
    private val lines = listOf(
        Line(0, "LÉA", "a", VoiceType.FEMME), Line(1, "MARC", "b"), Line(2, "LÉA", "c"), Line(3, "NARRATEUR", "d"),
    )

    @Test fun castFromHints() {
        val p = Project.create("Test", "raw", lines)
        assertEquals(listOf("LÉA", "MARC", "NARRATEUR"), p.characters)
        assertEquals(VoiceType.FEMME, p.voiceTypeFor(lines[2]))
        assertEquals(VoiceType.ORIGINAL, p.voiceTypeFor(lines[1]))
    }

    @Test fun overridePriority() {
        var p = Project.create("Test", "raw", lines)
        p = p.withCharacterVoice("MARC", VoiceType.HOMME)
        assertEquals(VoiceType.HOMME, p.voiceTypeFor(lines[1]))
        p = p.withLineVoice(2, VoiceType.ENFANT)
        assertEquals(VoiceType.ENFANT, p.voiceTypeFor(lines[2]))
        assertEquals(VoiceType.FEMME, p.voiceTypeFor(lines[0]))
        p = p.withCharacterSettings("LÉA", VoiceSettings(3f, 1.1f))
        assertEquals(VoiceSettings(3f, 1.1f), p.settingsFor(lines[0]))
    }

    @Test fun jsonRoundTrip() {
        val p = Project.create("Mon dialogue", "raw", lines).withLineVoice(1, VoiceType.ROBOT)
        assertEquals(p, Project.fromJson(p.toJson()))
    }

    @Test fun reparseKeepsTakesWithTheirLines() {
        val root = java.nio.file.Files.createTempDirectory("voxea").toFile()
        val store = com.superessence.voxea.data.ProjectStore(root)
        val old = Project.create("T", "", lines)
        lines.forEach { store.recFile(old, it.index).apply { parentFile!!.mkdirs(); writeText("take${it.index}" + "x".repeat(60)) } }
        // une réplique insérée au début, la réplique « b » supprimée
        val newLines = listOf(Line(0, "LÉA", "nouvelle"), Line(1, "LÉA", "a"), Line(2, "LÉA", "c"), Line(3, "NARRATEUR", "d"))
        val fresh = old.copy(lines = newLines)
        store.remapRecordings(old, fresh)
        org.junit.Assert.assertFalse(store.hasRecording(fresh, 0))
        assertEquals("take0", store.recFile(fresh, 1).readText().take(5))
        assertEquals("take2", store.recFile(fresh, 2).readText().take(5))
        assertEquals("take3", store.recFile(fresh, 3).readText().take(5))
    }

    @Test fun accidentalEmptyTakeDoesNotReplaceGoodOne() {
        val sr = 44100
        val blip = FloatArray(sr / 20) { 0.001f }
        val speech = FloatArray(sr) { (0.3 * kotlin.math.sin(it * 0.05)).toFloat() }
        org.junit.Assert.assertFalse(com.superessence.voxea.core.Audio.shouldKeepTake(blip, sr, replacing = true))
        org.junit.Assert.assertTrue(com.superessence.voxea.core.Audio.shouldKeepTake(blip, sr, replacing = false))
        org.junit.Assert.assertTrue(com.superessence.voxea.core.Audio.shouldKeepTake(speech, sr, replacing = true))
    }
}
