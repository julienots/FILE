package com.superessence.voxea.ui

import android.Manifest
import android.content.Intent
import android.content.pm.PackageManager
import android.net.Uri
import android.provider.Settings as AndroidSettings
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.animation.AnimatedContent
import androidx.compose.animation.core.RepeatMode
import androidx.compose.animation.core.animateFloat
import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.core.infiniteRepeatable
import androidx.compose.animation.core.rememberInfiniteTransition
import androidx.compose.animation.core.tween
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.slideInVertically
import androidx.compose.animation.slideOutVertically
import androidx.compose.animation.togetherWith
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.navigationBarsPadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.outlined.ArrowBack
import androidx.compose.material.icons.outlined.CheckCircle
import androidx.compose.material.icons.outlined.Pause
import androidx.compose.material.icons.outlined.PlayArrow
import androidx.compose.material.icons.outlined.Replay
import androidx.compose.material.icons.outlined.SkipPrevious
import androidx.compose.material.icons.outlined.TextDecrease
import androidx.compose.material.icons.outlined.TextIncrease
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.alpha
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalView
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.core.content.ContextCompat

@Composable
fun PrompterScreen(vm: AppViewModel, id: String) {
    val projects by vm.projects.collectAsState()
    val p = projects.firstOrNull { it.id == id } ?: return
    val st by vm.prompter.collectAsState()
    val rv by vm.recVersion.collectAsState()
    val recorded = remember(rv, p) { vm.store.recorded(p) }
    val level by vm.recorder.level.collectAsState()
    val playing by vm.player.playing.collectAsState()
    val ctx = LocalContext.current
    var size by remember { mutableFloatStateOf(vm.settings.prompterSize) }
    var granted by remember { mutableStateOf(ContextCompat.checkSelfPermission(ctx, Manifest.permission.RECORD_AUDIO) == PackageManager.PERMISSION_GRANTED) }
    var denied by remember { mutableStateOf(false) }
    val askMic = rememberLauncherForActivityResult(ActivityResultContracts.RequestPermission()) { ok ->
        granted = ok; denied = !ok
        if (ok) vm.startSession()
    }
    fun start() { if (granted) vm.startSession() else askMic.launch(Manifest.permission.RECORD_AUDIO) }

    // Écran allumé pendant le prompteur ; micro coupé en quittant
    val view = LocalView.current
    DisposableEffect(Unit) {
        view.keepScreenOn = true
        onDispose { view.keepScreenOn = false; vm.stopSession() }
    }

    val line = p.lines.getOrNull(st.index) ?: return
    val next = p.lines.getOrNull(st.index + 1)
    val type = p.voiceTypeFor(line)

    Column(Modifier.fillMaxSize().background(Color(0xFF0B0616)).statusBarsPadding().navigationBarsPadding()) {
        // --- barre du haut ---
        Row(Modifier.fillMaxWidth().padding(horizontal = 4.dp, vertical = 4.dp), verticalAlignment = Alignment.CenterVertically) {
            IconButton(onClick = { vm.back() }) { Icon(Icons.AutoMirrored.Outlined.ArrowBack, "Retour") }
            RecBadge(st.recording, level)
            Spacer(Modifier.width(10.dp))
            Text("Réplique ${st.index + 1} / ${p.lines.size}", style = MaterialTheme.typography.titleMedium, modifier = Modifier.weight(1f))
            IconButton(onClick = { size = (size - 3f).coerceAtLeast(18f); vm.settings.prompterSize = size }) { Icon(Icons.Outlined.TextDecrease, "Texte plus petit", tint = Vx.Muted) }
            IconButton(onClick = { size = (size + 3f).coerceAtMost(54f); vm.settings.prompterSize = size }) { Icon(Icons.Outlined.TextIncrease, "Texte plus grand", tint = Vx.Muted) }
        }
        // barre de progression des répliques
        Row(Modifier.fillMaxWidth().padding(horizontal = 16.dp).height(4.dp), horizontalArrangement = Arrangement.spacedBy(2.dp)) {
            p.lines.forEach { l ->
                Box(Modifier.weight(1f).height(4.dp).clip(CircleShape).background(when {
                    l.index == st.index -> Vx.Magenta
                    l.index in recorded -> Vx.Ok.copy(alpha = 0.7f)
                    else -> Vx.Surface2
                }))
            }
        }

        // --- texte : toucher = suivant ---
        Box(
            Modifier.weight(1f).fillMaxWidth()
                .clickable(remember { MutableInteractionSource() }, null, enabled = st.recording) { vm.prompterNext(p) }
                .padding(horizontal = 22.dp),
        ) {
            if (st.finished) {
                Column(Modifier.align(Alignment.Center), horizontalAlignment = Alignment.CenterHorizontally) {
                    Icon(Icons.Outlined.CheckCircle, null, tint = Vx.Ok, modifier = Modifier.size(64.dp))
                    Text("Bravo, c'est enregistré !", style = MaterialTheme.typography.headlineMedium, modifier = Modifier.padding(top = 12.dp))
                    Text("${recorded.size}/${p.lines.size} répliques", color = Vx.Muted)
                    Spacer(Modifier.height(20.dp))
                    BigAction(Icons.Outlined.PlayArrow, "Choisir les voix et générer", Vx.Magenta) { vm.go(Screen.Edit(p.id, 2)) }
                }
            } else {
                AnimatedContent(
                    st.index, label = "line",
                    transitionSpec = { (slideInVertically { it / 3 } + fadeIn(tween(220))) togetherWith (slideOutVertically { -it / 3 } + fadeOut(tween(160))) },
                ) { idx ->
                    val l = p.lines[idx]
                    val t = p.voiceTypeFor(l)
                    val nx = p.lines.getOrNull(idx + 1)
                    Column(Modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(top = 28.dp, bottom = 24.dp)) {
                        Row(verticalAlignment = Alignment.CenterVertically) {
                            Text(l.character, color = t.color(), fontFamily = Fredoka, fontWeight = FontWeight.Bold, fontSize = 22.sp)
                            Text("  ${t.emoji()} ${t.label}", color = Vx.Muted, style = MaterialTheme.typography.bodyMedium)
                            if (l.index in recorded) Text("  ✓", color = Vx.Ok, fontWeight = FontWeight.Bold)
                        }
                        Spacer(Modifier.height(14.dp))
                        Text(
                            l.text, color = Vx.Cream, fontFamily = Nunito, fontWeight = FontWeight.SemiBold,
                            fontSize = size.sp, lineHeight = (size * 1.32f).sp,
                        )
                        if (nx != null) {
                            Spacer(Modifier.height(34.dp))
                            Text("ENSUITE", color = Vx.Muted, style = MaterialTheme.typography.bodySmall, fontWeight = FontWeight.Bold)
                            Text("${nx.character} · ${p.voiceTypeFor(nx).short}", color = p.voiceTypeFor(nx).color().copy(alpha = 0.7f), style = MaterialTheme.typography.labelLarge)
                            Text(nx.text, color = Vx.Muted.copy(alpha = 0.75f), fontSize = (size * 0.6f).sp, lineHeight = (size * 0.8f).sp,
                                maxLines = 3, overflow = TextOverflow.Ellipsis)
                        }
                    }
                }
            }
        }

        st.message?.let { Text(it, color = Color(0xFFFFB547), style = MaterialTheme.typography.bodyMedium, modifier = Modifier.padding(horizontal = 20.dp)) }
        if (denied) MicDenied {
            ctx.startActivity(Intent(AndroidSettings.ACTION_APPLICATION_DETAILS_SETTINGS, Uri.fromParts("package", ctx.packageName, null)))
        }

        // --- commandes ---
        if (!st.finished) Column(Modifier.fillMaxWidth().padding(16.dp)) {
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceEvenly) {
                Control(Icons.Outlined.SkipPrevious, "Précédente") { vm.prompterPrev(p) }
                Control(Icons.Outlined.Replay, "Refaire") { if (st.recording) vm.prompterRedo() else start() }
                Control(if (playing != null) Icons.Outlined.Pause else Icons.Outlined.PlayArrow, "Réécouter",
                    enabled = line.index in recorded || st.index > 0) {
                    val target = if (line.index in recorded) st.index else st.index - 1
                    vm.replay(p, target)
                }
                if (st.recording) Control(Icons.Outlined.Pause, "Pause") { vm.stopSession() }
            }
            Spacer(Modifier.height(12.dp))
            val last = st.index >= p.lines.lastIndex
            Box(
                Modifier.fillMaxWidth().heightIn(min = 64.dp).clip(RoundedCornerShape(20.dp))
                    .background(if (st.recording) Vx.Magenta else Vx.Rec)
                    .clickable { if (st.recording) vm.prompterNext(p) else start() },
                contentAlignment = Alignment.Center,
            ) {
                Text(
                    when {
                        !st.recording -> "●  ENREGISTRER"
                        last -> "TERMINER  ✓"
                        else -> "SUIVANT  ▶"
                    },
                    color = Color.White, fontFamily = Fredoka, fontWeight = FontWeight.Bold, fontSize = 22.sp,
                )
            }
            Text(
                if (st.recording) "Touche le texte ou un bouton de volume pour passer à la suivante." else "Le micro démarre sur cette réplique.",
                color = Vx.Muted, style = MaterialTheme.typography.bodySmall, textAlign = TextAlign.Center,
                modifier = Modifier.fillMaxWidth().padding(top = 8.dp),
            )
        }
    }

    LaunchedEffect(type) { /* recomposition quand la voix change */ }
}

@Composable
private fun RecBadge(recording: Boolean, level: Float) {
    val inf = rememberInfiniteTransition(label = "rec")
    val blink by inf.animateFloat(1f, 0.25f, infiniteRepeatable(tween(700), RepeatMode.Reverse), label = "blink")
    val lv by animateFloatAsState(level.coerceIn(0f, 1f), tween(80), label = "lv")
    Row(
        Modifier.clip(RoundedCornerShape(50)).background(if (recording) Vx.Rec.copy(alpha = 0.18f) else Vx.Surface2).padding(horizontal = 10.dp, vertical = 6.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Box(Modifier.size(10.dp).alpha(if (recording) blink else 0.4f).clip(CircleShape).background(if (recording) Vx.Rec else Vx.Muted))
        Spacer(Modifier.width(6.dp))
        Text(if (recording) "REC" else "PAUSE", color = if (recording) Vx.Rec else Vx.Muted, fontWeight = FontWeight.Bold, fontSize = 12.sp)
        if (recording) {
            Spacer(Modifier.width(8.dp))
            // Vumètre
            Row(horizontalArrangement = Arrangement.spacedBy(2.dp), verticalAlignment = Alignment.Bottom) {
                for (i in 0 until 5) {
                    val on = lv * 5 > i
                    Box(Modifier.width(3.dp).height((6 + i * 2).dp).clip(CircleShape).background(if (on) Vx.Cyan else Vx.Line))
                }
            }
        }
    }
}

@Composable
private fun Control(icon: androidx.compose.ui.graphics.vector.ImageVector, label: String, enabled: Boolean = true, onClick: () -> Unit) {
    Column(
        Modifier.clip(RoundedCornerShape(14.dp)).clickable(enabled = enabled, onClick = onClick).padding(horizontal = 12.dp, vertical = 6.dp).alpha(if (enabled) 1f else 0.35f),
        horizontalAlignment = Alignment.CenterHorizontally,
    ) {
        Icon(icon, label, tint = Vx.Cream, modifier = Modifier.size(28.dp))
        Text(label, style = MaterialTheme.typography.bodySmall, color = Vx.Muted)
    }
}

@Composable
private fun MicDenied(openSettings: () -> Unit) {
    Column(Modifier.fillMaxWidth().padding(horizontal = 16.dp).clip(RoundedCornerShape(16.dp)).background(Vx.Surface).padding(14.dp)) {
        Text("Le micro est bloqué", style = MaterialTheme.typography.titleMedium)
        Text("VOXÉA a besoin du micro pour enregistrer ta voix. Rien n'est envoyé sur Internet (sauf si tu actives la voix IA).",
            style = MaterialTheme.typography.bodySmall, color = Vx.Muted)
        Text("Ouvrir les réglages", color = Vx.Cyan, style = MaterialTheme.typography.labelLarge,
            modifier = Modifier.padding(top = 8.dp).clickable(onClick = openSettings))
    }
}
