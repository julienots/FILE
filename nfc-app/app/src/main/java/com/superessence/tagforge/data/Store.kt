package com.superessence.tagforge.data

import android.content.Context
import android.content.SharedPreferences
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import com.superessence.tagforge.nfc.RecordItem
import org.json.JSONArray
import org.json.JSONObject

data class HistoryEntry(val time: Long, val action: String, val uid: String, val chip: String, val summary: String, val detail: String)

data class Template(val name: String, val items: List<RecordItem>, val created: Long = System.currentTimeMillis()) {
    fun toJson(): JSONObject = JSONObject().put("name", name).put("created", created)
        .put("items", JSONArray().apply { items.forEach { put(it.toJson()) } })

    companion object {
        fun fromJson(o: JSONObject) = Template(
            o.getString("name"),
            o.getJSONArray("items").let { a -> (0 until a.length()).map { RecordItem.fromJson(a.getJSONObject(it)) } },
            o.optLong("created"),
        )
    }
}

object Store {
    private lateinit var prefs: SharedPreferences

    val history = mutableStateListOf<HistoryEntry>()
    val templates = mutableStateListOf<Template>()

    var autoDump by mutableStateOf(true)
    var sound by mutableStateOf(true)
    var vibrate by mutableStateOf(true)
    var emulationOn by mutableStateOf(false)
    var emulationNdef: ByteArray? = null
    var emulationLabel by mutableStateOf("")

    fun init(ctx: Context) {
        if (::prefs.isInitialized) return
        prefs = ctx.applicationContext.getSharedPreferences("tagforge", Context.MODE_PRIVATE)
        autoDump = prefs.getBoolean("autoDump", true)
        sound = prefs.getBoolean("sound", true)
        vibrate = prefs.getBoolean("vibrate", true)
        emulationOn = prefs.getBoolean("emuOn", false)
        emulationLabel = prefs.getString("emuLabel", "").orEmpty()
        emulationNdef = prefs.getString("emuNdef", null)?.let { android.util.Base64.decode(it, android.util.Base64.NO_WRAP) }
        runCatching {
            val a = JSONArray(prefs.getString("history", "[]"))
            for (i in 0 until a.length()) {
                val o = a.getJSONObject(i)
                history += HistoryEntry(o.getLong("t"), o.getString("a"), o.optString("u"), o.optString("c"), o.optString("s"), o.optString("d"))
            }
        }
        runCatching {
            val a = JSONArray(prefs.getString("templates", "[]"))
            for (i in 0 until a.length()) templates += Template.fromJson(a.getJSONObject(i))
        }
        if (templates.isEmpty() && !prefs.getBoolean("seeded", false)) {
            templates.addAll(Samples.templates)
            prefs.edit().putBoolean("seeded", true).apply()
            saveTemplates()
        }
    }

    fun saveSettings() {
        prefs.edit().putBoolean("autoDump", autoDump).putBoolean("sound", sound).putBoolean("vibrate", vibrate)
            .putBoolean("emuOn", emulationOn).putString("emuLabel", emulationLabel)
            .putString("emuNdef", emulationNdef?.let { android.util.Base64.encodeToString(it, android.util.Base64.NO_WRAP) })
            .apply()
    }

    fun addHistory(e: HistoryEntry) {
        history.add(0, e)
        while (history.size > 300) history.removeAt(history.size - 1)
        saveHistory()
    }

    fun clearHistory() { history.clear(); saveHistory() }

    private fun saveHistory() {
        val a = JSONArray()
        history.forEach { a.put(JSONObject().put("t", it.time).put("a", it.action).put("u", it.uid).put("c", it.chip).put("s", it.summary).put("d", it.detail)) }
        prefs.edit().putString("history", a.toString()).apply()
    }

    fun saveTemplate(t: Template) {
        val i = templates.indexOfFirst { it.name == t.name }
        if (i >= 0) templates[i] = t else templates.add(0, t)
        saveTemplates()
    }

    fun deleteTemplate(t: Template) { templates.remove(t); saveTemplates() }

    private fun saveTemplates() {
        val a = JSONArray(); templates.forEach { a.put(it.toJson()) }
        prefs.edit().putString("templates", a.toString()).apply()
    }

    fun exportJson(): String = JSONObject()
        .put("app", "TagForge").put("version", 1)
        .put("templates", JSONArray().apply { templates.forEach { put(it.toJson()) } })
        .toString(2)

    fun importJson(s: String): Int {
        val o = JSONObject(s)
        val a = o.getJSONArray("templates")
        var n = 0
        for (i in 0 until a.length()) { saveTemplate(Template.fromJson(a.getJSONObject(i))); n++ }
        return n
    }
}
