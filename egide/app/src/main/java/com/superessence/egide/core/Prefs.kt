package com.superessence.egide.core

import android.content.Context
import android.content.SharedPreferences
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow

data class Upstream(val id: String, val title: String, val detail: String, val v4: List<String>)

object Prefs {
    private lateinit var sp: SharedPreferences

    val upstreams = listOf(
        Upstream("quad9", "Quad9", "Rapide, bloque aussi les malwares connus (recommandé)", listOf("9.9.9.9", "149.112.112.112")),
        Upstream("cloudflare", "Cloudflare", "Très rapide, respect de la vie privée", listOf("1.1.1.1", "1.0.0.1")),
        Upstream("adguard", "AdGuard DNS", "Blocage supplémentaire côté serveur", listOf("94.140.14.14", "94.140.15.15")),
        Upstream("google", "Google", "Le plus compatible", listOf("8.8.8.8", "8.8.4.4")),
    )

    private val _rev = MutableStateFlow(0)
    /** Incrémenté à chaque changement, pour rafraîchir l'interface. */
    val revision: StateFlow<Int> = _rev

    fun init(ctx: Context) {
        sp = ctx.getSharedPreferences("egide", Context.MODE_PRIVATE)
    }

    private fun bump() { _rev.value++ }

    var protectionWanted: Boolean
        get() = sp.getBoolean("wanted", false)
        set(v) { sp.edit().putBoolean("wanted", v).apply(); bump() }

    var startOnBoot: Boolean
        get() = sp.getBoolean("boot", true)
        set(v) { sp.edit().putBoolean("boot", v).apply(); bump() }

    var upstreamId: String
        get() = sp.getString("upstream", "quad9")!!
        set(v) { sp.edit().putString("upstream", v).apply(); bump() }

    val upstream: Upstream get() = upstreams.firstOrNull { it.id == upstreamId } ?: upstreams[0]

    var browserBlockPopups: Boolean
        get() = sp.getBoolean("popups", true)
        set(v) { sp.edit().putBoolean("popups", v).apply(); bump() }

    var totalBlocked: Long
        get() = sp.getLong("total_blocked", 0)
        set(v) { sp.edit().putLong("total_blocked", v).apply() }

    fun isListEnabled(id: String) = sp.getBoolean("list_$id", true)
    fun setListEnabled(id: String, on: Boolean) {
        sp.edit().putBoolean("list_$id", on).apply(); bump()
        Filter.onListToggled()
    }

    var allowList: Set<String>
        get() = sp.getStringSet("allow", emptySet())!!.toSortedSet()
        set(v) { sp.edit().putStringSet("allow", v).apply(); bump(); Filter.reloadUserRules() }

    var denyList: Set<String>
        get() = sp.getStringSet("deny", emptySet())!!.toSortedSet()
        set(v) { sp.edit().putStringSet("deny", v).apply(); bump(); Filter.reloadUserRules() }

    fun allow(domain: String) {
        denyList = denyList - domain
        allowList = allowList + domain
    }

    fun deny(domain: String) {
        allowList = allowList - domain
        denyList = denyList + domain
    }

    fun reset(domain: String) {
        allowList = allowList - domain
        denyList = denyList - domain
    }
}
