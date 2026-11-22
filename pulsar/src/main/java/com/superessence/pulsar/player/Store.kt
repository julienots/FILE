package com.superessence.pulsar.player

import android.content.Context
import com.superessence.pulsar.engine.GenParams
import com.superessence.pulsar.engine.LiveParams
import com.superessence.pulsar.engine.Scale
import com.superessence.pulsar.engine.Song
import com.superessence.pulsar.engine.TrackId
import org.json.JSONArray
import org.json.JSONObject

data class Favorite(val code: String, val title: String, val genreId: String, val bpm: Int, val key: String, val params: GenParams, val time: Long)

/** Persistance locale (favoris, récents, réglages du mixeur). */
class Store(context: Context) {
    private val prefs = context.getSharedPreferences("pulsar", Context.MODE_PRIVATE)

    private fun GenParams.toJson() = JSONObject().apply {
        put("g", genreId); put("s", seed); bpm?.let { put("bpm", it.toDouble()) }; root?.let { put("root", it) }
        scale?.let { put("scale", it.name) }; put("e", energy.toDouble()); put("c", complexity.toDouble())
        swing?.let { put("sw", it.toDouble()) }; put("len", length.toDouble()); put("lead", withLead)
    }

    private fun paramsOf(o: JSONObject) = GenParams(
        genreId = o.getString("g"), seed = o.getLong("s"),
        bpm = if (o.has("bpm")) o.getDouble("bpm").toFloat() else null,
        root = if (o.has("root")) o.getInt("root") else null,
        scale = if (o.has("scale")) runCatching { Scale.valueOf(o.getString("scale")) }.getOrNull() else null,
        energy = o.optDouble("e", 0.6).toFloat(), complexity = o.optDouble("c", 0.5).toFloat(),
        swing = if (o.has("sw")) o.getDouble("sw").toFloat() else null,
        length = o.optDouble("len", 1.0).toFloat(), withLead = o.optBoolean("lead", true),
    )

    private fun favOf(s: Song) = Favorite(s.code, s.title, s.genre.id, s.bpm.toInt(), s.keyName, s.params, System.currentTimeMillis())

    private fun read(key: String): List<Favorite> = runCatching {
        val arr = JSONArray(prefs.getString(key, "[]"))
        (0 until arr.length()).map { i ->
            val o = arr.getJSONObject(i)
            Favorite(o.getString("code"), o.getString("title"), o.getString("genre"), o.optInt("bpm"), o.optString("key"), paramsOf(o.getJSONObject("p")), o.optLong("t"))
        }
    }.getOrDefault(emptyList())

    private fun write(key: String, list: List<Favorite>) {
        val arr = JSONArray()
        list.forEach { f ->
            arr.put(JSONObject().apply {
                put("code", f.code); put("title", f.title); put("genre", f.genreId); put("bpm", f.bpm); put("key", f.key)
                put("p", f.params.toJson()); put("t", f.time)
            })
        }
        prefs.edit().putString(key, arr.toString()).apply()
    }

    fun favorites() = read("favs")
    fun recents() = read("recents")

    fun isFavorite(s: Song?) = s != null && favorites().any { it.title == s.title && it.params == s.params }

    fun toggleFavorite(s: Song) {
        val list = favorites().toMutableList()
        val idx = list.indexOfFirst { it.title == s.title && it.params == s.params }
        if (idx >= 0) list.removeAt(idx) else list.add(0, favOf(s))
        write("favs", list)
    }

    fun removeFavorite(f: Favorite) = write("favs", favorites().filterNot { it == f })

    fun addRecent(s: Song) {
        val list = recents().filterNot { it.params == s.params }.toMutableList()
        list.add(0, favOf(s))
        write("recents", list.take(30))
    }

    fun saveMixer(l: LiveParams) {
        val o = JSONObject()
        o.put("vol", JSONArray(l.volume.map { it.toDouble() }))
        o.put("master", l.masterVolume.toDouble()); o.put("rev", l.reverb.toDouble()); o.put("dly", l.delay.toDouble())
        o.put("drive", l.drive.toDouble()); o.put("sc", l.sidechain.toDouble()); o.put("lofi", l.lofi.toDouble())
        o.put("vinyl", l.vinyl.toDouble()); o.put("rain", l.rain.toDouble()); o.put("width", l.width.toDouble())
        prefs.edit().putString("mixer", o.toString()).apply()
    }

    fun loadMixer(l: LiveParams) {
        val s = prefs.getString("mixer", null) ?: return
        runCatching {
            val o = JSONObject(s)
            val v = o.getJSONArray("vol")
            for (i in 0 until minOf(v.length(), TrackId.COUNT)) l.volume[i] = v.getDouble(i).toFloat()
            l.masterVolume = o.optDouble("master", 0.9).toFloat(); l.reverb = o.optDouble("rev", 1.0).toFloat()
            l.delay = o.optDouble("dly", 1.0).toFloat(); l.drive = o.optDouble("drive", 1.0).toFloat()
            l.sidechain = o.optDouble("sc", 1.0).toFloat(); l.lofi = o.optDouble("lofi", 1.0).toFloat()
            l.vinyl = o.optDouble("vinyl", -1.0).toFloat(); l.rain = o.optDouble("rain", -1.0).toFloat()
            l.width = o.optDouble("width", 1.0).toFloat()
        }
    }

    fun loadMasterVolume(l: LiveParams) {
        val s = prefs.getString("mixer", null)
        l.masterVolume = if (s == null) 0.9f else runCatching { JSONObject(s).optDouble("master", 0.9).toFloat() }.getOrDefault(0.9f)
    }

    var onboarded: Boolean
        get() = prefs.getBoolean("onboarded", false)
        set(v) { prefs.edit().putBoolean("onboarded", v).apply() }
}
