package com.superessence.tagforge

import com.superessence.tagforge.nfc.Encoders
import com.superessence.tagforge.tasks.TaskCodec
import com.superessence.tagforge.tasks.TaskItem
import com.superessence.tagforge.tasks.TaskType
import org.junit.Assert.*
import org.junit.Test

class EncodersTest {
    @Test fun wifiRoundTrip() {
        val p = Encoders.wifiPayload("MaBox", "motdepasse123", "WPA2")
        val w = Encoders.parseWifi(p)!!
        assertEquals("MaBox", w.ssid)
        assertEquals("motdepasse123", w.password)
        assertEquals("WPA2", w.auth)
    }

    @Test fun bluetoothRoundTrip() {
        val p = Encoders.bluetoothPayload("AA:BB:CC:DD:EE:01", "Enceinte")
        assertEquals(p.size, (p[0].toInt() and 0xFF) or (p[1].toInt() shl 8))
        assertEquals("Enceinte (AA:BB:CC:DD:EE:01)", Encoders.parseBluetooth(p))
    }

    @Test fun tasksRoundTrip() {
        val t = listOf(TaskItem(TaskType.SPEAK, listOf("Bonjour", "fr-FR", "1.0")), TaskItem(TaskType.FLASH, listOf("on")))
        val s = TaskCodec.encode(t)
        assertEquals("""[["sp","Bonjour"],["fl","on"]]""", s)
        val d = TaskCodec.decode(s)
        assertEquals(TaskType.SPEAK, d[0].type)
        assertEquals("fr-FR", d[0].arg(1))
        assertEquals("on", d[1].arg(0))
    }

    @Test fun type2Tlv() {
        val t = Encoders.type2Tlv(ByteArray(10))
        assertEquals(0x03, t[0].toInt()); assertEquals(10, t[1].toInt()); assertEquals(0xFE.toByte(), t[12])
        assertEquals(0, t.size % 4)
        val big = Encoders.type2Tlv(ByteArray(300))
        assertEquals(0xFF.toByte(), big[1]); assertEquals(1, big[2].toInt()); assertEquals(44, big[3].toInt())
    }

    @Test fun password() {
        val (pwd, _) = Encoders.ntagPassword("1A2B3C4D")
        assertArrayEquals(byteArrayOf(0x1A, 0x2B, 0x3C, 0x4D), pwd)
        assertEquals(4, Encoders.ntagPassword("mon secret").first.size)
    }

    @Test fun placeholders() {
        assertEquals("Table 007", Encoders.placeholders("Table {n3}", 7))
        assertEquals("20261224T193000", Encoders.calDate("2026-12-24 19:30"))
    }
}
