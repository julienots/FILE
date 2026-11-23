package com.fifi.assistant

import java.text.Normalizer
import java.time.LocalDateTime
import kotlin.random.Random

/** Une réplique enregistrée : fichier audio (dans assets/voice) + texte affiché. */
data class Line(val file: String, val text: String)

enum class Action { NONE, SEARCH, WEATHER, MUSIC, YOUTUBE, TORCH_ON, TORCH_OFF, DIAL, CAMERA, STOP }

data class Reply(
    val clips: List<String>,
    val text: String,
    val action: Action = Action.NONE,
    val query: String = "",
    val category: String = "",
)

/** Banque de répliques : catégorie -> lignes. Tire au hasard sans répéter la dernière. */
class VoiceBank(private val lines: Map<String, List<Line>>, private val rnd: Random = Random.Default) {
    private val last = HashMap<String, Int>()

    fun has(cat: String) = !lines[cat].isNullOrEmpty()

    fun pick(cat: String): Line? {
        val l = lines[cat] ?: return null
        if (l.isEmpty()) return null
        var i = rnd.nextInt(l.size)
        if (l.size > 1 && i == last[cat]) i = (i + 1 + rnd.nextInt(l.size - 1)) % l.size
        last[cat] = i
        return l[i]
    }

    fun at(cat: String, index: Int): Line? = lines[cat]?.getOrNull(index)
}

/** Le « cerveau » de Fifi : comprend (à peu près) et choisit quoi répondre. */
class Brain(
    private val bank: VoiceBank,
    private val rnd: Random = Random.Default,
    private val now: () -> LocalDateTime = { LocalDateTime.now() },
) {
    /** Probabilité que Fifi refuse purement et simplement de répondre. */
    var flemmeRate = 0.15
    /** Probabilité qu'elle râle avant de répondre quand même. */
    var flemmeOkRate = 0.20

    private class Rule(val cat: String, val pattern: Regex, val action: Action = Action.NONE, val lazyOk: Boolean = true)

    private fun r(vararg words: String) = Regex("\\b(" + words.joinToString("|") + ")\\b")

    private val rules = listOf(
        Rule("stop", r("tais toi", "chut", "stop", "arrete", "ta gueule", "silence", "ferme la"), Action.STOP, lazyOk = false),
        Rule("@heure", r("quelle heure", "l heure", "heure est il", "heure il est", "il est quelle heure")),
        Rule("@date", r("quel jour", "la date", "quelle date", "on est le combien", "on est quel", "jour on est", "jour sommes")),
        Rule("@pileface", r("pile ou face")),
        Rule("@de", r("lance un de", "lancer un de", "jette un de", "lance le de", "lance les des")),
        Rule("blague", r("blague", "blagues", "drole", "fais moi rire", "rigoler", "marrant", "raconte")),
        Rule("meteo", r("meteo", "temps qu il fait", "temps fait il", "il pleut", "pleuvoir", "soleil", "temperature", "fait il chaud", "fait il froid"), Action.WEATHER),
        Rule("chanter", r("chante", "chanter", "chantes")),
        Rule("youtube", r("youtube"), Action.YOUTUBE),
        Rule("musique", r("musique", "chanson", "playlist", "met du son", "mets du son", "spotify", "deezer"), Action.MUSIC),
        Rule("@lampe", r("lampe", "torche", "lumiere", "flash")),
        Rule("appeler", r("appelle", "appeler", "telephone a", "passe un coup de fil", "compose"), Action.DIAL),
        Rule("photo", r("photo", "camera", "selfie", "appareil photo"), Action.CAMERA),
        Rule("qui", r("qui es tu", "t es qui", "tu es qui", "ton nom", "tu t appelles", "presente toi", "c est qui fifi", "qui est fifi")),
        Rule("age", r("quel age", "ton age", "t as quel age", "tu as quel age")),
        Rule("ca_va", r("ca va", "comment vas tu", "comment tu vas", "la forme", "tu vas bien", "quoi de neuf")),
        Rule("merci", r("merci", "thanks", "trop gentille"), lazyOk = false),
        Rule("aurevoir", r("au revoir", "bye", "bonne nuit", "a plus", "a demain", "ciao", "tchao", "a tout a l heure"), lazyOk = false),
        Rule("amour", r("je t aime", "je t adore", "t aime", "mariage", "epouse moi", "amoureux", "amoureuse", "bisou", "bisous")),
        Rule("compliment", r("belle", "geniale", "genial", "trop forte", "intelligente", "la meilleure", "t es trop bien", "trop bien", "bravo", "super")),
        Rule("insulte", r("nulle", "nul", "idiote", "idiot", "bete", "debile", "pourrie", "conne", "con", "stupide", "moche", "naze", "inutile")),
        Rule("rire", r("mdr", "lol", "ptdr", "haha", "hahaha")),
        Rule("manger", r("faim", "manger", "pizza", "mange", "diner", "dejeuner", "repas")),
        Rule("fatigue", r("fatigue", "fatiguee", "dormir", "dodo", "sommeil", "sieste")),
        Rule("ennui", r("ennuie", "m ennuie", "je m ennuie", "quoi faire", "je fais quoi", "ennuyer")),
        Rule("calcul", r("combien font", "combien fait", "calcul", "calcule", "multiplie", "divise", "racine carree")),
        Rule("bonjour", r("bonjour", "salut", "coucou", "hello", "wesh", "yo", "bonsoir", "hey")),
    )

    private val questionWords = r(
        "c est quoi", "qu est ce", "qui est", "qui a", "comment", "pourquoi", "ou est", "ou se trouve",
        "quand", "combien", "cherche", "recherche", "trouve", "quel", "quelle", "quels", "quelles", "definition",
        "explique", "dis moi", "sais tu", "tu sais",
    )

    fun normalize(s: String): String {
        val n = Normalizer.normalize(s.lowercase(), Normalizer.Form.NFD).replace(Regex("\\p{Mn}+"), "")
        return n.replace(Regex("[^a-z0-9 ]"), " ").replace(Regex("\\s+"), " ").trim()
    }

    fun answer(raw: String): Reply {
        val text = normalize(raw)
        if (text.isEmpty()) return simple("silence")

        val rule = rules.firstOrNull { it.pattern.containsMatchIn(text) }
        if (rule == null) {
            return if (questionWords.containsMatchIn(text) || text.split(" ").size >= 4) {
                if (rnd.nextDouble() < 0.12) simple("flemme")
                else simple("va_chercher", Action.SEARCH, raw)
            } else if (rnd.nextBoolean()) simple("incompris") else simple("flemme")
        }

        if (rule.lazyOk) {
            val x = rnd.nextDouble()
            if (x < flemmeRate) return simple("flemme")
            if (x < flemmeRate + flemmeOkRate) {
                val base = build(rule, text, raw)
                val pre = bank.pick("flemme_ok") ?: return base
                return base.copy(clips = listOf(pre.file) + base.clips, text = pre.text + " " + base.text)
            }
        }
        return build(rule, text, raw)
    }

    private fun build(rule: Rule, text: String, raw: String): Reply = when (rule.cat) {
        "@heure" -> heure()
        "@date" -> date()
        "@pileface" -> combo(listOfNotNull(bank.pick("pileface_intro"), bank.pick(if (rnd.nextBoolean()) "pile" else "face")), "pileface")
        "@de" -> {
            val v = rnd.nextInt(1, 7)
            combo(listOfNotNull(bank.pick("de_intro"), bank.at("nombre", v)), "de")
        }
        "@lampe" -> {
            val off = Regex("\\b(eteins|eteint|eteindre|off|coupe|arrete)\\b").containsMatchIn(text)
            if (off) simple("lampe_off", Action.TORCH_OFF) else simple("lampe_on", Action.TORCH_ON)
        }
        else -> simple(rule.cat, rule.action, raw)
    }

    fun simple(cat: String, action: Action = Action.NONE, query: String = ""): Reply {
        val l = bank.pick(cat) ?: bank.pick("incompris") ?: return Reply(emptyList(), "…", action, query, cat)
        return Reply(listOf(l.file), l.text, action, query, cat)
    }

    private fun combo(lines: List<Line>, cat: String) =
        Reply(lines.map { it.file }.filter { it.isNotEmpty() }, lines.joinToString(" ") { it.text }.trim(), category = cat)

    fun heure(): Reply {
        val t = now()
        val parts = mutableListOf<Line>()
        bank.pick("heure_intro")?.let(parts::add)
        bank.at("heure", t.hour)?.let(parts::add)
        // Fifi a la flemme d'être précise : minutes arrondies à 5.
        val m = t.minute / 5 * 5
        if (m > 0) bank.at("nombre", m)?.let { if (it.file.isNotEmpty()) parts.add(it) }
        bank.pick("heure_outro")?.let(parts::add)
        return combo(parts, "heure")
    }

    fun date(): Reply {
        val t = now()
        val parts = mutableListOf<Line>()
        bank.pick("date_intro")?.let(parts::add)
        bank.at("jour", t.dayOfWeek.value - 1)?.let(parts::add)
        (if (t.dayOfMonth == 1) bank.at("premier", 0) else bank.at("nombre", t.dayOfMonth))?.let(parts::add)
        bank.at("mois", t.monthValue - 1)?.let(parts::add)
        return combo(parts, "date")
    }

    companion object {
        private val WAKE = Regex("\\b(?:(?:dis|di|dit|dites|hey|eh|ok|allo)\\s+)?(?:fifi|fi fi|fifis|fifie|fifit|phiphi|fiffi|fify|fyfy|fi\\s?fy|fee fee)\\b")

        /** Renvoie null si « Fifi » n'a pas été prononcé, sinon ce qui suit le mot (peut être vide). */
        fun afterWakeWord(normalized: String): String? {
            val m = WAKE.find(normalized) ?: return null
            return normalized.substring(m.range.last + 1).trim()
        }
    }
}
