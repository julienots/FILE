package com.superessence.egide.vpn

/** Paquet UDP extrait d'un paquet IP (v4 ou v6) lu sur l'interface VPN. */
class UdpPacket(
    val ipv6: Boolean,
    val srcAddr: ByteArray,
    val dstAddr: ByteArray,
    val srcPort: Int,
    val dstPort: Int,
    val payload: ByteArray,
)

object Packets {

    fun parseUdp(buf: ByteArray, len: Int): UdpPacket? {
        if (len < 20) return null
        val version = (buf[0].toInt() ushr 4) and 0xF
        return when (version) {
            4 -> {
                val ihl = (buf[0].toInt() and 0xF) * 4
                if (buf[9].toInt() != 17 || len < ihl + 8) return null
                // fragments ignorés
                val frag = ((buf[6].toInt() and 0x1F) shl 8) or (buf[7].toInt() and 0xFF)
                if (frag != 0 || (buf[6].toInt() and 0x20) != 0) return null
                udp(buf, len, false, buf.copyOfRange(12, 16), buf.copyOfRange(16, 20), ihl)
            }
            6 -> {
                if (len < 48 || buf[6].toInt() != 17) return null
                udp(buf, len, true, buf.copyOfRange(8, 24), buf.copyOfRange(24, 40), 40)
            }
            else -> null
        }
    }

    private fun udp(buf: ByteArray, len: Int, v6: Boolean, src: ByteArray, dst: ByteArray, off: Int): UdpPacket? {
        val sp = u16(buf, off)
        val dp = u16(buf, off + 2)
        val ulen = u16(buf, off + 4)
        val end = minOf(len, off + ulen)
        if (end < off + 8) return null
        return UdpPacket(v6, src, dst, sp, dp, buf.copyOfRange(off + 8, end))
    }

    /** Construit la réponse : adresses et ports inversés par rapport à la requête. */
    fun buildReply(req: UdpPacket, payload: ByteArray): ByteArray {
        val udpLen = 8 + payload.size
        return if (!req.ipv6) {
            val total = 20 + udpLen
            val p = ByteArray(total)
            p[0] = 0x45
            put16(p, 2, total)
            p[6] = 0x40 // ne pas fragmenter
            p[8] = 64
            p[9] = 17
            System.arraycopy(req.dstAddr, 0, p, 12, 4)
            System.arraycopy(req.srcAddr, 0, p, 16, 4)
            put16(p, 10, checksum(p, 0, 20, 0))
            put16(p, 20, req.dstPort); put16(p, 22, req.srcPort); put16(p, 24, udpLen)
            System.arraycopy(payload, 0, p, 28, payload.size)
            // somme de contrôle UDP facultative en IPv4 : laissée à 0
            p
        } else {
            val total = 40 + udpLen
            val p = ByteArray(total)
            p[0] = 0x60
            put16(p, 4, udpLen)
            p[6] = 17
            p[7] = 64
            System.arraycopy(req.dstAddr, 0, p, 8, 16)
            System.arraycopy(req.srcAddr, 0, p, 24, 16)
            put16(p, 40, req.dstPort); put16(p, 42, req.srcPort); put16(p, 44, udpLen)
            System.arraycopy(payload, 0, p, 48, payload.size)
            // pseudo-en-tête IPv6 : adresses + longueur + protocole
            var sum = 0L
            sum = partial(p, 8, 32, sum)
            sum += udpLen + 17
            var c = checksum(p, 40, udpLen, sum)
            if (c == 0) c = 0xFFFF
            put16(p, 46, c)
            p
        }
    }

    private fun partial(b: ByteArray, off: Int, len: Int, start: Long): Long {
        var sum = start
        var i = off
        val end = off + len
        while (i + 1 < end) { sum += u16(b, i); i += 2 }
        if (i < end) sum += (b[i].toInt() and 0xFF) shl 8
        return sum
    }

    private fun checksum(b: ByteArray, off: Int, len: Int, start: Long): Int {
        var sum = partial(b, off, len, start)
        while (sum ushr 16 != 0L) sum = (sum and 0xFFFF) + (sum ushr 16)
        return (sum.inv() and 0xFFFF).toInt()
    }

    fun u16(b: ByteArray, o: Int) = ((b[o].toInt() and 0xFF) shl 8) or (b[o + 1].toInt() and 0xFF)
    fun put16(b: ByteArray, o: Int, v: Int) { b[o] = (v ushr 8).toByte(); b[o + 1] = v.toByte() }
}

/** Lecture minimale d'une requête DNS et fabrication de la réponse « bloquée ». */
class DnsQuery(val name: String, val qtype: Int, private val questionEnd: Int, private val raw: ByteArray) {

    fun blockedResponse(): ByteArray {
        val answer = when (qtype) { 1 -> 4; 28 -> 16; else -> 0 }
        val qLen = questionEnd - 12
        val out = ByteArray(12 + qLen + if (answer > 0) 12 + answer else 0)
        out[0] = raw[0]; out[1] = raw[1]
        // QR=1, opcode conservé, RD conservé ; RA=1, RCODE=0 (NOERROR)
        out[2] = ((raw[2].toInt() and 0x79) or 0x80).toByte()
        out[3] = 0x80.toByte()
        Packets.put16(out, 4, 1)
        Packets.put16(out, 6, if (answer > 0) 1 else 0)
        System.arraycopy(raw, 12, out, 12, qLen)
        if (answer > 0) {
            var o = 12 + qLen
            out[o++] = 0xC0.toByte(); out[o++] = 0x0C
            Packets.put16(out, o, qtype); o += 2
            Packets.put16(out, o, 1); o += 2
            out[o++] = 0; out[o++] = 0; out[o++] = 0x01; out[o++] = 0x2C // TTL 300 s
            Packets.put16(out, o, answer)
            // adresse 0.0.0.0 ou :: (déjà à zéro)
        }
        return out
    }

    companion object {
        fun parse(p: ByteArray): DnsQuery? {
            if (p.size < 17) return null
            if ((p[2].toInt() and 0x80) != 0) return null // c'est une réponse
            if (Packets.u16(p, 4) < 1) return null
            val sb = StringBuilder()
            var i = 12
            while (true) {
                if (i >= p.size) return null
                val l = p[i].toInt() and 0xFF
                if (l == 0) { i++; break }
                if (l and 0xC0 != 0 || i + 1 + l > p.size) return null
                if (sb.isNotEmpty()) sb.append('.')
                for (k in 1..l) sb.append((p[i + k].toInt() and 0xFF).toChar())
                i += l + 1
            }
            if (i + 4 > p.size) return null
            val qtype = Packets.u16(p, i)
            return DnsQuery(sb.toString().lowercase(), qtype, i + 4, p)
        }
    }
}
