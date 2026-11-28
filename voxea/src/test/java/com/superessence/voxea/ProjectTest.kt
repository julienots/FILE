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
}
