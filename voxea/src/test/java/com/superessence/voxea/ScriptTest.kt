package com.superessence.voxea

import com.superessence.voxea.core.ScriptParser
import com.superessence.voxea.core.VoiceType
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class ScriptTest {
    @Test fun simpleLine() {
        val l = ScriptParser.parse("LÉA : Salut toi").single()
        assertEquals("LÉA", l.character); assertEquals("Salut toi", l.text); assertNull(l.voiceHint)
    }
    @Test fun bracketTag() {
        val l = ScriptParser.parse("MARC [HOMME]: Ok").single()
        assertEquals("MARC", l.character); assertEquals(VoiceType.HOMME, l.voiceHint); assertEquals("Ok", l.text)
    }
    @Test fun parenTag() {
        val l = ScriptParser.parse("LÉA (femme) : x").single()
        assertEquals("LÉA", l.character); assertEquals(VoiceType.FEMME, l.voiceHint)
    }
    @Test fun leadingTagInText() {
        val l = ScriptParser.parse("[ENFANT] Tom : Coucou").single()
        assertEquals("Tom", l.character); assertEquals(VoiceType.ENFANT, l.voiceHint)
    }
    @Test fun plainParagraphsBecomeNarrator() {
        val ls = ScriptParser.parse("Il était une fois.\nUne forêt.\n\nPuis la nuit tomba.")
        assertEquals(2, ls.size); assertEquals("NARRATEUR", ls[0].character)
        assertEquals("Il était une fois. Une forêt.", ls[0].text); assertEquals(1, ls[1].index)
    }
    @Test fun colonInsideSentenceIsNotAName() {
        val l = ScriptParser.parse("Alors il se retourna et lui dit doucement : non.").single()
        assertEquals("NARRATEUR", l.character)
        assertEquals("Alors il se retourna et lui dit doucement : non.", l.text)
    }
    @Test fun colonInText() {
        val l = ScriptParser.parse("LÉA : Il a dit : non.").single()
        assertEquals("LÉA", l.character); assertEquals("Il a dit : non.", l.text)
    }
    @Test fun stageDirectionsKeptAndMarkdownStripped() {
        val ls = ScriptParser.parse("**MARC** : (rire) Vraiment ?\n- LÉA — Oui.")
        assertEquals("MARC", ls[0].character); assertEquals("(rire) Vraiment ?", ls[0].text)
        assertEquals("LÉA", ls[1].character); assertEquals("Oui.", ls[1].text)
    }
    @Test fun continuationLineJoinsPreviousReplica() {
        val ls = ScriptParser.parse("LÉA : Bonjour.\nComment vas-tu ?\nMARC : Bien.")
        assertEquals(2, ls.size); assertEquals("Bonjour. Comment vas-tu ?", ls[0].text)
    }
    @Test fun titlesAndEmptyIgnored() {
        val ls = ScriptParser.parse("# Scène 1\n\nLÉA : A\n---\nMARC : B")
        assertEquals(listOf("LÉA", "MARC"), ls.map { it.character })
    }
    @Test fun voiceKeywords() {
        assertEquals(VoiceType.AGE, ScriptParser.parse("PAPI [VIEUX] : a").single().voiceHint)
        assertEquals(VoiceType.ROBOT, ScriptParser.parse("R2 (robot) : a").single().voiceHint)
        assertEquals(VoiceType.ORIGINAL, ScriptParser.parse("MOI [MA VOIX] : a").single().voiceHint)
        assertEquals(VoiceType.AGE, ScriptParser.parse("PAPI JO [ÂGÉ] : a").single().voiceHint)
        assertEquals(VoiceType.AGE, ScriptParser.parse("MAMIE (âgée) : a").single().voiceHint)
        assertEquals(VoiceType.FEMME, ScriptParser.parse("ÉLODIE [FÉMININE] : a").single().voiceHint)
        assertEquals(VoiceType.ENFANT, ScriptParser.parse("BÉBÉ [BÉBÉ] : a").single().voiceHint)
    }
}
