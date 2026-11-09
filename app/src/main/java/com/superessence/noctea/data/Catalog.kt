package com.superessence.noctea.data

import android.content.Context
import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.int
import kotlinx.serialization.json.jsonArray
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import kotlinx.serialization.json.long
import kotlinx.serialization.json.longOrNull

/** Élément d'une timeline audio (voix + silences). */
sealed interface Seg {
    val ms: Long

    /** Clip vocal embarqué (assets/voice/<id>.ogg). */
    data class Clip(val id: String, override val ms: Long) : Seg

    /** Effet sonore embarqué (bol chantant, inspiration). */
    data class Fx(val name: String, override val ms: Long) : Seg

    /** Fichier audio local (voix de synthèse générée sur l'appareil ou par un serveur). */
    data class File(val path: String, override val ms: Long) : Seg

    /** Silence réel. */
    data class Silence(override val ms: Long) : Seg
}

data class AmbienceLayer(val sound: String, val volume: Float)

data class BreathBlock(
    val start: Long,
    val inhale: Long,
    val hold: Long,
    val exhale: Long,
    val hold2: Long,
    val count: Int,
    val end: Long,
) {
    val cycle: Long get() = inhale + hold + exhale + hold2
}

data class Category(val id: String, val title: String, val subtitle: String)

data class Track(
    val id: String,
    val category: String,
    val title: String,
    val description: String,
    val art: String,
    val durationMs: Long,
    val premium: Boolean,
    val ambience: List<AmbienceLayer>,
    val breath: List<BreathBlock>,
    val segments: List<Seg>,
    val text: String,
)

data class BankPhrase(val text: String, val segments: List<Seg>, val durationMs: Long)

data class Quote(val text: String, val author: String)

/** Catalogue embarqué, produit par tools/build_content.py. */
class Catalog(
    val voiceName: String,
    val clips: Map<String, Long>,
    val fx: Map<String, Long>,
    val categories: List<Category>,
    val tracks: List<Track>,
    val bank: Map<String, List<BankPhrase>>,
    val quotes: List<Quote>,
) {
    private val byId = tracks.associateBy { it.id }
    fun track(id: String): Track? = byId[id]
    fun inCategory(cat: String) = tracks.filter { it.category == cat }
    fun category(id: String) = categories.firstOrNull { it.id == id }

    companion object {
        private val json = Json { ignoreUnknownKeys = true }

        fun load(context: Context): Catalog =
            context.assets.open("content/catalog.json").use { parse(it.readBytes().decodeToString()) }

        fun parse(text: String): Catalog {
            val root = json.parseToJsonElement(text).jsonObject
            val clips = root["clips"]!!.jsonObject.mapValues { it.value.jsonPrimitive.long }
            val fx = root["fx"]!!.jsonObject.mapValues { it.value.jsonPrimitive.long }

            fun segs(arr: JsonArray): List<Seg> = arr.map { el ->
                val p = el.jsonPrimitive
                val asLong = if (p.isString) null else p.longOrNull
                when {
                    asLong != null -> Seg.Silence(asLong)
                    p.content.startsWith("fx:") -> p.content.removePrefix("fx:").let { Seg.Fx(it, fx[it] ?: 0L) }
                    else -> Seg.Clip(p.content, clips[p.content] ?: error("clip inconnu ${p.content}"))
                }
            }

            val categories = root["categories"]!!.jsonArray.map {
                val o = it.jsonObject
                Category(o.s("id"), o.s("title"), o.s("subtitle"))
            }
            val tracks = root["tracks"]!!.jsonArray.map {
                val o = it.jsonObject
                Track(
                    id = o.s("id"),
                    category = o.s("category"),
                    title = o.s("title"),
                    description = o.s("description"),
                    art = o.s("art"),
                    durationMs = o["durationMs"]!!.jsonPrimitive.long,
                    premium = o["premium"]?.jsonPrimitive?.content == "true",
                    ambience = o["ambience"]!!.jsonArray.map { a ->
                        val ao = a.jsonObject
                        AmbienceLayer(ao.s("sound"), ao["volume"]!!.jsonPrimitive.content.toFloat())
                    },
                    breath = o["breath"]!!.jsonArray.map { b ->
                        val bo = b.jsonObject
                        BreathBlock(
                            bo.l("start"), bo.l("inhale"), bo.l("hold"), bo.l("exhale"), bo.l("hold2"),
                            bo["count"]!!.jsonPrimitive.int, bo.l("end"),
                        )
                    },
                    segments = segs(o["segments"]!!.jsonArray),
                    text = o.s("text"),
                )
            }
            val bank = root["bank"]!!.jsonObject.mapValues { (_, v) ->
                v.jsonArray.map { e ->
                    val eo = e.jsonObject
                    BankPhrase(eo.s("text"), segs(eo["segments"]!!.jsonArray), eo.l("durationMs"))
                }
            }
            val quotes = root["quotes"]!!.jsonArray.map {
                val o = it.jsonObject
                Quote(o.s("text"), o.s("author"))
            }
            val voice = (root["voice"] as? JsonObject)?.get("name")?.let { (it as JsonPrimitive).content } ?: ""
            return Catalog(voice, clips, fx, categories, tracks, bank, quotes)
        }

        private fun JsonObject.s(k: String) = this[k]!!.jsonPrimitive.content
        private fun JsonObject.l(k: String) = this[k]!!.jsonPrimitive.long
    }
}
