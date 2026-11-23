package com.fifi.assistant

import org.junit.Assert.*
import org.junit.Test
import java.time.LocalDateTime
import kotlin.random.Random

class BrainTest {
    private val cats = listOf(
        "oui", "flemme", "flemme_ok", "va_chercher", "incompris", "silence", "bonjour", "ca_va", "qui", "merci",
        "aurevoir", "amour", "insulte", "compliment", "blague", "rire", "meteo", "musique", "chanter", "youtube",
        "lampe_on", "lampe_off", "appeler", "photo", "age", "manger", "fatigue", "ennui", "calcul", "stop",
        "pile", "face", "de_intro", "pileface_intro", "heure_intro", "heure_outro", "date_intro", "premier",
    )
    private val bank = VoiceBank(
        cats.associateWith { c -> listOf(Line("$c/0.ogg", c)) } +
            mapOf(
                "nombre" to (0..55).map { Line("nombre/$it.ogg", "$it") },
                "heure" to (0..23).map { Line("heure/$it.ogg", "${it}h") },
                "jour" to (0..6).map { Line("jour/$it.ogg", "j$it") },
                "mois" to (0..11).map { Line("mois/$it.ogg", "m$it") },
            )
    )

    private fun brain() = Brain(bank, Random(1), { LocalDateTime.of(2026, 11, 23, 15, 22) }).apply {
        flemmeRate = 0.0; flemmeOkRate = 0.0
    }

    private fun cat(q: String) = brain().answer(q).category

    @Test fun wakeWord() {
        val b = brain()
        assertEquals("", Brain.afterWakeWord(b.normalize("Dis Fifi")))
        assertEquals("quelle heure il est", Brain.afterWakeWord(b.normalize("dis Fifi quelle heure il est ?")))
        assertEquals("raconte une blague", Brain.afterWakeWord(b.normalize("Dit fifi, raconte une blague")))
        assertNull(Brain.afterWakeWord(b.normalize("il fait beau aujourd'hui")))
    }

    @Test fun intents() {
        assertEquals("heure", cat("Quelle heure il est ?"))
        assertEquals("date", cat("On est quel jour ?"))
        assertEquals("blague", cat("raconte-moi une blague"))
        assertEquals("lampe_on", cat("allume la lampe"))
        assertEquals("lampe_off", cat("éteins la lampe torche"))
        assertEquals("meteo", cat("quel temps il fait ? la météo"))
        assertEquals("ca_va", cat("ça va ?"))
        assertEquals("qui", cat("t'es qui toi ?"))
        assertEquals("amour", cat("je t'aime"))
        assertEquals("insulte", cat("t'es nulle"))
        assertEquals("stop", cat("tais-toi"))
        assertEquals("youtube", cat("ouvre YouTube"))
        assertEquals("pileface", cat("pile ou face"))
        assertEquals("va_chercher", cat("c'est quoi un ornithorynque ?"))
        assertEquals("silence", cat(""))
    }

    @Test fun heureArrondie() {
        val r = brain().heure()
        assertTrue(r.clips.contains("heure/15.ogg"))
        assertTrue(r.clips.contains("nombre/20.ogg"))
    }

    @Test fun date() {
        val r = brain().date()
        assertEquals(listOf("date_intro/0.ogg", "jour/0.ogg", "nombre/23.ogg", "mois/10.ogg"), r.clips)
    }

    @Test fun searchKeepsQuery() {
        val r = brain().answer("Pourquoi le ciel est bleu")
        assertEquals(Action.SEARCH, r.action)
        assertEquals("Pourquoi le ciel est bleu", r.query)
    }

    @Test fun flemmeHappens() {
        val b = Brain(bank, Random(3)).apply { flemmeRate = 1.0 }
        assertEquals("flemme", b.answer("raconte une blague").category)
        // merci / au revoir : jamais de flemme
        assertEquals("merci", b.answer("merci").category)
    }
}
