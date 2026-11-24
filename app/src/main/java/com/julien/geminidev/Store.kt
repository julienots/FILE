package com.julien.geminidev

import android.content.Context

/** Stockage local (privé à l'app) du token GitHub, de la clé Gemini et du modèle. */
class Store(ctx: Context) {
    private val p = ctx.getSharedPreferences("geminidev", Context.MODE_PRIVATE)

    var ghToken: String
        get() = p.getString("gh", "") ?: ""
        set(v) { p.edit().putString("gh", v.trim()).apply() }

    var geminiKey: String
        get() = p.getString("key", "") ?: ""
        set(v) { p.edit().putString("key", v.trim()).apply() }

    var model: String
        get() = p.getString("model", DEFAULT_MODEL) ?: DEFAULT_MODEL
        set(v) {
            val clean = v.trim().removePrefix("models/").ifBlank { DEFAULT_MODEL }
            p.edit().putString("model", clean).apply()
        }

    companion object {
        const val DEFAULT_MODEL = "gemini-3.8-flash"
    }
}
