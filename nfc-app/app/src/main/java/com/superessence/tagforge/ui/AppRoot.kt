package com.superessence.tagforge.ui

import android.content.Intent
import android.provider.Settings
import androidx.activity.compose.BackHandler
import androidx.compose.animation.AnimatedContent
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.window.Dialog
import androidx.compose.ui.window.DialogProperties
import com.superessence.tagforge.MainActivity
import com.superessence.tagforge.nfc.NfcBus
import com.superessence.tagforge.nfc.RecordItem
import com.superessence.tagforge.nfc.RecordType

enum class Tab(val label: String, val icon: androidx.compose.ui.graphics.vector.ImageVector) {
    READ("Lire", Icons.Filled.Contactless), WRITE("Écrire", Icons.Filled.EditNote),
    TOOLS("Outils", Icons.Filled.Build), LIBRARY("Modèles", Icons.Filled.Bookmarks),
}

sealed class Sub {
    object Guide : Sub()
    object Password : Sub()
    object Emulation : Sub()
    object Settings : Sub()
    object Capacity : Sub()
    object History : Sub()
    object TypePicker : Sub()
    class Editor(val type: RecordType, val initial: RecordItem?, val index: Int) : Sub()
}

object Nav {
    var tab by mutableStateOf(Tab.READ)
    val stack = mutableStateListOf<Sub>()
    fun push(s: Sub) { stack.add(s) }
    fun pop() { if (stack.isNotEmpty()) stack.removeAt(stack.size - 1) }
}

@Composable
fun AppRoot(nfc: MainActivity.NfcState) {
    val ctx = LocalContext.current
    val sub = Nav.stack.lastOrNull()
    BackHandler(enabled = sub != null) { Nav.pop() }

    Scaffold(
        containerColor = MaterialTheme.colorScheme.background,
        bottomBar = {
            if (sub == null) NavigationBar(containerColor = MaterialTheme.colorScheme.surface) {
                Tab.entries.forEach { t ->
                    NavigationBarItem(selected = Nav.tab == t, onClick = { Nav.tab = t }, icon = { Icon(t.icon, null) }, label = { Text(t.label) })
                }
            }
        },
    ) { pad ->
        Column(Modifier.padding(pad).fillMaxSize()) {
            if (nfc == MainActivity.NfcState.DISABLED || nfc == MainActivity.NfcState.MISSING) {
                Row(
                    Modifier.fillMaxWidth().padding(12.dp).background(Danger.copy(alpha = .15f), RoundedCornerShape(14.dp)).padding(12.dp),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Icon(Icons.Filled.Warning, null, tint = Danger)
                    Spacer(Modifier.width(10.dp))
                    Text(if (nfc == MainActivity.NfcState.MISSING) "Ce téléphone n'a pas de puce NFC. Vous pouvez préparer des contenus et des QR codes." else "Le NFC est désactivé.",
                        Modifier.weight(1f), style = MaterialTheme.typography.bodySmall)
                    if (nfc == MainActivity.NfcState.DISABLED) TextButton(onClick = { ctx.startActivity(Intent(Settings.ACTION_NFC_SETTINGS)) }) { Text("Activer") }
                }
            }
            AnimatedContent(sub ?: Nav.tab, label = "screen", modifier = Modifier.weight(1f)) { s ->
                when (s) {
                    Tab.READ -> ReadScreen()
                    Tab.WRITE -> WriteScreen()
                    Tab.TOOLS -> ToolsScreen()
                    Tab.LIBRARY -> LibraryScreen()
                    Sub.Guide -> GuideScreen()
                    Sub.Password -> PasswordScreen()
                    Sub.Emulation -> EmulationScreen()
                    Sub.Settings -> SettingsScreen()
                    Sub.Capacity -> CapacityScreen()
                    Sub.History -> HistoryScreen()
                    Sub.TypePicker -> TypePickerScreen()
                    is Sub.Editor -> RecordEditorScreen(s)
                    else -> {}
                }
            }
        }
    }
    ScanDialog()
}

@Composable
fun ScanDialog() {
    val op = NfcBus.pending
    val res = NfcBus.result
    if (op == null && !(NfcBus.showResult && res != null)) return
    Dialog(onDismissRequest = { NfcBus.cancel() }, properties = DialogProperties(usePlatformDefaultWidth = false)) {
        Column(
            Modifier.padding(20.dp).fillMaxWidth().background(MaterialTheme.colorScheme.surface, RoundedCornerShape(28.dp)).padding(24.dp),
            horizontalAlignment = Alignment.CenterHorizontally,
        ) {
            Text(op?.title ?: NfcBus.lastTitle, style = MaterialTheme.typography.titleLarge, fontWeight = FontWeight.Bold)
            Spacer(Modifier.height(8.dp))
            if (op != null) {
                Box(Modifier.size(170.dp), contentAlignment = Alignment.Center) {
                    NfcPulse(Modifier.fillMaxSize(), color = if (op is com.superessence.tagforge.nfc.NfcOp.Lock) Danger else Teal)
                    if (NfcBus.busy) CircularProgressIndicator()
                }
                Text(op.hint, textAlign = TextAlign.Center, style = MaterialTheme.typography.bodyLarge)
                Text("Zone NFC : généralement au centre du dos, près de l'appareil photo.", textAlign = TextAlign.Center,
                    style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant, modifier = Modifier.padding(top = 4.dp))
            }
            if (res != null) {
                Spacer(Modifier.height(14.dp))
                Row(
                    Modifier.fillMaxWidth().background((if (res.ok) Ok else Danger).copy(alpha = .14f), RoundedCornerShape(14.dp)).padding(12.dp),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Icon(if (res.ok) Icons.Filled.CheckCircle else Icons.Filled.Error, null, tint = if (res.ok) Ok else Danger)
                    Spacer(Modifier.width(10.dp))
                    Text(res.message, style = MaterialTheme.typography.bodyMedium)
                }
            }
            if (NfcBus.count > 0 && (op is com.superessence.tagforge.nfc.NfcOp.Write || op is com.superessence.tagforge.nfc.NfcOp.CloneWrite)) {
                Text("${NfcBus.count} tag(s) écrit(s) — approchez le suivant", Modifier.padding(top = 8.dp), color = Teal, fontWeight = FontWeight.SemiBold)
            }
            Spacer(Modifier.height(16.dp))
            Button(onClick = { NfcBus.cancel() }, modifier = Modifier.fillMaxWidth()) { Text(if (op == null) "OK" else if (NfcBus.count > 0) "Terminer" else "Annuler") }
        }
    }
}
