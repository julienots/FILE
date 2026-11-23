package com.superessence.focus.core

import android.content.Context
import android.content.SharedPreferences
import androidx.compose.runtime.mutableIntStateOf
import org.json.JSONArray
import org.json.JSONObject
import java.time.LocalDate

/** Persistance locale (SharedPreferences) + compteur observable pour Compose. */
object Store {
    private lateinit var sp: SharedPreferences
    /** Incrémenté à chaque modification : lire `Store.rev.intValue` dans un composable pour se recomposer. */
    val rev = mutableIntStateOf(0)

    fun init(c: Context) {
        if (!::sp.isInitialized) sp = c.applicationContext.getSharedPreferences("focus", Context.MODE_PRIVATE)
    }

    private fun bump() { rev.intValue++ }
    private fun edit(block: SharedPreferences.Editor.() -> Unit) { sp.edit().apply(block).apply(); bump() }

    // ---------------------------------------------------------------- Session
    var sessionStart: Long
        get() = sp.getLong("s_start", 0)
        set(v) = edit { putLong("s_start", v) }
    var sessionEnd: Long
        get() = sp.getLong("s_end", 0)
        set(v) = edit { putLong("s_end", v) }
    var sessionBlocking: Boolean
        get() = sp.getBoolean("s_block", false)
        set(v) = edit { putBoolean("s_block", v) }
    var sessionStrict: Boolean
        get() = sp.getBoolean("s_strict", false)
        set(v) = edit { putBoolean("s_strict", v) }
    var lastDuration: Int
        get() = sp.getInt("last_dur", 45)
        set(v) = edit { putInt("last_dur", v) }
    var blockPref: Boolean
        get() = sp.getBoolean("block_pref", true)
        set(v) = edit { putBoolean("block_pref", v) }
    var strictPref: Boolean
        get() = sp.getBoolean("strict_pref", false)
        set(v) = edit { putBoolean("strict_pref", v) }

    // ---------------------------------------------------------------- Applis autorisées
    var allowedApps: Set<String>
        get() = sp.getStringSet("allowed", emptySet()) ?: emptySet()
        set(v) = edit { putStringSet("allowed", HashSet(v)) }
    var allowedInitialized: Boolean
        get() = sp.getBoolean("allowed_init", false)
        set(v) = edit { putBoolean("allowed_init", v) }

    // ---------------------------------------------------------------- Romane
    var romaneName: String
        get() = sp.getString("r_name", "Romane") ?: "Romane"
        set(v) = edit { putString("r_name", v) }
    var romaneNumber: String
        get() = sp.getString("r_num", "") ?: ""
        set(v) = edit { putString("r_num", v) }
    /** Les applis de téléphone sont autorisées jusqu'à cet instant (appel lancé depuis Focus.). */
    var callGraceUntil: Long
        get() = sp.getLong("call_grace", 0)
        set(v) = edit { putLong("call_grace", v) }

    // ---------------------------------------------------------------- Musique
    var track: String
        get() = sp.getString("track", "piano") ?: "piano"
        set(v) = edit { putString("track", v) }
    var nature: String
        get() = sp.getString("nature", "rain_soft") ?: "rain_soft"
        set(v) = edit { putString("nature", v) }
    var musicVol: Float
        get() = sp.getFloat("m_vol", 0.7f)
        set(v) = edit { putFloat("m_vol", v) }
    var natureVol: Float
        get() = sp.getFloat("n_vol", 0.35f)
        set(v) = edit { putFloat("n_vol", v) }

    // ---------------------------------------------------------------- Statistiques
    fun addMinutes(min: Int) {
        if (min <= 0) return
        val o = JSONObject(sp.getString("days", "{}") ?: "{}")
        val k = LocalDate.now().toString()
        o.put(k, o.optInt(k) + min)
        edit {
            putString("days", o.toString())
            putInt("sessions", sp.getInt("sessions", 0) + 1)
        }
    }
    fun minutesOn(d: LocalDate): Int = JSONObject(sp.getString("days", "{}") ?: "{}").optInt(d.toString())
    val sessionsCount: Int get() = sp.getInt("sessions", 0)
    fun streak(): Int {
        var d = LocalDate.now()
        if (minutesOn(d) == 0) d = d.minusDays(1)
        var n = 0
        while (minutesOn(d) > 0) { n++; d = d.minusDays(1) }
        return n
    }
    val totalMinutes: Int get() {
        val o = JSONObject(sp.getString("days", "{}") ?: "{}")
        return o.keys().asSequence().sumOf { o.optInt(it) }
    }

    // ---------------------------------------------------------------- Fiches
    var decksJson: JSONArray
        get() = JSONArray(sp.getString("decks", null) ?: Cards.SAMPLE)
        set(v) = edit { putString("decks", v.toString()) }
}
