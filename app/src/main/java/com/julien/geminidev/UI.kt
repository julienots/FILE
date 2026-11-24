package com.julien.geminidev

import androidx.activity.compose.BackHandler
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.imePadding
import androidx.compose.foundation.layout.navigationBarsPadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.selection.SelectionContainer
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Surface
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalUriHandler
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.lifecycle.viewmodel.compose.viewModel

private val Bg = Color(0xFF0B0F14)
private val Panel = Color(0xFF131A23)
private val Accent = Color(0xFF7AA2FF)
private val Green = Color(0xFF5AE0A0)
private val Muted = Color(0xFF8A97A8)
private val Red = Color(0xFFFF7A7A)
private val UserBubble = Color(0xFF22345E)
private val Outline = Color(0xFF2A3544)

@Composable
fun App() {
    val vm: MainVM = viewModel()
    MaterialTheme(
        colorScheme = darkColorScheme(
            primary = Accent,
            onPrimary = Bg,
            background = Bg,
            onBackground = Color(0xFFE6EDF5),
            surface = Panel,
            onSurface = Color(0xFFE6EDF5),
            surfaceVariant = Panel,
            onSurfaceVariant = Muted,
            outline = Outline,
        )
    ) {
        Surface(Modifier.fillMaxSize(), color = Bg) {
            Box(
                Modifier
                    .fillMaxSize()
                    .statusBarsPadding()
                    .navigationBarsPadding()
                    .imePadding()
            ) {
                when (vm.screen) {
                    Screen.SETUP -> SetupScreen(vm)
                    Screen.REPOS -> ReposScreen(vm)
                    Screen.CHAT -> ChatScreen(vm)
                }
            }
        }
    }
}

// ---------------------------------------------------------------- Réglages

@Composable
fun SetupScreen(vm: MainVM) {
    val uri = LocalUriHandler.current
    var gh by remember { mutableStateOf(vm.store.ghToken) }
    var key by remember { mutableStateOf(vm.store.geminiKey) }
    var model by remember { mutableStateOf(vm.store.model) }
    val configured = vm.store.ghToken.isNotBlank() && vm.store.geminiKey.isNotBlank()
    if (configured) BackHandler { vm.closeSettings() }

    Column(
        Modifier
            .fillMaxSize()
            .verticalScroll(rememberScrollState())
            .padding(20.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp),
    ) {
        Text("Gemini Dev", fontSize = 28.sp, fontWeight = FontWeight.Bold, color = Accent)
        Text("Configuration — une seule fois", color = Muted)

        Text("1. GitHub", fontWeight = FontWeight.SemiBold)
        Text(
            "Crée un token (case « repo » déjà cochée), mets l'expiration sur « No expiration », " +
                "descends en bas, « Generate token », copie-le et colle-le ici.",
            color = Muted, fontSize = 13.sp,
        )
        OutlinedButton(
            onClick = { uri.openUri("https://github.com/settings/tokens/new?scopes=repo&description=Gemini%20Dev") },
            modifier = Modifier.fillMaxWidth(),
        ) { Text("Créer mon token GitHub") }
        OutlinedTextField(
            value = gh,
            onValueChange = { gh = it },
            label = { Text("Token GitHub") },
            singleLine = true,
            visualTransformation = PasswordVisualTransformation(),
            modifier = Modifier.fillMaxWidth(),
        )

        Text("2. Gemini (gratuit)", fontWeight = FontWeight.SemiBold)
        Text(
            "Sur AI Studio : « Create API key », copie-la et colle-la ici. Aucune carte bancaire.",
            color = Muted, fontSize = 13.sp,
        )
        OutlinedButton(
            onClick = { uri.openUri("https://aistudio.google.com/apikey") },
            modifier = Modifier.fillMaxWidth(),
        ) { Text("Obtenir ma clé Gemini gratuite") }
        OutlinedTextField(
            value = key,
            onValueChange = { key = it },
            label = { Text("Clé Gemini") },
            singleLine = true,
            visualTransformation = PasswordVisualTransformation(),
            modifier = Modifier.fillMaxWidth(),
        )

        Text("3. Modèle", fontWeight = FontWeight.SemiBold)
        OutlinedTextField(
            value = model,
            onValueChange = { model = it },
            label = { Text("Nom du modèle") },
            singleLine = true,
            modifier = Modifier.fillMaxWidth(),
        )
        Text(
            "Gratuits : gemini-3.8-flash, gemini-3.6-flash. " +
                "gemini-4-argon fonctionne seulement avec l'API payante : ton abonnement Google " +
                "te le donne dans l'app Gemini, pas ici.",
            color = Muted, fontSize = 13.sp,
        )

        vm.message?.let { Text(it, color = Red) }

        Button(
            onClick = { vm.saveSetup(gh, key, model) },
            enabled = !vm.busy,
            modifier = Modifier.fillMaxWidth(),
        ) { Text(if (vm.busy) "Vérification…" else "Enregistrer et continuer") }

        if (configured) {
            TextButton(onClick = { vm.closeSettings() }, modifier = Modifier.fillMaxWidth()) { Text("Retour") }
        }
    }
}

// ---------------------------------------------------------------- Dépôts

@Composable
fun ReposScreen(vm: MainVM) {
    var showNew by remember { mutableStateOf(false) }

    Column(Modifier.fillMaxSize()) {
        Row(
            Modifier
                .fillMaxWidth()
                .padding(start = 20.dp, end = 8.dp, top = 12.dp, bottom = 4.dp),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            Column(Modifier.weight(1f)) {
                Text("Mes dépôts", fontSize = 22.sp, fontWeight = FontWeight.Bold, color = Accent)
                if (vm.login.isNotBlank()) Text("@${vm.login}", color = Muted, fontSize = 12.sp)
            }
            TextButton(onClick = { showNew = true }) { Text("＋ Nouveau") }
            TextButton(onClick = { vm.openSettings() }) { Text("⚙") }
        }
        HorizontalDivider(color = Outline)

        vm.message?.let {
            Text(it, color = Red, modifier = Modifier.padding(16.dp))
            TextButton(onClick = { vm.loadRepos() }, modifier = Modifier.padding(horizontal = 8.dp)) { Text("Réessayer") }
        }

        Box(Modifier.weight(1f).fillMaxWidth()) {
            if (vm.loadingRepos && vm.repos.isEmpty()) {
                CircularProgressIndicator(Modifier.align(Alignment.Center))
            } else {
                LazyColumn(
                    Modifier.fillMaxSize(),
                    contentPadding = PaddingValues(12.dp),
                    verticalArrangement = Arrangement.spacedBy(8.dp),
                ) {
                    items(vm.repos, key = { it.fullName }) { r ->
                        Column(
                            Modifier
                                .fillMaxWidth()
                                .clip(RoundedCornerShape(14.dp))
                                .background(Panel)
                                .clickable { vm.openRepo(r) }
                                .padding(14.dp)
                        ) {
                            Text(
                                (if (r.isPrivate) "🔒 " else "") + r.fullName,
                                fontWeight = FontWeight.SemiBold,
                            )
                            if (r.description.isNotBlank()) {
                                Text(r.description, color = Muted, fontSize = 13.sp, maxLines = 2)
                            }
                        }
                    }
                }
            }
        }
    }

    if (showNew) {
        var name by remember { mutableStateOf("") }
        var priv by remember { mutableStateOf(false) }
        AlertDialog(
            onDismissRequest = { showNew = false },
            title = { Text("Nouveau dépôt") },
            text = {
                Column {
                    OutlinedTextField(
                        value = name,
                        onValueChange = { name = it },
                        label = { Text("Nom (ex : mon-jeu)") },
                        singleLine = true,
                    )
                    Row(Modifier.padding(top = 12.dp), verticalAlignment = Alignment.CenterVertically) {
                        Switch(checked = priv, onCheckedChange = { priv = it })
                        Spacer(Modifier.width(8.dp))
                        Text("Privé")
                    }
                }
            },
            confirmButton = {
                TextButton(
                    onClick = { vm.createRepo(name.trim(), priv); showNew = false },
                    enabled = name.isNotBlank(),
                ) { Text("Créer") }
            },
            dismissButton = { TextButton(onClick = { showNew = false }) { Text("Annuler") } },
        )
    }
}

// ---------------------------------------------------------------- Chat

@Composable
fun ChatScreen(vm: MainVM) {
    BackHandler { vm.back() }
    var input by remember { mutableStateOf("") }
    val listState = rememberLazyListState()

    LaunchedEffect(vm.lines.size) {
        if (vm.lines.isNotEmpty()) listState.animateScrollToItem(vm.lines.lastIndex)
    }

    Column(Modifier.fillMaxSize()) {
        Row(Modifier.fillMaxWidth().padding(end = 12.dp), verticalAlignment = Alignment.CenterVertically) {
            TextButton(onClick = { vm.back() }) { Text("‹", fontSize = 22.sp) }
            Column(Modifier.weight(1f)) {
                Text(vm.current?.fullName ?: "", fontWeight = FontWeight.SemiBold, maxLines = 1)
                Text(vm.store.model, color = Muted, fontSize = 11.sp)
            }
        }
        HorizontalDivider(color = Outline)

        Box(Modifier.weight(1f).fillMaxWidth()) {
            if (vm.lines.size <= 1) {
                Text(
                    "Dis-moi ce que tu veux construire ou modifier dans ce dépôt.",
                    color = Muted,
                    modifier = Modifier.align(Alignment.Center).padding(32.dp),
                )
            }
            LazyColumn(
                state = listState,
                modifier = Modifier.fillMaxSize(),
                contentPadding = PaddingValues(12.dp),
                verticalArrangement = Arrangement.spacedBy(8.dp),
            ) {
                items(vm.lines, key = { it.id }) { LineView(it) }
            }
        }

        if (vm.running) LinearProgressIndicator(Modifier.fillMaxWidth())

        Row(
            Modifier.fillMaxWidth().padding(8.dp),
            verticalAlignment = Alignment.Bottom,
        ) {
            OutlinedTextField(
                value = input,
                onValueChange = { input = it },
                modifier = Modifier.weight(1f),
                placeholder = { Text("Ex : fais un jeu de snake en HTML") },
                maxLines = 5,
            )
            Spacer(Modifier.width(8.dp))
            if (vm.running) {
                Button(
                    onClick = { vm.stop() },
                    colors = ButtonDefaults.buttonColors(containerColor = Red),
                ) { Text("■ Stop") }
            } else {
                Button(
                    onClick = { vm.send(input); input = "" },
                    enabled = input.isNotBlank(),
                ) { Text("Envoyer") }
            }
        }
    }
}

@Composable
fun LineView(l: Line) {
    when (l.kind) {
        Kind.USER -> Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.End) {
            Box(
                Modifier
                    .widthIn(max = 300.dp)
                    .clip(RoundedCornerShape(16.dp))
                    .background(UserBubble)
                    .padding(12.dp)
            ) { SelectionContainer { Text(l.text) } }
        }

        Kind.ASSISTANT -> Box(
            Modifier
                .widthIn(max = 340.dp)
                .clip(RoundedCornerShape(16.dp))
                .background(Panel)
                .padding(12.dp)
        ) { SelectionContainer { Text(l.text) } }

        Kind.TOOL -> Text(
            "› ${l.text}",
            color = Green,
            fontFamily = FontFamily.Monospace,
            fontSize = 12.sp,
        )

        Kind.INFO -> Text(l.text, color = Muted, fontSize = 12.sp)
        Kind.ERROR -> Text(l.text, color = Red, fontSize = 13.sp)
    }
}
