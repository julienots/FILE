package com.superessence.tagforge.nfc

import android.nfc.NdefMessage
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue

/** Opération en attente du prochain tag approché. */
sealed class NfcOp(val title: String, val hint: String) {
    class Write(
        val produce: (Int) -> NdefMessage,
        val batch: Boolean,
        val startN: Int,
        val authPassword: String?,
        val lock: Boolean,
        val newPassword: String?,
        val protectRead: Boolean,
        val label: String,
    ) : NfcOp(if (batch) "Écriture en série" else "Écriture", "Approchez le tag à écrire au dos du téléphone")
    object Erase : NfcOp("Effacer", "Approchez le tag à effacer")
    object Format : NfcOp("Formater", "Approchez le tag à formater en NDEF")
    object Lock : NfcOp("Verrouiller définitivement", "Approchez le tag : il ne pourra PLUS JAMAIS être modifié")
    class SetPassword(val password: String, val protectRead: Boolean, val current: String?) : NfcOp("Définir un mot de passe", "Approchez le tag NTAG")
    class RemovePassword(val password: String) : NfcOp("Retirer le mot de passe", "Approchez le tag protégé")
    object CloneRead : NfcOp("Copier — étape 1/2", "Approchez le tag SOURCE à copier")
    class CloneWrite(val message: NdefMessage, val batch: Boolean) : NfcOp("Copier — étape 2/2", "Approchez le tag DESTINATION")
    object Dump : NfcOp("Lecture complète", "Approchez le tag pour lire toute sa mémoire")
    object Emulate : NfcOp("Charger pour émulation", "Approchez le tag dont le contenu doit être émulé")
}

data class OpResult(val ok: Boolean, val message: String)

object NfcBus {
    var pending by mutableStateOf<NfcOp?>(null)
    var result by mutableStateOf<OpResult?>(null)
    var count by mutableIntStateOf(0)
    var lastTag by mutableStateOf<TagInfo?>(null)
    var busy by mutableStateOf(false)
    var showResult by mutableStateOf(false)
    /** Vrai sur l'écran d'émulation : la lecture est suspendue pour laisser le téléphone se comporter comme un tag. */
    var emulating by mutableStateOf(false)
    var lastTitle by mutableStateOf("")
    /** Contenu lu à l'étape 1 de la copie. */
    var cloneSource: NdefMessage? = null

    fun start(op: NfcOp) { result = null; count = 0; showResult = false; lastTitle = op.title; pending = op }
    fun cancel() { pending = null; busy = false; showResult = false }
}
