package com.superessence.bodymotion.ui.screens

import android.content.Intent
import android.net.Uri
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.outlined.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import androidx.navigation.NavHostController
import com.superessence.bodymotion.BodyMotionApp
import com.superessence.bodymotion.BuildConfig
import com.superessence.bodymotion.ui.Routes
import com.superessence.bodymotion.ui.components.*
import com.superessence.bodymotion.ui.go
import com.superessence.bodymotion.ui.theme.C
import com.superessence.bodymotion.ui.theme.T
import java.text.DateFormat
import java.util.Date

@Composable
fun ProjectsScreen(nav: NavHostController) {
    val store = BodyMotionApp.store
    val projects by store.projects.collectAsState()
    val chars by store.characters.collectAsState()
    var create by remember { mutableStateOf(false) }
    Column(Modifier.fillMaxSize().background(C.Bg)) {
        TopBar("Projets", onBack = null, actions = { TextButton({ create = true }) { Icon(Icons.Outlined.Add, null, tint = C.Cyan); Text("Nouveau", color = C.Cyan) } })
        if (projects.isEmpty()) EmptyState("Aucun projet", "Un projet regroupe un personnage, ses animations et le moteur cible.") { PrimaryButton("Créer un projet") { create = true } }
        LazyColumn(Modifier.weight(1f), contentPadding = PaddingValues(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            items(projects, key = { it.id }) { p ->
                Group {
                    ListRow(p.name, Icons.Outlined.Folder, "${chars.firstOrNull { it.id == p.characterId }?.name ?: "Sans personnage"} · ${p.clipIds.size} animations · ${p.engine} · ${DateFormat.getDateInstance(DateFormat.SHORT).format(Date(p.updatedAt))}",
                        onClick = { nav.go(Routes.project(p.id)) })
                }
            }
        }
        BottomTabs(nav, Routes.PROJECTS, onCapture = { chars.firstOrNull { it.report.compatible }?.let { nav.go(Routes.capture(it.id)) } ?: nav.go(Routes.CHARACTERS) })
    }
    if (create) NewProjectDialog(chars, { create = false }) { p -> create = false; store.saveProject(p); nav.go(Routes.project(p.id)) }
}

@Composable
fun ProjectDetailScreen(nav: NavHostController, id: String) {
    val store = BodyMotionApp.store
    val projects by store.projects.collectAsState()
    val clips by store.clips.collectAsState()
    val chars by store.characters.collectAsState()
    val p = projects.firstOrNull { it.id == id } ?: run { LaunchedEffect(Unit) { nav.popBackStack() }; return }
    val ch = chars.firstOrNull { it.id == p.characterId }
    var add by remember { mutableStateOf(false) }
    var rename by remember { mutableStateOf(false) }
    var delete by remember { mutableStateOf(false) }
    val projectClips = p.clipIds.mapNotNull { cid -> clips.firstOrNull { it.id == cid } }
    Column(Modifier.fillMaxSize().background(C.Bg)) {
        TopBar(p.name, { nav.popBackStack() }, subtitle = "Moteur : ${p.engine}", actions = {
            IconButton({ rename = true }) { Icon(Icons.Outlined.Edit, "Renommer", tint = C.Text2) }
            IconButton({ delete = true }) { Icon(Icons.Outlined.Delete, "Supprimer", tint = C.Text2) }
        })
        Column(Modifier.weight(1f).verticalScroll(rememberScrollState()).padding(16.dp), verticalArrangement = Arrangement.spacedBy(14.dp)) {
            SectionLabel("Personnage")
            Group { ListRow(ch?.name ?: "Aucun", Icons.Outlined.Accessibility, ch?.let { "${it.format} · ${it.report.jointCount} os" }, onClick = ch?.let { c -> { nav.go(Routes.character(c.id)) } }) }
            if (ch != null && ch.report.compatible) PrimaryButton("Nouvelle capture pour ce projet", Modifier.fillMaxWidth(), icon = Icons.Outlined.PhotoCamera) { nav.go(Routes.capture(ch.id)) }
            SectionLabel("Animations du projet")
            if (projectClips.isEmpty()) Text("Ajoutez des animations de ce personnage depuis la bibliothèque.", style = T.small)
            Group {
                projectClips.forEachIndexed { i, c ->
                    ListRow(c.name, subtitle = "%.2f s · %s".format(c.duration, c.category), trailing = {
                        IconButton({ store.saveProject(p.copy(clipIds = p.clipIds - c.id)) }) { Icon(Icons.Outlined.RemoveCircleOutline, "Retirer", tint = C.Muted) }
                    }, onClick = { nav.go(Routes.editor(c.id)) })
                    if (i < projectClips.size - 1) GroupDivider()
                }
                if (projectClips.isNotEmpty()) GroupDivider()
                ListRow("Ajouter des animations", Icons.Outlined.Add, onClick = { add = true })
            }
            if (projectClips.isNotEmpty()) PrimaryButton("Exporter le projet (${projectClips.size})", Modifier.fillMaxWidth(), icon = Icons.Outlined.IosShare) { nav.go(Routes.export(projectClips.map { it.id })) }
        }
    }
    if (add) {
        val candidates = clips.filter { it.characterId == p.characterId && it.id !in p.clipIds }
        AlertDialog(onDismissRequest = { add = false }, containerColor = C.Panel, title = { Text("Ajouter", style = T.h2) }, confirmButton = { TextButton({ add = false }) { Text("Fermer") } },
            text = {
                Column(Modifier.verticalScroll(rememberScrollState())) {
                    if (candidates.isEmpty()) Text("Aucune autre animation pour ce personnage.", style = T.small)
                    candidates.forEach { c -> ListRow(c.name, subtitle = c.category, onClick = { store.saveProject(p.copy(clipIds = p.clipIds + c.id)) }) }
                }
            })
    }
    if (rename) TextInputDialog("Renommer", p.name, { rename = false }) { store.saveProject(p.copy(name = it)); rename = false }
    if (delete) ConfirmDialog("Supprimer le projet ?", "Les animations restent dans la bibliothèque.", onDismiss = { delete = false }) { store.deleteProject(p.id); delete = false; nav.popBackStack() }
}

@Composable
fun SettingsScreen(nav: NavHostController) {
    val store = BodyMotionApp.store
    val s by store.settings.collectAsState()
    val ctx = LocalContext.current
    Column(Modifier.fillMaxSize().background(C.Bg)) {
        TopBar("Paramètres", { nav.popBackStack() })
        Column(Modifier.weight(1f).verticalScroll(rememberScrollState()).padding(16.dp), verticalArrangement = Arrangement.spacedBy(14.dp)) {
            SectionLabel("Qualité graphique du viewport")
            Segmented(listOf(0, 1, 2), s.quality, { listOf("Basse", "Moyenne", "Haute")[it] }) { v -> store.updateSettings { it.copy(quality = v) } }
            Text("Haute : ombres 2048, MSAA 4×, occlusion ambiante. Basse : résolution dynamique, sans ombres (appareils modestes). La capture utilise toujours la qualité basse pour l'aperçu afin de préserver le suivi.", style = T.small)
            SectionLabel("Capture")
            Segmented(listOf(0, 1, 2), s.smoothing, { listOf("Lissage faible", "Moyen", "Fort")[it] }) { v -> store.updateSettings { it.copy(smoothing = v) } }
            Segmented(listOf(24, 30, 60), s.captureFps, { "$it FPS" }) { v -> store.updateSettings { it.copy(captureFps = v) } }
            Group { ToggleRow("Garder l'orientation du modèle", s.keepModelFacing, "Le personnage conserve son orientation d'origine (recommandé pour les moteurs de jeu)") { v -> store.updateSettings { it.copy(keepModelFacing = v) } } }
            SectionLabel("Limites de la capture (en toute transparence)")
            Group {
                ListRow("Profondeur estimée", subtitle = "Une seule caméra ne mesure pas la profondeur : elle est estimée par le modèle de détection. Les mouvements vers la caméra et les membres cachés sont moins précis.")
                GroupDivider(); ListRow("Pertes de suivi", subtitle = "Les images sans corps détecté sont reconstruites par interpolation (≤ 1,5 s) ou figées, et signalées en orange dans la timeline.")
                GroupDivider(); ListRow("Multi-caméra", subtitle = "Non disponible dans cette version : la capture utilise une caméra (ou une vidéo) à la fois.")
                GroupDivider(); ListRow("Traitement local", subtitle = "Toute l'analyse se fait sur l'appareil : aucune image n'est envoyée, aucun abonnement.")
            }
            SectionLabel("À propos")
            Group {
                ListRow("BodyMotion Studio ${BuildConfig.VERSION_NAME}", subtitle = "Édité par SUPER ESSENCE STUDIO — Create. Capture. Animate.")
                GroupDivider(); ListRow("Revoir l'introduction", Icons.Outlined.Replay, onClick = { store.updateSettings { it.copy(introSeen = false) }; nav.navigate("intro") { popUpTo(0) } })
                GroupDivider(); ListRow("Licences", subtitle = "MediaPipe (Apache 2.0, Google) · Filament (Apache 2.0, Google) · Polices Inter, Space Grotesk, JetBrains Mono, Fredoka (SIL OFL 1.1)")
            }
        }
    }
    @Suppress("UNUSED_VARIABLE") val unused = ctx to Uri.EMPTY to Intent()
}
