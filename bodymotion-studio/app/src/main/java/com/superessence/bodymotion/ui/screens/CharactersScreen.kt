package com.superessence.bodymotion.ui.screens

import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
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
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.navigation.NavHostController
import com.superessence.bodymotion.BodyMotionApp
import com.superessence.bodymotion.data.LoadedCharacter
import com.superessence.bodymotion.data.ModelImporter
import com.superessence.bodymotion.rig.HumanBone
import com.superessence.bodymotion.ui.Routes
import com.superessence.bodymotion.ui.components.*
import com.superessence.bodymotion.ui.go
import com.superessence.bodymotion.ui.theme.C
import com.superessence.bodymotion.ui.theme.T
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

@Composable
fun CharactersScreen(nav: NavHostController, autoImport: Boolean) {
    val store = BodyMotionApp.store
    val ctx = LocalContext.current
    val chars by store.characters.collectAsState()
    val scope = rememberCoroutineScope()
    var busy by remember { mutableStateOf(false) }
    var error by remember { mutableStateOf<String?>(null) }
    val picker = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
        if (uri == null) return@rememberLauncherForActivityResult
        busy = true
        scope.launch {
            val res = withContext(Dispatchers.Default) {
                runCatching {
                    val r = ModelImporter.import(ctx, uri)
                    store.addCharacter(r.fileName.substringBeforeLast('.'), r.fileName, r.format, r.doc)
                }
            }
            busy = false
            res.onSuccess { nav.go(Routes.character(it.id)) }.onFailure { error = it.message ?: "Import impossible." }
        }
    }
    LaunchedEffect(autoImport) { if (autoImport) picker.launch(arrayOf("*/*")) }

    Column(Modifier.fillMaxSize().background(C.Bg)) {
        TopBar("Mes personnages", onBack = { nav.popBackStack() }, actions = {
            TextButton(onClick = { picker.launch(arrayOf("*/*")) }) { Icon(Icons.Outlined.FileOpen, null, tint = C.Cyan); Spacer(Modifier.width(6.dp)); Text("Importer", color = C.Cyan) }
        })
        if (busy) LinearProgressIndicator(Modifier.fillMaxWidth(), color = C.Cyan, trackColor = C.Panel2)
        LazyColumn(Modifier.weight(1f), contentPadding = PaddingValues(16.dp), verticalArrangement = Arrangement.spacedBy(10.dp)) {
            item {
                Text("Formats : FBX (binaire), GLB, glTF, VRM, ou .zip contenant le modèle et ses textures.", style = T.small)
            }
            items(chars, key = { it.id }) { c ->
                val r = c.report
                Row(Modifier.fillMaxWidth().background(C.Panel, Shape6).border(1.dp, C.Border, Shape6).clickable { nav.go(Routes.character(c.id)) }.padding(14.dp),
                    verticalAlignment = Alignment.CenterVertically) {
                    Box(Modifier.size(44.dp).background(C.Panel2, Shape4), contentAlignment = Alignment.Center) { Icon(Icons.Outlined.Accessibility, null, tint = C.Text2) }
                    Spacer(Modifier.width(12.dp))
                    Column(Modifier.weight(1f)) {
                        Text(c.name, style = T.bodyM, maxLines = 1, overflow = TextOverflow.Ellipsis)
                        Text("${c.format} · ${r.jointCount} os · ${"%.2f".format(r.heightMeters)} m${if (c.isSample) " · exemple" else ""}", style = T.label)
                    }
                    when {
                        !r.hasSkeleton -> Chip("SANS SQUELETTE", C.Danger)
                        !r.compatible -> Chip("À CONFIGURER", C.Warn)
                        else -> Chip("PRÊT", C.Cyan)
                    }
                }
            }
        }
    }
    error?.let { msg ->
        AlertDialog(onDismissRequest = { error = null }, containerColor = C.Panel, title = { Text("Import impossible", style = T.h2) },
            text = { Text(msg, style = T.body) }, confirmButton = { TextButton({ error = null }) { Text("OK") } })
    }
}

@Composable
fun CharacterDetailScreen(nav: NavHostController, id: String) {
    val store = BodyMotionApp.store
    val chars by store.characters.collectAsState()
    val settings by store.settings.collectAsState()
    val info = chars.firstOrNull { it.id == id }
    var loaded by remember(info) { mutableStateOf<LoadedCharacter?>(null) }
    var loadError by remember { mutableStateOf<String?>(null) }
    LaunchedEffect(info) { if (info != null) runCatching { withContext(Dispatchers.Default) { store.load(id) } }.onSuccess { loaded = it }.onFailure { loadError = it.message } }
    var showBones by remember { mutableStateOf(true) }
    var editBone by remember { mutableStateOf<HumanBone?>(null) }
    var rename by remember { mutableStateOf(false) }
    var confirmDelete by remember { mutableStateOf(false) }
    val controller = remember { ViewportController() }
    if (info == null) { LaunchedEffect(Unit) { nav.popBackStack() }; return }
    val r = info.report

    Column(Modifier.fillMaxSize().background(C.Bg)) {
        TopBar(info.name, onBack = { nav.popBackStack() }, subtitle = "${info.format} · ${info.sourceFile}", actions = {
            IconButton({ rename = true }) { Icon(Icons.Outlined.Edit, "Renommer", tint = C.Text2) }
            if (!info.isSample) IconButton({ confirmDelete = true }) { Icon(Icons.Outlined.Delete, "Supprimer", tint = C.Text2) }
        })
        Column(Modifier.weight(1f).verticalScroll(rememberScrollState())) {
            Box(Modifier.fillMaxWidth().height(340.dp)) {
                loaded?.let { ch -> Viewport3D(ch, settings.quality, Modifier.fillMaxSize(), showBones = showBones, controller = controller) { FramePose(null) } }
                    ?: Box(Modifier.fillMaxSize(), contentAlignment = Alignment.Center) { if (loadError != null) Text(loadError!!, style = T.small) else CircularProgressIndicator(color = C.Cyan) }
                Row(Modifier.padding(10.dp), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    FilterChip(showBones, { showBones = !showBones }, label = { Text("Os") }, shape = Shape4)
                    FilterChip(false, { controller.resetCamera() }, label = { Text("Recentrer") }, shape = Shape4)
                }
                Text("1 doigt : orbite · 2 doigts : zoom / déplacement", style = T.small, modifier = Modifier.align(Alignment.BottomCenter).padding(8.dp))
            }
            Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(14.dp)) {
                if (!r.hasSkeleton) InfoBanner("Ce modèle n'a pas de squelette (aucun skin). Il ne peut pas être animé par la capture : ajoutez un rig (par ex. Mixamo ou Blender) puis réimportez-le.", C.Danger)
                else if (!r.compatible) InfoBanner("Os humanoïdes manquants : ${r.missing.joinToString { it.label }}. Associez-les manuellement ci-dessous.")
                Group {
                    Row(Modifier.padding(14.dp), horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                        Stat("${r.jointCount}", "os", Modifier.weight(1f))
                        Stat("${HumanBone.entries.count { it.required } - r.missing.size}/${HumanBone.entries.count { it.required }}", "os requis", Modifier.weight(1f))
                        Stat("%.2f m".format(r.heightMeters), "taille du rig", Modifier.weight(1f))
                    }
                    GroupDivider()
                    ListRow("Doigts", trailing = { Text(if (r.fingers > 0) "${r.fingers} / 30" else "absents", style = T.mono) })
                    GroupDivider()
                    ListRow("Visage (blendshapes)", subtitle = if (r.faceCompatible) "${r.morphCount} cibles détectées" else "Aucun blendshape compatible : la capture faciale est désactivée pour ce modèle",
                        trailing = { Chip(if (r.faceCompatible) "OK" else "ABSENT", if (r.faceCompatible) C.Cyan else C.Warn) })
                    if (r.isVrm) { GroupDivider(); ListRow("VRM", subtitle = "Os humanoïdes lus depuis l'extension VRM") }
                }
                if (r.compatible) PrimaryButton("Capturer avec ce personnage", Modifier.fillMaxWidth(), icon = Icons.Outlined.PhotoCamera) { nav.go(Routes.capture(id)) }
                if (r.hasSkeleton) {
                    SectionLabel("Correspondance des os")
                    Text("Touchez un os pour le réassigner. Les os non humanoïdes (accessoires, torsions) restent animés par leurs parents.", style = T.small)
                    Group {
                        val list = HumanBone.entries.filter { !it.isFinger || info.boneMap[it] != null }
                        list.forEachIndexed { i, b ->
                            val node = info.boneMap[b]
                            ListRow(b.label + if (b.required) " *" else "", subtitle = node?.let { n -> loaded?.skeleton?.nodes?.getOrNull(n)?.name } ?: "non assigné",
                                iconTint = C.Text2, trailing = { if (node == null && b.required) Chip("MANQUANT", C.Warn) else Text(b.name, style = T.label) },
                                onClick = { editBone = b })
                            if (i < list.size - 1) GroupDivider()
                        }
                    }
                }
                Spacer(Modifier.navigationBarsPadding())
            }
        }
    }

    editBone?.let { b ->
        val sk = loaded?.skeleton
        AlertDialog(onDismissRequest = { editBone = null }, containerColor = C.Panel, title = { Text("${b.label} (${b.name})", style = T.h2) },
            text = {
                Column(Modifier.heightIn(max = 420.dp).verticalScroll(rememberScrollState())) {
                    ListRow("Aucun", onClick = { store.updateBoneMap(id, info.boneMap - b); editBone = null })
                    sk?.nodes?.filter { it.isJoint }?.forEach { n ->
                        val selected = info.boneMap[b] == n.index
                        ListRow(n.name, accent = selected, onClick = {
                            val m = info.boneMap.filterValues { it != n.index }.toMutableMap(); m[b] = n.index
                            store.updateBoneMap(id, m); editBone = null
                        })
                    }
                }
            }, confirmButton = { TextButton({ editBone = null }) { Text("Fermer") } })
    }
    if (rename) TextInputDialog("Renommer", info.name, onDismiss = { rename = false }) { store.renameCharacter(id, it); rename = false }
    if (confirmDelete) ConfirmDialog("Supprimer « ${info.name} » ?", "Les animations de ce personnage seront aussi supprimées.", onDismiss = { confirmDelete = false }) {
        confirmDelete = false; store.deleteCharacter(id); nav.popBackStack()
    }
}

@Composable
fun Stat(value: String, label: String, modifier: Modifier = Modifier) {
    Column(modifier.background(C.Bg, Shape4).padding(10.dp)) { Text(value, style = T.mono.copy(fontSize = androidx.compose.ui.unit.TextUnit(16f, androidx.compose.ui.unit.TextUnitType.Sp), color = C.Text)); Text(label, style = T.small) }
}

@Composable
fun TextInputDialog(title: String, initial: String, onDismiss: () -> Unit, onOk: (String) -> Unit) {
    var v by remember { mutableStateOf(initial) }
    AlertDialog(onDismissRequest = onDismiss, containerColor = C.Panel, title = { Text(title, style = T.h2) },
        text = { OutlinedTextField(v, { v = it }, singleLine = true, modifier = Modifier.fillMaxWidth()) },
        confirmButton = { TextButton(enabled = v.isNotBlank(), onClick = { onOk(v.trim()) }) { Text("OK") } }, dismissButton = { TextButton(onDismiss) { Text("Annuler") } })
}

@Composable
fun ConfirmDialog(title: String, text: String, confirm: String = "Supprimer", onDismiss: () -> Unit, onOk: () -> Unit) {
    AlertDialog(onDismissRequest = onDismiss, containerColor = C.Panel, title = { Text(title, style = T.h2) }, text = { Text(text, style = T.body) },
        confirmButton = { TextButton(onOk) { Text(confirm, color = C.Danger) } }, dismissButton = { TextButton(onDismiss) { Text("Annuler") } })
}
