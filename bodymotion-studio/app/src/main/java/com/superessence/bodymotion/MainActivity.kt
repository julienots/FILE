package com.superessence.bodymotion

import android.content.ClipData
import android.content.ClipboardManager
import android.content.Intent
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import com.superessence.bodymotion.data.CrashGuard
import com.superessence.bodymotion.ui.AppNav
import com.superessence.bodymotion.ui.theme.BmTheme
import com.superessence.bodymotion.ui.theme.C
import com.superessence.bodymotion.ui.theme.T

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        setContent {
            BmTheme {
                Box(Modifier.fillMaxSize().background(C.Bg)) { AppNav() }
                CrashReportDialog()
            }
        }
    }
}

/** Rapport du plantage précédent : à copier ou partager pour le diagnostic. */
@Composable
private fun CrashReportDialog() {
    val ctx = LocalContext.current
    var report by remember { mutableStateOf(CrashGuard.pendingReport()) }
    val text = report ?: return
    fun close() { CrashGuard.clearReport(); report = null }
    AlertDialog(
        onDismissRequest = ::close, containerColor = C.Panel,
        title = { Text("L'application s'est fermée", style = T.h2) },
        text = {
            Column(verticalArrangement = Arrangement.spacedBy(10.dp)) {
                Text("Voici le rapport du problème. Envoyez-le pour qu'il soit corrigé. La détection sur GPU a été désactivée par sécurité si elle était en cause.", style = T.small)
                Column(Modifier.heightIn(max = 320.dp).verticalScroll(rememberScrollState()).background(C.Bg).padding(8.dp)) {
                    Text(text, style = T.mono.copy(color = C.Text2))
                }
            }
        },
        confirmButton = {
            Row {
                TextButton({
                    ctx.getSystemService(ClipboardManager::class.java)?.setPrimaryClip(ClipData.newPlainText("Rapport BodyMotion", text))
                }) { Text("Copier") }
                TextButton({
                    ctx.startActivity(Intent.createChooser(Intent(Intent.ACTION_SEND).apply { type = "text/plain"; putExtra(Intent.EXTRA_TEXT, text) }, "Envoyer le rapport"))
                }) { Text("Partager") }
            }
        },
        dismissButton = { TextButton(::close) { Text("Fermer") } },
    )
}
