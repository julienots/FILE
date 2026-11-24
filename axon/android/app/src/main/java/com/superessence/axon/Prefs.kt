package com.superessence.axon

import android.content.Context
import java.security.SecureRandom

/** Réglages AXON stockés sur le téléphone. */
class Prefs(ctx: Context) {
    private val sp = ctx.getSharedPreferences("axon", Context.MODE_PRIVATE)

    var relay: String
        get() = sp.getString("relay", null)?.takeIf { it.isNotBlank() } ?: BuildConfig.RELAY_URL
        set(v) = sp.edit().putString("relay", v.trim().trimEnd('/')).apply()

    /** Clé privée du salon : générée une fois, jamais partagée hors de vos appareils et de Claude. */
    val key: String
        get() = sp.getString("key", null) ?: newKey()

    fun newKey(): String {
        val abc = "ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz23456789"
        val rnd = SecureRandom()
        val k = (1..28).map { abc[rnd.nextInt(abc.length)] }.joinToString("")
        sp.edit().putString("key", k).apply()
        return k
    }

    /** "device" = ce téléphone est piloté ; "viewer" = écran de contrôle (tablette). */
    var role: String
        get() = sp.getString("role", "") ?: ""
        set(v) = sp.edit().putString("role", v).apply()

    /** Lien de jumelage d'un écran de contrôle (tablette). */
    var viewerLink: String
        get() = sp.getString("viewerLink", "") ?: ""
        set(v) = sp.edit().putString("viewerLink", v).apply()

    var paused: Boolean
        get() = sp.getBoolean("paused", false)
        set(v) = sp.edit().putBoolean("paused", v).apply()

    val connectorUrl: String get() = if (relay.isBlank()) "" else "$relay/mcp/$key"
    val viewerUrl: String get() = if (relay.isBlank()) "" else "$relay/#k=$key"
}
