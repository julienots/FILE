package com.superessence.tagforge.hce

import android.nfc.cardemulation.HostApduService
import android.os.Bundle
import com.superessence.tagforge.data.Store

/**
 * Émule un tag NFC Forum Type 4 contenant le message NDEF choisi dans l'appli.
 * Un autre téléphone Android qui approche lit ce contenu comme un vrai tag (lien, Wi-Fi, contact…).
 */
class NdefHceService : HostApduService() {

    private enum class File { NONE, CC, NDEF }
    private var selected = File.NONE
    private var appSelected = false

    override fun onCreate() {
        super.onCreate()
        Store.init(this)
    }

    override fun processCommandApdu(apdu: ByteArray, extras: Bundle?): ByteArray {
        val ndef = Store.emulationNdef
        if (!Store.emulationOn || ndef == null) return NOT_FOUND
        val file = byteArrayOf((ndef.size shr 8).toByte(), ndef.size.toByte()) + ndef

        if (apdu.size >= 4 && apdu[0] == 0x00.toByte() && apdu[1] == 0xA4.toByte()) {
            // SELECT
            val data = if (apdu.size > 5) apdu.copyOfRange(5, 5 + (apdu[4].toInt() and 0xFF).coerceAtMost(apdu.size - 5)) else ByteArray(0)
            return when {
                apdu[2] == 0x04.toByte() && data.contentEquals(AID) -> { appSelected = true; selected = File.NONE; OK }
                appSelected && data.contentEquals(byteArrayOf(0xE1.toByte(), 0x03)) -> { selected = File.CC; OK }
                appSelected && data.contentEquals(byteArrayOf(0xE1.toByte(), 0x04)) -> { selected = File.NDEF; OK }
                else -> NOT_FOUND
            }
        }
        if (apdu.size >= 5 && apdu[0] == 0x00.toByte() && apdu[1] == 0xB0.toByte()) {
            // READ BINARY
            val offset = ((apdu[2].toInt() and 0xFF) shl 8) or (apdu[3].toInt() and 0xFF)
            val le = (apdu[4].toInt() and 0xFF).let { if (it == 0) 256 else it }
            val src = when (selected) { File.CC -> CC; File.NDEF -> file; else -> return NOT_FOUND }
            if (offset > src.size) return WRONG_P1P2
            return src.copyOfRange(offset, (offset + le).coerceAtMost(src.size)) + OK
        }
        if (apdu.size >= 2 && apdu[1] == 0xD6.toByte()) return DENIED // UPDATE BINARY : lecture seule
        return UNKNOWN
    }

    override fun onDeactivated(reason: Int) {
        selected = File.NONE
        appSelected = false
    }

    companion object {
        val AID = byteArrayOf(0xD2.toByte(), 0x76, 0x00, 0x00, 0x85.toByte(), 0x01, 0x01)
        val OK = byteArrayOf(0x90.toByte(), 0x00)
        val NOT_FOUND = byteArrayOf(0x6A, 0x82.toByte())
        val WRONG_P1P2 = byteArrayOf(0x6B, 0x00)
        val DENIED = byteArrayOf(0x69, 0x82.toByte())
        val UNKNOWN = byteArrayOf(0x6D, 0x00)

        /** Capability Container : version 2.0, MLe 0x00FF, MLc 0x00FF, fichier NDEF E104 de 0x7FFF octets max, lecture libre, écriture interdite. */
        val CC = byteArrayOf(
            0x00, 0x0F, 0x20, 0x00, 0xFF.toByte(), 0x00, 0xFF.toByte(),
            0x04, 0x06, 0xE1.toByte(), 0x04, 0x7F, 0xFF.toByte(), 0x00, 0xFF.toByte(),
        )
    }
}
