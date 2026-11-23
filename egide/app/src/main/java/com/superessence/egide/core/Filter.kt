package com.superessence.egide.core

import android.content.Context
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import java.io.File
import java.net.HttpURLConnection
import java.net.URL

/** Une liste de blocage : embarquée dans l'APK et mise à jour depuis Internet. */
data class ListSource(
    val id: String,
    val title: String,
    val description: String,
    val url: String,
    val credit: String,
)

object Sources {
    val all = listOf(
        ListSource(
            "ads", "Publicités & traqueurs",
            "Régies publicitaires, pisteurs, bannières, pubs dans les applis et les jeux.",
            "https://raw.githubusercontent.com/StevenBlack/hosts/master/hosts",
            "StevenBlack/hosts (MIT)",
        ),
        ListSource(
            "popups", "Pop-ups & redirections",
            "Réseaux de pop-unders et de redirections forcées (fausses alertes, « vous avez gagné », onglets qui s'ouvrent seuls).",
            "https://raw.githubusercontent.com/hagezi/dns-blocklists/main/wildcard/popupads-onlydomains.txt",
            "HaGeZi Pop-Up Ads (GPL-3.0)",
        ),
        ListSource(
            "scams", "Arnaques & faux sites",
            "Faux magasins, faux supports techniques, escroqueries connues.",
            "https://raw.githubusercontent.com/hagezi/dns-blocklists/main/wildcard/fake-onlydomains.txt",
            "HaGeZi Fake (GPL-3.0)",
        ),
        ListSource(
            "threats", "Malwares & hameçonnage",
            "Sites qui diffusent des virus, du phishing, des cryptomineurs ou des faux téléchargements.",
            "https://raw.githubusercontent.com/hagezi/dns-blocklists/main/wildcard/tif.mini-onlydomains.txt",
            "HaGeZi Threat Intelligence Feeds mini (GPL-3.0)",
        ),
    )
}

/**
 * Domaines jamais bloqués : ils sont indispensables au fonctionnement normal du téléphone
 * (Play Store, mises à jour, YouTube lui-même, connexion aux comptes…).
 */
private val ESSENTIALS = listOf(
    "youtube.com", "googlevideo.com", "ytimg.com", "ggpht.com", "youtubei.googleapis.com",
    "googleapis.com", "gstatic.com", "android.com", "play.google.com", "accounts.google.com",
    "connectivitycheck.gstatic.com", "clients3.google.com", "whatsapp.net", "whatsapp.com",
    "raw.githubusercontent.com", "apple.com", "icloud.com", "microsoft.com", "live.com",
)

object Filter {
    private lateinit var appCtx: Context
    @Volatile private var lists: Map<String, DomainSet> = emptyMap()
    @Volatile private var essentials = buildSet(ESSENTIALS)
    @Volatile private var allow = DomainSet.EMPTY
    @Volatile private var deny = DomainSet.EMPTY

    private val _loaded = MutableStateFlow(false)
    val loaded: StateFlow<Boolean> = _loaded
    private val _domainCount = MutableStateFlow(0)
    val domainCount: StateFlow<Int> = _domainCount

    private fun buildSet(items: Collection<String>): DomainSet {
        val b = DomainSet.Builder(items.size + 1)
        items.forEach { b.add(it) }
        return b.build()
    }

    fun init(ctx: Context) {
        appCtx = ctx.applicationContext
    }

    private fun listDir() = File(appCtx.filesDir, "lists").apply { mkdirs() }
    private fun cacheFile(id: String) = File(listDir(), "$id.bin")
    private fun downloadedFile(id: String) = File(listDir(), "$id.txt")

    fun lastUpdate(id: String): Long = downloadedFile(id).takeIf { it.exists() }?.lastModified() ?: 0L

    @Synchronized
    fun ensureLoaded() { if (!_loaded.value) load() }

    /** Charge (ou recharge) toutes les listes. À appeler hors du thread principal. */
    @Synchronized
    fun load() {
        val map = HashMap<String, DomainSet>()
        for (src in Sources.all) map[src.id] = loadList(src.id)
        lists = map
        reloadUserRules()
        _loaded.value = true
    }

    private fun loadList(id: String): DomainSet {
        val cache = cacheFile(id)
        val txt = downloadedFile(id)
        val sourceStamp = if (txt.exists()) txt.lastModified() else appCtx.packageManager
            .getPackageInfo(appCtx.packageName, 0).lastUpdateTime
        if (cache.exists() && cache.lastModified() >= sourceStamp) {
            runCatching { return readCache(cache) }
        }
        val b = DomainSet.Builder(200_000)
        val stream = if (txt.exists()) txt.inputStream() else appCtx.assets.open("lists/$id.txt")
        stream.bufferedReader().useLines { seq -> seq.forEach { l -> DomainSet.parseLine(l)?.let(b::add) } }
        val set = b.build()
        runCatching { writeCache(cache, set) }
        return set
    }

    private fun readCache(f: File): DomainSet {
        val bytes = f.readBytes()
        val bb = java.nio.ByteBuffer.wrap(bytes)
        val arr = LongArray(bytes.size / 8)
        bb.asLongBuffer().get(arr)
        return DomainSet.fromSorted(arr)
    }

    private fun writeCache(f: File, set: DomainSet) {
        val arr = set.toLongArray()
        val bb = java.nio.ByteBuffer.allocate(arr.size * 8)
        bb.asLongBuffer().put(arr)
        val tmp = File(f.path + ".tmp")
        tmp.writeBytes(bb.array())
        tmp.renameTo(f)
    }

    fun reloadUserRules() {
        allow = buildSet(Prefs.allowList)
        deny = buildSet(Prefs.denyList)
        recount()
    }

    private fun recount() {
        _domainCount.value = Sources.all.filter { Prefs.isListEnabled(it.id) }.sumOf { lists[it.id]?.size ?: 0 } + deny.size
    }

    fun onListToggled() = recount()

    fun listSize(id: String): Int = lists[id]?.size ?: 0

    /** Décision de filtrage pour un nom d'hôte. */
    fun isBlocked(rawHost: String): Boolean {
        val host = rawHost.trimEnd('.').lowercase()
        if (host.isEmpty()) return false
        if (allow.matches(host)) return false
        if (deny.matches(host)) return true
        if (essentials.matches(host)) return false
        for (src in Sources.all) {
            if (!Prefs.isListEnabled(src.id)) continue
            if (lists[src.id]?.matches(host) == true) return true
        }
        return false
    }

    /** Télécharge la dernière version de toutes les listes actives. Renvoie (réussies, échouées). */
    fun updateAll(onProgress: (String) -> Unit): Pair<Int, Int> {
        var ok = 0; var ko = 0
        for (src in Sources.all) {
            onProgress(src.title)
            try {
                val conn = URL(src.url).openConnection() as HttpURLConnection
                conn.connectTimeout = 15_000; conn.readTimeout = 30_000
                conn.setRequestProperty("User-Agent", "Egide/1.0")
                if (conn.responseCode != 200) throw IllegalStateException("HTTP ${conn.responseCode}")
                val tmp = File(listDir(), "${src.id}.download")
                var count = 0
                conn.inputStream.bufferedReader().use { r ->
                    tmp.bufferedWriter().use { w ->
                        r.forEachLine { l -> DomainSet.parseLine(l)?.let { w.write(it); w.write('\n'.code); count++ } }
                    }
                }
                if (count < 100) throw IllegalStateException("liste vide")
                tmp.renameTo(downloadedFile(src.id))
                ok++
            } catch (e: Exception) {
                ko++
            }
        }
        load()
        return ok to ko
    }
}
