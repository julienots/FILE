package com.superessence.tagforge.tasks

import android.content.Context
import android.content.Intent
import android.net.Uri
import android.nfc.NdefMessage
import android.nfc.NfcAdapter
import android.os.Build
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Bolt
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import androidx.lifecycle.lifecycleScope
import com.superessence.tagforge.data.HistoryEntry
import com.superessence.tagforge.data.Store
import com.superessence.tagforge.nfc.Encoders
import com.superessence.tagforge.nfc.NdefParser
import com.superessence.tagforge.nfc.RecordTypes
import com.superessence.tagforge.ui.TagForgeTheme
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch

/**
 * Lancée par Android dès qu'un tag TagForge est approché (appli fermée, écran déverrouillé).
 * Exécute les tâches, ouvre ensuite les éventuels liens du tag, puis se ferme toute seule.
 */
class TaskActivity : ComponentActivity() {

    private var step by mutableStateOf("Lecture du tag…")
    private var started = false

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        Store.init(this)
        setContent {
            TagForgeTheme {
                Box(Modifier.fillMaxSize().padding(24.dp), contentAlignment = Alignment.BottomCenter) {
                    Row(
                        Modifier.fillMaxWidth().background(MaterialTheme.colorScheme.surface, RoundedCornerShape(20.dp)).padding(16.dp),
                        verticalAlignment = Alignment.CenterVertically,
                    ) {
                        Icon(Icons.Filled.Bolt, null, tint = MaterialTheme.colorScheme.primary)
                        Spacer(Modifier.width(12.dp))
                        Column(Modifier.weight(1f)) {
                            Text("TagForge", style = MaterialTheme.typography.labelMedium, color = MaterialTheme.colorScheme.primary)
                            Text(step, style = MaterialTheme.typography.bodyMedium)
                        }
                        CircularProgressIndicator(Modifier.size(22.dp), strokeWidth = 2.dp)
                    }
                }
            }
        }
        handle(intent)
    }

    override fun onNewIntent(intent: Intent) {
        super.onNewIntent(intent)
        started = false
        handle(intent)
    }

    private fun handle(intent: Intent) {
        if (started) return
        started = true
        val messages = extractMessages(intent)
        val direct = intent.getStringExtra(EXTRA_TASKS)
        val tasks = if (direct != null) TaskCodec.decode(direct) else messages.flatMap { m ->
            m.records.filter { RecordTypes.isTagForge(it) }.flatMap { TaskCodec.decode(String(it.payload, Charsets.UTF_8)) }
        }
        val links = messages.flatMap { NdefParser.parse(it) }.mapNotNull { it.uri }
        val tag: android.nfc.Tag? = if (Build.VERSION.SDK_INT >= 33) intent.getParcelableExtra(NfcAdapter.EXTRA_TAG, android.nfc.Tag::class.java)
        else @Suppress("DEPRECATION") intent.getParcelableExtra(NfcAdapter.EXTRA_TAG)

        lifecycleScope.launch {
            val log = TaskRunner(this@TaskActivity).run(tasks) { step = it }
            links.forEach { u -> runCatching { startActivity(Intent(Intent.ACTION_VIEW, Uri.parse(u)).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)) } }
            if (direct == null) Store.addHistory(HistoryEntry(System.currentTimeMillis(), "Tâches", Encoders.hex(tag?.id, ":"), "", "${tasks.size} tâche(s)", log.joinToString("\n")))
            step = log.lastOrNull() ?: "Aucune tâche"
            delay(600)
            finish()
        }
    }

    private fun extractMessages(intent: Intent): List<NdefMessage> {
        val raw = if (Build.VERSION.SDK_INT >= 33) intent.getParcelableArrayExtra(NfcAdapter.EXTRA_NDEF_MESSAGES, NdefMessage::class.java)?.toList()
        else @Suppress("DEPRECATION") intent.getParcelableArrayExtra(NfcAdapter.EXTRA_NDEF_MESSAGES)?.mapNotNull { it as? NdefMessage }
        return raw ?: emptyList()
    }

    companion object {
        const val EXTRA_TASKS = "tasks"
        fun launch(ctx: Context, tasks: List<TaskItem>) {
            ctx.startActivity(Intent(ctx, TaskActivity::class.java).putExtra(EXTRA_TASKS, TaskCodec.encode(tasks)).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK))
        }
    }
}
