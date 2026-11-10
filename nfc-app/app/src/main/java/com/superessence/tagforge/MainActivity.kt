package com.superessence.tagforge

import android.media.AudioManager
import android.media.ToneGenerator
import android.nfc.NfcAdapter
import android.nfc.Tag
import android.os.Bundle
import android.os.VibrationEffect
import android.os.Vibrator
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.compose.runtime.snapshotFlow
import androidx.lifecycle.lifecycleScope
import kotlinx.coroutines.launch
import com.superessence.tagforge.data.HistoryEntry
import com.superessence.tagforge.data.Store
import com.superessence.tagforge.nfc.NfcBus
import com.superessence.tagforge.nfc.NfcOp
import com.superessence.tagforge.nfc.OpResult
import com.superessence.tagforge.nfc.TagOps
import com.superessence.tagforge.ui.AppRoot
import com.superessence.tagforge.ui.TagForgeTheme

class MainActivity : ComponentActivity() {

    private var adapter: NfcAdapter? = null
    var nfcState by mutableStateOf(NfcState.UNKNOWN)
        private set

    enum class NfcState { UNKNOWN, MISSING, DISABLED, READY }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        Store.init(this)
        adapter = NfcAdapter.getDefaultAdapter(this)
        setContent { TagForgeTheme { AppRoot(nfcState) } }
        lifecycleScope.launch {
            snapshotFlow { NfcBus.emulating }.collect { applyReader() }
        }
    }

    private var resumed = false

    override fun onResume() {
        super.onResume()
        resumed = true
        val a = adapter
        nfcState = when {
            a == null -> NfcState.MISSING
            !a.isEnabled -> NfcState.DISABLED
            else -> NfcState.READY
        }
        applyReader()
    }

    private fun applyReader() {
        val a = adapter ?: return
        if (!resumed || NfcBus.emulating) { runCatching { a.disableReaderMode(this) }; return }
        if (a.isEnabled) {
            val flags = NfcAdapter.FLAG_READER_NFC_A or NfcAdapter.FLAG_READER_NFC_B or NfcAdapter.FLAG_READER_NFC_F or
                NfcAdapter.FLAG_READER_NFC_V or NfcAdapter.FLAG_READER_NFC_BARCODE or NfcAdapter.FLAG_READER_NO_PLATFORM_SOUNDS
            val opts = Bundle().apply { putInt(NfcAdapter.EXTRA_READER_PRESENCE_CHECK_DELAY, 250) }
            a.enableReaderMode(this, { tag -> onTag(tag) }, flags, opts)
        }
    }

    override fun onPause() {
        super.onPause()
        resumed = false
        runCatching { adapter?.disableReaderMode(this) }
    }

    /** Appelé sur un fil NFC (hors fil principal) : on peut faire les E/S ici. */
    private fun onTag(tag: Tag) {
        val op = NfcBus.pending
        runOnUiThread { NfcBus.busy = true }
        val uid = com.superessence.tagforge.nfc.Encoders.hex(tag.id, ":")
        val result: OpResult = try {
            when (op) {
                null, NfcOp.Dump -> {
                    val info = TagOps.read(tag, dumpMemory = op == NfcOp.Dump || Store.autoDump)
                    runOnUiThread {
                        NfcBus.lastTag = info
                        Store.addHistory(HistoryEntry(info.time, "Lecture", info.uid, info.chip, info.records.firstOrNull()?.let { it.kind + " : " + it.value.lineSequence().first() } ?: "Tag vide",
                            info.records.joinToString("\n") { it.kind + " : " + it.value }))
                    }
                    OpResult(true, "Tag lu : ${info.records.size} enregistrement(s)")
                }
                is NfcOp.Write -> {
                    val n = op.startN + NfcBus.count
                    val msg = op.produce(n)
                    val r = TagOps.write(tag, msg, op.authPassword, op.lock, op.newPassword, op.protectRead)
                    runOnUiThread { Store.addHistory(HistoryEntry(System.currentTimeMillis(), "Écriture", uid, "", op.label + if (op.batch) " (#$n)" else "", r)) }
                    OpResult(true, if (op.batch) "Tag n°$n écrit. $r" else r)
                }
                NfcOp.Erase -> OpResult(true, TagOps.erase(tag))
                NfcOp.Format -> OpResult(true, TagOps.format(tag))
                NfcOp.Lock -> OpResult(true, TagOps.lock(tag))
                is NfcOp.SetPassword -> { TagOps.setPassword(tag, op.password, op.protectRead, op.current); OpResult(true, "Mot de passe activé. Notez-le bien : il sera demandé pour modifier ce tag.") }
                is NfcOp.RemovePassword -> { TagOps.removePassword(tag, op.password); OpResult(true, "Protection retirée.") }
                NfcOp.CloneRead -> {
                    val m = TagOps.readMessage(tag)
                    NfcBus.cloneSource = m
                    runOnUiThread { NfcBus.pending = NfcOp.CloneWrite(m, batch = true) }
                    OpResult(true, "Source lue (${m.byteArrayLength} octets). Approchez maintenant le tag destination.")
                }
                is NfcOp.CloneWrite -> OpResult(true, "Copie ${NfcBus.count + 1} écrite. " + TagOps.write(tag, op.message, null, false, null, false))
                NfcOp.Emulate -> {
                    val m = TagOps.readMessage(tag)
                    Store.emulationNdef = m.toByteArray(); Store.emulationLabel = "Copie du tag $uid"
                    runOnUiThread { Store.saveSettings() }
                    OpResult(true, "Contenu chargé pour l'émulation.")
                }
            }
        } catch (e: Throwable) {
            OpResult(false, TagOps.humanError(e))
        }
        feedback(result.ok)
        runOnUiThread {
            NfcBus.busy = false
            NfcBus.result = result
            if (op != null) NfcBus.showResult = true
            if (result.ok) NfcBus.count++
            val keep = when (op) {
                is NfcOp.Write -> op.batch || !result.ok
                is NfcOp.CloneWrite -> true
                NfcOp.CloneRead -> !result.ok || NfcBus.pending is NfcOp.CloneWrite
                null -> false
                else -> !result.ok
            }
            if (!keep && op != null) NfcBus.pending = null
            if (op is NfcOp.CloneRead && result.ok) NfcBus.count = 0
        }
    }

    private fun feedback(ok: Boolean) {
        if (Store.vibrate) runCatching {
            getSystemService(Vibrator::class.java).vibrate(
                if (ok) VibrationEffect.createOneShot(60, VibrationEffect.DEFAULT_AMPLITUDE)
                else VibrationEffect.createWaveform(longArrayOf(0, 80, 80, 80), -1),
            )
        }
        if (Store.sound) runCatching {
            val tg = ToneGenerator(AudioManager.STREAM_NOTIFICATION, 70)
            tg.startTone(if (ok) ToneGenerator.TONE_PROP_ACK else ToneGenerator.TONE_PROP_NACK, 180)
            android.os.Handler(mainLooper).postDelayed({ tg.release() }, 400)
        }
    }
}
