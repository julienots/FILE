package com.superessence.bodymotion.ui.screens

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.ExperimentalFoundationApi
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.combinedClickable
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.outlined.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.navigation.NavHostController
import com.superessence.bodymotion.BodyMotionApp
import com.superessence.bodymotion.anim.AnimationClip
import com.superessence.bodymotion.anim.CLIP_CATEGORIES
import com.superessence.bodymotion.data.ClipInfo
import com.superessence.bodymotion.data.LoadedCharacter
import com.superessence.bodymotion.retarget.PoseEvaluator
import com.superessence.bodymotion.ui.Routes
import com.superessence.bodymotion.ui.components.*
import com.superessence.bodymotion.ui.go
import com.superessence.bodymotion.ui.theme.C
import com.superessence.bodymotion.ui.theme.T
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext

@OptIn(ExperimentalFoundationApi::class)
@Composable
fun LibraryScreen(nav: NavHostController) {
    val store = BodyMotionApp.store
    val clips by store.clips.collectAsState()
    val chars by store.characters.collectAsState()
    var category by remember { mutableStateOf<String?>(null) }
    var selection by remember { mutableStateOf(setOf<String>()) }
    var menuFor by remember { mutableStateOf<ClipInfo?>(null) }
    var renameFor by remember { mutableStateOf<ClipInfo?>(null) }
    var categoryFor by remember { mutableStateOf<ClipInfo?>(null) }
    var deleteFor by remember { mutableStateOf<ClipInfo?>(null) }
    val shown = clips.filter { category == null || it.category == category }.sortedByDescending { it.createdAt }

    Column(Modifier.fillMaxSize().background(C.Bg)) {
        TopBar(if (selection.isEmpty()) "Mes animations" else "${selection.size} sélectionnée(s)", onBack = null, actions = {
            if (selection.isNotEmpty()) {
                TextButton({ nav.go(Routes.export(selection.toList())) }) { Text("Exporter", color = C.Cyan) }
                IconButton({ selection = emptySet() }) { Icon(Icons.Outlined.Close, "Annuler la sélection", tint = C.Text2) }
            }
        })
        Row(Modifier.horizontalScroll(rememberScrollState()).padding(horizontal = 12.dp, vertical = 8.dp), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            FilterChip(category == null, { category = null }, label = { Text("Toutes (${clips.size})") }, shape = Shape4)
            CLIP_CATEGORIES.filter { c -> clips.any { it.category == c } }.forEach { c ->
                FilterChip(category == c, { category = c }, label = { Text(c) }, shape = Shape4)
            }
        }
        if (shown.isEmpty()) EmptyState("Aucune animation", "Lancez une capture : chaque enregistrement est rangé ici.")
        LazyColumn(Modifier.weight(1f), contentPadding = PaddingValues(12.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            items(shown, key = { it.id }) { c ->
                val selected = c.id in selection
                Row(Modifier.fillMaxWidth().background(if (selected) C.Cyan.copy(alpha = 0.08f) else C.Panel, Shape6)
                    .border(1.dp, if (selected) C.Cyan else C.Border, Shape6)
                    .combinedClickable(
                        onClick = { if (selection.isNotEmpty()) selection = if (selected) selection - c.id else selection + c.id else menuFor = c },
                        onLongClick = { selection = selection + c.id })
                    .padding(8.dp), verticalAlignment = Alignment.CenterVertically) {
                    ClipThumbnail(c, Modifier.size(64.dp))
                    Spacer(Modifier.width(12.dp))
                    Column(Modifier.weight(1f)) {
                        Text(c.name, style = T.bodyM, maxLines = 1, overflow = TextOverflow.Ellipsis)
                        Text("${"%.2f".format(c.duration)} s · ${c.fps.toInt()} FPS${if (c.loop) " · boucle" else ""}", style = T.label)
                        Text(chars.firstOrNull { it.id == c.characterId }?.name ?: "—", style = T.small, maxLines = 1)
                    }
                    Chip(c.category.uppercase())
                }
            }
        }
        BottomTabs(nav, Routes.LIBRARY, onCapture = {
            val usable = chars.filter { it.report.compatible }
            if (usable.isNotEmpty()) nav.go(Routes.capture(usable.first().id)) else nav.go(Routes.CHARACTERS)
        })
    }

    menuFor?.let { c ->
        AlertDialog(onDismissRequest = { menuFor = null }, containerColor = C.Panel, title = { Text(c.name, style = T.h2) }, confirmButton = {},
            dismissButton = { TextButton({ menuFor = null }) { Text("Fermer") } },
            text = {
                Column(Modifier.verticalScroll(rememberScrollState())) {
                    ClipThumbnail(c, Modifier.fillMaxWidth().height(180.dp))
                    Spacer(Modifier.height(8.dp))
                    Group {
                        ListRow("Ouvrir dans l'éditeur", Icons.Outlined.Timeline, onClick = { menuFor = null; nav.go(Routes.editor(c.id)) })
                        GroupDivider(); ListRow("Exporter", Icons.Outlined.IosShare, onClick = { menuFor = null; nav.go(Routes.export(listOf(c.id))) })
                        GroupDivider(); ListRow("Renommer", Icons.Outlined.Edit, onClick = { menuFor = null; renameFor = c })
                        GroupDivider(); ListRow("Catégorie", Icons.Outlined.Label, trailing = { Text(c.category, style = T.small) }, onClick = { menuFor = null; categoryFor = c })
                        GroupDivider(); ListRow("Dupliquer", Icons.Outlined.ContentCopy, onClick = { menuFor = null; store.duplicateClip(c.id) })
                        GroupDivider(); ListRow("Supprimer", Icons.Outlined.Delete, iconTint = C.Danger, onClick = { menuFor = null; deleteFor = c })
                    }
                }
            })
    }
    renameFor?.let { c -> TextInputDialog("Renommer", c.name, { renameFor = null }) { n -> store.loadClip(c.id)?.let { it.name = n; store.saveClip(it) }; renameFor = null } }
    categoryFor?.let { c ->
        AlertDialog(onDismissRequest = { categoryFor = null }, containerColor = C.Panel, title = { Text("Catégorie", style = T.h2) }, confirmButton = {},
            text = {
                Column(Modifier.verticalScroll(rememberScrollState())) {
                    CLIP_CATEGORIES.forEach { cat -> ListRow(cat, accent = cat == c.category, onClick = { store.loadClip(c.id)?.let { it.category = cat; store.saveClip(it) }; categoryFor = null }) }
                }
            })
    }
    deleteFor?.let { c -> ConfirmDialog("Supprimer « ${c.name} » ?", "Cette action est définitive.", onDismiss = { deleteFor = null }) { store.deleteClip(c.id); deleteFor = null } }
}

/** Miniature animée : silhouette du squelette du personnage jouant le clip en boucle (vue de face). */
@Composable
fun ClipThumbnail(info: ClipInfo, modifier: Modifier) {
    val store = BodyMotionApp.store
    val data by produceState<Pair<AnimationClip, LoadedCharacter>?>(null, info.id, info.frames) {
        value = withContext(Dispatchers.Default) {
            val clip = store.loadClip(info.id) ?: return@withContext null
            val ch = store.load(clip.characterId) ?: return@withContext null
            clip to ch
        }
    }
    var time by remember { mutableFloatStateOf(0f) }
    LaunchedEffect(data) {
        val start = withFrameNanos { it }
        while (data != null) withFrameNanos { time = ((it - start) / 1e9).toFloat() }
    }
    Box(modifier.background(C.Bg, Shape4), contentAlignment = Alignment.Center) {
        val d = data ?: return@Box
        val (clip, ch) = d
        val pairs = remember(ch) { bonePairs(ch) }
        val h = remember(ch) { ch.skeleton.restHeight() }
        val t = if (clip.duration > 0) time % clip.duration else 0.0
        val pose = clip.poseAtTime(t.toDouble(), ch.skeleton)
        val g = PoseEvaluator.globals(ch.skeleton, pose)
        val hips0 = ch.skeleton.restPos(clip.hipsNode)
        Canvas(Modifier.fillMaxSize().padding(4.dp)) {
            val s = size.height / (h * 1.15f).toFloat()
            fun p(i: Int): Offset { val w = g[i].translation(); return Offset((size.width / 2 + (w.x - hips0.x) * s).toFloat(), (size.height * 0.96f - (w.y - (hips0.y - h * 0.55)) * s).toFloat()) }
            drawLine(C.Border2, Offset(0f, size.height * 0.96f), Offset(size.width, size.height * 0.96f), 1f)
            for ((a, b) in pairs) drawLine(C.Cyan, p(a), p(b), (size.height / 40f).coerceAtLeast(2f), StrokeCap.Round)
            ch.boneMap[com.superessence.bodymotion.rig.HumanBone.Head]?.let { drawCircle(C.Cyan, size.height / 22f, p(it)) }
        }
    }
}
