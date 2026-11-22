package com.superessence.promzy.data

import android.content.Context
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.async
import kotlinx.coroutines.coroutineScope
import kotlinx.coroutines.withContext
import kotlinx.serialization.builtins.ListSerializer
import kotlinx.serialization.json.Json
import java.io.File
import java.net.HttpURLConnection
import java.net.URL
import java.net.URLEncoder

enum class Feed(val label: String, val url: String) {
    HOT("Top promos", "https://www.dealabs.com/rss/hot"),
    NEW("En direct", "https://www.dealabs.com/rss/nouveaux"),
}

/** Récupère les bons plans en direct (flux publics Dealabs), avec cache local pour l'hors-ligne. */
class DealRepository(private val context: Context) {
    private val json = Json { ignoreUnknownKeys = true }

    suspend fun load(feed: Feed): List<Deal> = withContext(Dispatchers.IO) {
        val deals = fetch(feed.url)
        cacheFile(feed.name).writeText(json.encodeToString(ListSerializer(Deal.serializer()), deals))
        deals
    }

    fun cached(feed: Feed): List<Deal> = runCatching {
        json.decodeFromString(ListSerializer(Deal.serializer()), cacheFile(feed.name).readText())
    }.getOrDefault(emptyList())

    /**
     * Recherche : flux de recherche Dealabs + filtrage local des flux « Top » et « En direct »,
     * fusionnés et triés (les plus chauds d'abord, puis les plus récents).
     */
    suspend fun search(query: String): List<Deal> = withContext(Dispatchers.IO) {
        coroutineScope {
            val q = URLEncoder.encode(query.trim(), "UTF-8")
            val remote = async { runCatching { fetch("https://www.dealabs.com/rss/search?q=$q") }.getOrDefault(emptyList()) }
            val hot = async { runCatching { fetch(Feed.HOT.url) }.getOrElse { cached(Feed.HOT) } }
            val new = async { runCatching { fetch(Feed.NEW.url) }.getOrElse { cached(Feed.NEW) } }
            val local = (hot.await() + new.await()).filter { it.matchesQuery(query) }
            val remoteOk = remote.await()
            (remoteOk + local).distinctBy { it.id }
                .sortedWith(compareByDescending<Deal> { it.matchesQuery(query) }.thenByDescending { it.temperature ?: 0 }.thenByDescending { it.publishedAt })
        }
    }

    private fun cacheFile(name: String) = File(context.cacheDir, "feed_$name.json")

    private fun fetch(url: String, redirects: Int = 0): List<Deal> {
        val c = URL(url).openConnection() as HttpURLConnection
        c.connectTimeout = 12_000
        c.readTimeout = 15_000
        c.instanceFollowRedirects = true
        c.setRequestProperty("User-Agent", "Mozilla/5.0 (Linux; Android 14) Promzy/1.0")
        c.setRequestProperty("Accept", "application/rss+xml, application/xml, text/xml, */*")
        try {
            val code = c.responseCode
            if (code in 300..399 && redirects < 4) {
                val loc = c.getHeaderField("Location") ?: throw IllegalStateException("Redirection sans destination")
                return fetch(URL(URL(url), loc).toString(), redirects + 1)
            }
            if (code !in 200..299) throw IllegalStateException("Le serveur a répondu $code")
            return c.inputStream.use { RssParser.parse(it) }
        } finally {
            c.disconnect()
        }
    }
}
