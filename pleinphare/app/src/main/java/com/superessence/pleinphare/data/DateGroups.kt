package com.superessence.pleinphare.data

import java.text.Normalizer
import java.time.Instant
import java.time.LocalDate
import java.time.ZoneId
import java.time.format.DateTimeFormatter
import java.util.Locale

/** Une journée de la liste : « Aujourd'hui », « Hier », « Jeudi 12 novembre »… */
data class DayGroup(val day: LocalDate, val label: String, val songs: List<Song>)

private val FR = Locale.FRENCH
private val SAME_YEAR = DateTimeFormatter.ofPattern("EEEE d MMMM", FR)
private val OTHER_YEAR = DateTimeFormatter.ofPattern("EEEE d MMMM yyyy", FR)
private val MONTH = DateTimeFormatter.ofPattern("MMMM yyyy", FR)

/** Tri par date d'ajout ; à date égale, par titre pour un ordre stable. */
fun sortByDate(songs: List<Song>, newestFirst: Boolean): List<Song> {
    val cmp = compareBy<Song> { it.dateAddedSec }.thenBy { it.title.lowercase(FR) }
    return songs.sortedWith(if (newestFirst) cmp.reversed() else cmp)
}

fun Song.day(zone: ZoneId = ZoneId.systemDefault()): LocalDate =
    Instant.ofEpochSecond(dateAddedSec).atZone(zone).toLocalDate()

fun dayLabel(day: LocalDate, today: LocalDate): String = when (day) {
    today -> "Aujourd'hui"
    today.minusDays(1) -> "Hier"
    else -> (if (day.year == today.year) SAME_YEAR else OTHER_YEAR).format(day).capitalizeFr()
}

fun monthLabel(day: LocalDate): String = MONTH.format(day).capitalizeFr()

/** Regroupe une liste déjà triée par jour, en conservant l'ordre. */
fun groupByDay(
    sorted: List<Song>,
    today: LocalDate = LocalDate.now(),
    zone: ZoneId = ZoneId.systemDefault(),
): List<DayGroup> {
    val out = ArrayList<DayGroup>()
    var current: LocalDate? = null
    var bucket = ArrayList<Song>()
    for (s in sorted) {
        val d = s.day(zone)
        if (d != current) {
            current?.let { out += DayGroup(it, dayLabel(it, today), bucket) }
            current = d
            bucket = ArrayList()
        }
        bucket += s
    }
    current?.let { out += DayGroup(it, dayLabel(it, today), bucket) }
    return out
}

/** Recherche insensible à la casse et aux accents (titre, artiste, album). */
fun filterSongs(songs: List<Song>, query: String): List<Song> {
    val q = query.normalizeSearch()
    if (q.isBlank()) return songs
    val words = q.split(' ').filter { it.isNotBlank() }
    return songs.filter { s ->
        val hay = "${s.title} ${s.artist} ${s.album}".normalizeSearch()
        words.all { hay.contains(it) }
    }
}

fun String.normalizeSearch(): String =
    Normalizer.normalize(lowercase(FR), Normalizer.Form.NFD).replace(Regex("\\p{Mn}+"), "").trim()

private fun String.capitalizeFr() = replaceFirstChar { it.titlecase(FR) }

fun formatDuration(ms: Long): String {
    val total = (ms / 1000).coerceAtLeast(0)
    val h = total / 3600
    val m = (total % 3600) / 60
    val s = total % 60
    return if (h > 0) "%d:%02d:%02d".format(h, m, s) else "%d:%02d".format(m, s)
}
