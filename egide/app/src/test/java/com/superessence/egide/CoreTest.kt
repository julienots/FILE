package com.superessence.egide

import com.superessence.egide.core.DomainSet
import com.superessence.egide.vpn.DnsQuery
import com.superessence.egide.vpn.Packets
import org.junit.Assert.*
import org.junit.Test

class CoreTest {
    @Test fun parseFormats() {
        assertEquals("ads.com", DomainSet.parseLine("0.0.0.0 ads.com"))
        assertEquals("ads.com", DomainSet.parseLine("127.0.0.1\tADS.com # pub"))
        assertEquals("ads.com", DomainSet.parseLine("||ads.com^"))
        assertEquals("ads.com", DomainSet.parseLine("*.ads.com"))
        assertNull(DomainSet.parseLine("# commentaire"))
        assertNull(DomainSet.parseLine("0.0.0.0 localhost"))
    }

    @Test fun suffixMatching() {
        val b = DomainSet.Builder(); b.add("doubleclick.net"); b.add("evil.co")
        val s = b.build()
        assertTrue(s.matches("doubleclick.net"))
        assertTrue(s.matches("ad.g.doubleclick.net"))
        assertFalse(s.matches("notdoubleclick.net"))
        assertFalse(s.matches("net"))
        assertFalse(s.matches("youtube.com"))
    }

    private fun query(name: String, type: Int): ByteArray {
        val out = java.io.ByteArrayOutputStream()
        out.write(byteArrayOf(0x12, 0x34, 0x01, 0x00, 0, 1, 0, 0, 0, 0, 0, 0))
        for (l in name.split('.')) { out.write(l.length); out.write(l.toByteArray()) }
        out.write(0); out.write(0); out.write(type); out.write(0); out.write(1)
        return out.toByteArray()
    }

    @Test fun dnsBlockedAnswer() {
        val q = DnsQuery.parse(query("ads.example.com", 1))!!
        assertEquals("ads.example.com", q.name)
        assertEquals(1, q.qtype)
        val r = q.blockedResponse()
        assertEquals(0x12, r[0].toInt()); assertEquals(0x34, r[1].toInt())
        assertTrue(r[2].toInt() and 0x80 != 0)      // réponse
        assertEquals(1, Packets.u16(r, 6))           // 1 réponse
        assertEquals(4, Packets.u16(r, r.size - 6))  // rdlength 4
        assertTrue(r.takeLast(4).all { it == 0.toByte() })
    }

    @Test fun ipv4RoundTrip() {
        val payload = query("x.org", 28)
        val ip = ByteArray(28 + payload.size)
        ip[0] = 0x45; Packets.put16(ip, 2, ip.size); ip[8] = 64; ip[9] = 17
        byteArrayOf(10, 111, 222.toByte(), 2).copyInto(ip, 12)
        byteArrayOf(10, 111, 222.toByte(), 53).copyInto(ip, 16)
        Packets.put16(ip, 20, 40000); Packets.put16(ip, 22, 53); Packets.put16(ip, 24, 8 + payload.size)
        payload.copyInto(ip, 28)
        val p = Packets.parseUdp(ip, ip.size)!!
        assertEquals(53, p.dstPort)
        val reply = Packets.buildReply(p, DnsQuery.parse(p.payload)!!.blockedResponse())
        val back = Packets.parseUdp(reply, reply.size)!!
        assertEquals(40000, back.dstPort); assertEquals(53, back.srcPort)
        assertArrayEquals(p.srcAddr, back.dstAddr)
        // somme de contrôle IPv4 valide
        var sum = 0; for (i in 0 until 20 step 2) sum += Packets.u16(reply, i)
        while (sum shr 16 != 0) sum = (sum and 0xFFFF) + (sum shr 16)
        assertEquals(0xFFFF, sum)
    }
}
