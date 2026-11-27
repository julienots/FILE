package com.superessence.bodymotion.ui.screens

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.outlined.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.drawBehind
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.unit.dp
import androidx.navigation.NavHostController
import com.superessence.bodymotion.BodyMotionApp
import com.superessence.bodymotion.data.CharacterInfo
import com.superessence.bodymotion.ui.Routes
import com.superessence.bodymotion.ui.components.*
import com.superessence.bodymotion.ui.go
import com.superessence.bodymotion.ui.tab
import com.superessence.bodymotion.ui.theme.C
import com.superessence.bodymotion.ui.theme.T
import java.text.DateFormat
import java.util.Date

@Composable
fun HomeScreen(nav: NavHostController) {
    val store = BodyMotionApp.store
    val chars by store.characters.collectAsState()
    val clips by store.clips.collectAsState()
    val projects by store.projects.collectAsState()
    var pickCharacterFor by remember { mutableStateOf<((CharacterInfo) -> Unit)?>(null) }
    var newProject by remember { mutableStateOf(false) }

    fun startCapture(video: Boolean) {
        val usable = chars.filter { it.report.compatible }
        when {
            usable.size == 1 -> nav.go(Routes.capture(usable[0].id, video))
            usable.isEmpty() -> nav.go(Routes.CHARACTERS)
            else -> pickCharacterFor = { nav.go(Routes.capture(it.id, video)) }
        }
    }

    Column(Modifier.fillMaxSize().background(C.Bg)) {
        Row(Modifier.fillMaxWidth().statusBarsPadding().height(56.dp).padding(start = 16.dp, end = 4.dp), verticalAlignment = Alignment.CenterVertically) {
            BmLogo(22.dp, gradient = false, trails = false)
            Spacer(Modifier.width(10.dp))
            Text("BodyMotion Studio", style = T.h2, modifier = Modifier.weight(1f))
            IconButton(onClick = { nav.go(Routes.SETTINGS) }) { Icon(Icons.Outlined.Tune, "Paramètres", tint = C.Muted) }
        }
        HorizontalDivider(color = C.Border)
        Column(Modifier.weight(1f).verticalScroll(rememberScrollState()).padding(16.dp), verticalArrangement = Arrangement.spacedBy(22.dp)) {
            Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                SectionLabel("Démarrer")
                Group {
                    ListRow("Nouvelle capture", Icons.Outlined.PhotoCamera, "Caméra en direct", accent = true, onClick = { startCapture(false) })
                    GroupDivider()
                    ListRow("Capture depuis une vidéo", Icons.Outlined.VideoFile, "Analyse d'une vidéo de votre galerie", onClick = { startCapture(true) })
                    GroupDivider()
                    ListRow("Nouveau projet", Icons.Outlined.CreateNewFolder, onClick = { newProject = true })
                    GroupDivider()
                    ListRow("Importer un personnage", Icons.Outlined.ViewInAr, trailing = { Text("FBX GLB VRM", style = T.label) }, onClick = { nav.go(Routes.CHARACTERS + "?import=true")})
                }
            }
            Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                SectionLabel("Bibliothèque")
                Group {
                    ListRow("Mes animations", Icons.Outlined.ViewColumn, trailing = { Text("${clips.size}", style = T.mono.copy(color = C.Muted)) }, onClick = { nav.tab(Routes.LIBRARY) })
                    GroupDivider()
                    ListRow("Mes personnages", Icons.Outlined.Accessibility, trailing = { Text("${chars.size}", style = T.mono.copy(color = C.Muted)) }, onClick = { nav.go(Routes.CHARACTERS) })
                }
            }
            Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    SectionLabel("Projets récents", Modifier.weight(1f))
                    TextButton(onClick = { nav.tab(Routes.PROJECTS) }) { Text("Tout afficher", style = T.small.copy(color = C.Text2)) }
                }
                if (projects.isEmpty()) Text("Aucun projet pour l'instant. Un projet regroupe un personnage et ses animations.", style = T.small)
                projects.take(4).forEach { p ->
                    val ch = chars.firstOrNull { it.id == p.characterId }
                    Row(Modifier.fillMaxWidth().clickable { nav.go(Routes.project(p.id)) }.padding(vertical = 10.dp), verticalAlignment = Alignment.CenterVertically) {
                        Box(Modifier.size(44.dp).background(C.Panel2, Shape4), contentAlignment = Alignment.Center) { BmLogo(26.dp, gradient = false, trails = false) }
                        Spacer(Modifier.width(12.dp))
                        Column(Modifier.weight(1f)) {
                            Text(p.name, style = T.bodyM)
                            Text("${ch?.format ?: "—"} · ${p.clipIds.size} clips · ${DateFormat.getDateInstance(DateFormat.SHORT).format(Date(p.updatedAt))}", style = T.label)
                        }
                        Text(p.engine, style = T.small)
                    }
                    HorizontalDivider(color = C.Border)
                }
            }
        }
        BottomTabs(nav, Routes.HOME, onCapture = { startCapture(false) })
    }

    pickCharacterFor?.let { cb ->
        CharacterPicker(chars.filter { it.report.compatible }, onDismiss = { pickCharacterFor = null }) { pickCharacterFor = null; cb(it) }
    }
    if (newProject) NewProjectDialog(chars, onDismiss = { newProject = false }) { p -> newProject = false; store.saveProject(p); nav.go(Routes.project(p.id)) }
}

@Composable
fun BottomTabs(nav: NavHostController, current: String, onCapture: () -> Unit) {
    val items = listOf(
        Triple(Routes.HOME, "Accueil", Icons.Outlined.GridView),
        Triple("capture", "Capture", Icons.Outlined.PhotoCamera),
        Triple(Routes.LIBRARY, "Bibliothèque", Icons.Outlined.ViewColumn),
        Triple(Routes.PROJECTS, "Projets", Icons.Outlined.Folder),
    )
    Column(Modifier.fillMaxWidth().background(C.Bar)) {
        HorizontalDivider(color = C.Border)
        Row(Modifier.fillMaxWidth().navigationBarsPadding().height(64.dp)) {
            items.forEach { (route, label, icon) ->
                val sel = route == current
                Column(
                    Modifier.weight(1f).fillMaxHeight()
                        .drawBehind { if (sel) drawRect(C.Cyan, size = Size(size.width, 2.dp.toPx())) }
                        .clickable(role = Role.Tab) { if (route == "capture") onCapture() else if (!sel) nav.tab(route) },
                    horizontalAlignment = Alignment.CenterHorizontally, verticalArrangement = Arrangement.Center,
                ) {
                    Icon(icon, null, tint = if (sel) C.Text else C.Muted, modifier = Modifier.size(20.dp))
                    Spacer(Modifier.height(4.dp))
                    Text(label, style = T.small.copy(color = if (sel) C.Text else C.Muted))
                }
            }
        }
    }
}

@Composable
fun CharacterPicker(chars: List<CharacterInfo>, onDismiss: () -> Unit, onPick: (CharacterInfo) -> Unit) {
    AlertDialog(onDismissRequest = onDismiss, confirmButton = {}, dismissButton = { TextButton(onDismiss) { Text("Annuler") } },
        containerColor = C.Panel, title = { Text("Personnage à animer", style = T.h2) },
        text = {
            Column(Modifier.verticalScroll(rememberScrollState())) {
                chars.forEach { c -> ListRow(c.name, Icons.Outlined.Accessibility, "${c.format} · ${c.report.jointCount} os", onClick = { onPick(c) }) }
            }
        })
}

@Composable
fun NewProjectDialog(chars: List<CharacterInfo>, onDismiss: () -> Unit, onCreate: (com.superessence.bodymotion.data.ProjectInfo) -> Unit) {
    var name by remember { mutableStateOf("Nouveau projet") }
    var engine by remember { mutableStateOf("Unity") }
    var charId by remember { mutableStateOf(chars.firstOrNull { it.report.compatible }?.id) }
    AlertDialog(onDismissRequest = onDismiss, containerColor = C.Panel, title = { Text("Nouveau projet", style = T.h2) },
        text = {
            Column(verticalArrangement = Arrangement.spacedBy(12.dp)) {
                OutlinedTextField(name, { name = it }, label = { Text("Nom") }, singleLine = true, modifier = Modifier.fillMaxWidth())
                SectionLabel("Moteur cible")
                Segmented(listOf("Unity", "Unreal", "Godot"), engine, { it }) { engine = it }
                SectionLabel("Personnage")
                Column(Modifier.heightIn(max = 220.dp).verticalScroll(rememberScrollState())) {
                    chars.filter { it.report.compatible }.forEach { c ->
                        Row(Modifier.fillMaxWidth().clickable { charId = c.id }.padding(vertical = 6.dp), verticalAlignment = Alignment.CenterVertically) {
                            RadioButton(charId == c.id, { charId = c.id }); Text(c.name, style = T.body)
                        }
                    }
                }
            }
        },
        confirmButton = { TextButton(enabled = name.isNotBlank(), onClick = {
            onCreate(com.superessence.bodymotion.data.ProjectInfo(java.util.UUID.randomUUID().toString(), name.trim(), charId, emptyList(), System.currentTimeMillis(), engine))
        }) { Text("Créer") } },
        dismissButton = { TextButton(onDismiss) { Text("Annuler") } })
}
