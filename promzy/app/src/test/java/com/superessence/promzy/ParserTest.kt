package com.superessence.promzy

import com.superessence.promzy.data.Category
import com.superessence.promzy.data.RssParser
import com.superessence.promzy.data.matchesQuery
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class ParserTest {
    @Test
    fun parsesPepperFeed() {
        val deals = RssParser.parse(SampleRss.byteInputStream())
        assertEquals(2, deals.size)
        val ps5 = deals[0]
        assertEquals("deal-123456", ps5.id)
        assertEquals("Amazon", ps5.merchant)
        assertEquals("349,99€", ps5.price)
        assertEquals("449,99€", ps5.oldPrice)
        assertTrue(ps5.image!!.contains("1024x1024"))
        assertEquals("Consoles · Jeux vidéo", ps5.category)
        assertEquals(1254, ps5.temperature)
        assertTrue(ps5.publishedAt > 0)
        assertTrue(ps5.summary.contains("PS5 & ses"))
        assertTrue(Category.GAMING.matches(ps5))
        assertTrue(ps5.matchesQuery("playstation slim"))
        val free = deals[1]
        assertEquals("Gratuit", free.price)
        assertTrue(Category.FREE.matches(free))
        assertTrue(free.matchesQuery("epic"))
    }
}
