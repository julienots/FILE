package com.superessence.focus.core

import org.json.JSONArray
import org.json.JSONObject

data class Card(val id: Long, val q: String, val a: String, val box: Int, val due: Long)
data class Deck(val id: Long, val name: String, val color: Int, val cards: List<Card>) {
    fun dueCount(now: Long = System.currentTimeMillis()) = cards.count { it.due <= now }
    fun mastered() = cards.count { it.box >= 5 }
}

/** Fiches de révision avec répétition espacée (système de Leitner, 5 boîtes). */
object Cards {
    private val INTERVAL_DAYS = intArrayOf(0, 0, 1, 3, 7, 16)
    val COLORS = listOf(0xFFFFC928.toInt(), 0xFF7CC8FF.toInt(), 0xFFFF8FA3.toInt(), 0xFF8EE3B0.toInt(), 0xFFC6A8FF.toInt(), 0xFFFFA866.toInt())

    const val SAMPLE = """[{"id":1,"name":"Exemple — Français","color":0,"cards":[
      {"id":11,"q":"Qu'est-ce qu'une métaphore ?","a":"Une comparaison sans outil de comparaison (« cette femme est une rose »).","box":1,"due":0},
      {"id":12,"q":"Qui a écrit « Les Misérables » ?","a":"Victor Hugo (1862).","box":1,"due":0},
      {"id":13,"q":"Définition d'une anaphore","a":"Répétition d'un même mot ou groupe de mots en début de phrases ou de vers.","box":1,"due":0}]}]"""

    fun decks(): List<Deck> {
        val arr = Store.decksJson
        return (0 until arr.length()).map { i ->
            val d = arr.getJSONObject(i)
            val cs = d.getJSONArray("cards")
            Deck(d.getLong("id"), d.getString("name"), d.optInt("color"),
                (0 until cs.length()).map { j ->
                    val c = cs.getJSONObject(j)
                    Card(c.getLong("id"), c.getString("q"), c.getString("a"), c.optInt("box", 1), c.optLong("due"))
                })
        }
    }

    private fun save(decks: List<Deck>) {
        val arr = JSONArray()
        decks.forEach { d ->
            val cs = JSONArray()
            d.cards.forEach { c ->
                cs.put(JSONObject().put("id", c.id).put("q", c.q).put("a", c.a).put("box", c.box).put("due", c.due))
            }
            arr.put(JSONObject().put("id", d.id).put("name", d.name).put("color", d.color).put("cards", cs))
        }
        Store.decksJson = arr
    }

    private fun newId() = System.nanoTime()

    fun addDeck(name: String) {
        val ds = decks()
        save(ds + Deck(newId(), name.trim(), ds.size % COLORS.size, emptyList()))
    }
    fun renameDeck(id: Long, name: String) = save(decks().map { if (it.id == id) it.copy(name = name.trim()) else it })
    fun deleteDeck(id: Long) = save(decks().filter { it.id != id })

    fun addCard(deckId: Long, q: String, a: String) = save(decks().map {
        if (it.id == deckId) it.copy(cards = it.cards + Card(newId(), q.trim(), a.trim(), 1, 0)) else it
    })
    fun editCard(deckId: Long, cardId: Long, q: String, a: String) = update(deckId, cardId) { it.copy(q = q.trim(), a = a.trim()) }
    fun deleteCard(deckId: Long, cardId: Long) = save(decks().map {
        if (it.id == deckId) it.copy(cards = it.cards.filter { c -> c.id != cardId }) else it
    })

    /** Réponse : su → boîte suivante ; pas su → retour boîte 1. */
    fun answer(deckId: Long, cardId: Long, known: Boolean) = update(deckId, cardId) {
        val box = if (known) (it.box + 1).coerceAtMost(5) else 1
        it.copy(box = box, due = System.currentTimeMillis() + INTERVAL_DAYS[box] * 86_400_000L)
    }

    private fun update(deckId: Long, cardId: Long, f: (Card) -> Card) = save(decks().map { d ->
        if (d.id == deckId) d.copy(cards = d.cards.map { if (it.id == cardId) f(it) else it }) else d
    })
}
