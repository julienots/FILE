package com.superessence.pleinphare

import com.superessence.pleinphare.data.Song
import com.superessence.pleinphare.data.filterSongs
import com.superessence.pleinphare.data.formatDuration
import com.superessence.pleinphare.data.groupByDay
import com.superessence.pleinphare.data.sortByDate
import org.junit.Assert.assertEquals
import org.junit.Test
import java.time.LocalDate
import java.time.ZoneOffset

class DateGroupsTest {
    private val zone = ZoneOffset.UTC
    private fun at(date: String, hour: Int) = LocalDate.parse(date).atTime(hour, 0).toEpochSecond(zone)
    private fun song(id: Long, title: String, artist: String, sec: Long) = Song(id, title, artist, "", 0, 200_000, sec)

    private val songs = listOf(
        song(1, "Ancienne", "Édith", at("2025-03-02", 10)),
        song(2, "Matin", "Zaz", at("2026-11-27", 8)),
        song(3, "Soir", "Stromae", at("2026-11-27", 21)),
        song(4, "Hier", "Angèle", at("2026-11-26", 12)),
        song(5, "Novembre", "Orelsan", at("2026-11-12", 12)),
    )

    @Test fun groupsNewestFirstWithFrenchLabels() {
        val g = groupByDay(sortByDate(songs, true), today = LocalDate.parse("2026-11-27"), zone = zone)
        assertEquals(listOf("Aujourd'hui", "Hier", "Jeudi 12 novembre", "Dimanche 2 mars 2025"), g.map { it.label })
        assertEquals(listOf(3L, 2L), g[0].songs.map { it.id })
    }

    @Test fun oldestFirst() {
        val ids = sortByDate(songs, false).map { it.id }
        assertEquals(listOf(1L, 5L, 4L, 2L, 3L), ids)
    }

    @Test fun searchIgnoresAccentsAndCase() {
        assertEquals(listOf(1L), filterSongs(songs, "edith").map { it.id })
        assertEquals(listOf(4L), filterSongs(songs, "ANGELE hier").map { it.id })
        assertEquals(5, filterSongs(songs, "  ").size)
    }

    @Test fun durations() {
        assertEquals("3:20", formatDuration(200_000))
        assertEquals("1:02:03", formatDuration(3_723_000))
    }
}
