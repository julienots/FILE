package com.superessence.promzy.data

import android.content.Context
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.serialization.builtins.ListSerializer
import kotlinx.serialization.json.Json

class Favorites(context: Context) {
    private val prefs = context.getSharedPreferences("promzy", Context.MODE_PRIVATE)
    private val json = Json { ignoreUnknownKeys = true }
    private val _items = MutableStateFlow(
        runCatching { json.decodeFromString(ListSerializer(Deal.serializer()), prefs.getString("favs", "[]")!!) }.getOrDefault(emptyList())
    )
    val items: StateFlow<List<Deal>> = _items

    fun toggle(d: Deal) {
        val cur = _items.value
        _items.value = if (cur.any { it.id == d.id }) cur.filterNot { it.id == d.id } else listOf(d) + cur
        prefs.edit().putString("favs", json.encodeToString(ListSerializer(Deal.serializer()), _items.value)).apply()
    }

    var recentSearches: List<String>
        get() = prefs.getString("recent", "")!!.split('\n').filter { it.isNotBlank() }
        set(v) { prefs.edit().putString("recent", v.take(8).joinToString("\n")).apply() }
}
