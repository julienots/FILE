package com.superessence.bodymotion.ui.screens

import android.content.Intent
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.outlined.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import androidx.core.content.FileProvider
import androidx.navigation.NavHostController
import com.superessence.bodymotion.BodyMotionApp
import com.superessence.bodymotion.export.*
import com.superessence.bodymotion.ui.components.*
import com.superessence.bodymotion.ui.theme.C
import com.superessence.bodymotion.ui.theme.T
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.io.File
import java.util.zip.ZipEntry
import java.util.zip.ZipOutputStream

@Composable
fun ExportScreen(nav: NavHostController, initialIds: List<String>) {
    val store = BodyMotionApp.store
    val ctx = LocalContext.current
    val allClips by store.clips.collectAsState()
    val first = allClips.firstOrNull { it.id in initialIds }
    val sameChar = allClips.filter { it.characterId == first?.characterId }
    var selected by remember { mutableStateOf(initialIds.toSet()) }
    var profile by remember { mutableStateOf(EngineProfile.UNITY) }
    var format by remember { mutableStateOf(profile.defaultFormat) }
    var content by remember { mutableStateOf(ExportContent.ANIMATION_ONLY) }
    var rootMotion by remember { mutableStateOf(true) }
    var fps by remember { mutableIntStateOf(30) }
    var quality by remember { mutableStateOf(ExportQuality.LOSSLESS) }
    var face by remember { mutableStateOf(true) }
    var busy by remember { mutableStateOf(false) }
    var output by remember { mutableStateOf<File?>(null) }
    var error by remember { mutableStateOf<String?>(null) }
    val scope = rememberCoroutineScope()
    val character = first?.let { store.characters.value.firstOrNull { c -> c.id == it.characterId } }

    LaunchedEffect(profile) { format = profile.defaultFormat; content = if (profile.defaultFormat == ExportFormat.FBX) ExportContent.ANIMATION_ONLY else ExportContent.FULL }
    LaunchedEffect(format) { if (format == ExportFormat.FBX || format == ExportFormat.BVH) content = ExportContent.ANIMATION_ONLY }

    val saver = rememberLauncherForActivityResult(ActivityResultContracts.CreateDocument("application/octet-stream")) { uri ->
        val f = output ?: return@rememberLauncherForActivityResult
        if (uri != null) runCatching { ctx.contentResolver.openOutputStream(uri)?.use { it.write(f.readBytes()) } }
            .onSuccess { error = "Fichier enregistré." }.onFailure { error = "Enregistrement impossible : ${it.message}" }
    }

    fun doExport() {
        val ids = selected.toList()
        busy = true; output = null
        scope.launch {
            val res = withContext(Dispatchers.Default) {
                runCatching {
                    val ch = store.load(first!!.characterId) ?: error("Personnage introuvable.")
                    val clips = ids.mapNotNull { store.loadClip(it) }
                    require(clips.isNotEmpty()) { "Aucune animation sélectionnée." }
                    val opt = ExportOptions(format, content, profile, rootMotion, fps, quality, true, face)
                    val prepared = clips.map { ExportPrep.prepare(it, ch.skeleton, opt) }
                    val base = ExportPrep.safeFileName(if (clips.size == 1) clips[0].name else ch.info.name + "_animations")
                    val dir = store.exportDir().apply { listFiles()?.forEach { it.delete() } }
                    when (format) {
                        ExportFormat.GLB, ExportFormat.GLTF -> File(dir, "$base.${format.ext}").apply { writeBytes(GltfExporter.export(ch.glb, ch.skeleton, prepared, opt)) }
                        ExportFormat.FBX -> File(dir, "$base.fbx").apply { writeBytes(FbxExporter.export(ch.skeleton, prepared, opt)) }
                        ExportFormat.BVH -> if (prepared.size == 1) File(dir, "$base.bvh").apply { writeBytes(BvhExporter.export(ch.skeleton, prepared[0])) }
                        else File(dir, "$base.zip").apply {
                            ZipOutputStream(outputStream()).use { z -> prepared.forEach { p -> z.putNextEntry(ZipEntry(ExportPrep.safeFileName(p.name) + ".bvh")); z.write(BvhExporter.export(ch.skeleton, p)); z.closeEntry() } }
                        }
                    }
                }
            }
            busy = false
            res.onSuccess { output = it }.onFailure { error = it.message ?: "Export impossible." }
        }
    }

    Column(Modifier.fillMaxSize().background(C.Bg)) {
        TopBar("Exporter", { nav.popBackStack() }, subtitle = character?.name)
        Column(Modifier.weight(1f).verticalScroll(rememberScrollState()).padding(16.dp), verticalArrangement = Arrangement.spacedBy(14.dp)) {
            SectionLabel("Profil moteur")
            Segmented(EngineProfile.entries, profile, { it.label }) { profile = it }
            Text(profile.note, style = T.small)
            SectionLabel("Format")
            Segmented(ExportFormat.entries, format, { it.label }) { format = it }
            Group {
                ExportContent.entries.forEach { c ->
                    val enabled = !(c == ExportContent.FULL && (format == ExportFormat.FBX || format == ExportFormat.BVH))
                    ListRow(c.label, subtitle = if (!enabled) (if (format == ExportFormat.BVH) "Le BVH ne contient que le squelette" else "Personnage complet en FBX non pris en charge : utilisez GLB/glTF") else null,
                        trailing = { RadioButton(content == c, { content = c }, enabled = enabled) }, onClick = if (enabled) ({ content = c }) else null)
                    if (c != ExportContent.entries.last()) GroupDivider()
                }
            }
            SectionLabel("Animations · ${selected.size} sélectionnée(s)")
            Group {
                sameChar.forEachIndexed { i, c ->
                    ListRow(c.name, subtitle = "%.2f s · %d FPS%s".format(c.duration, c.fps.toInt(), if (c.loop) " · boucle" else ""), trailing = {
                        Checkbox(c.id in selected, { v -> selected = if (v) selected + c.id else selected - c.id }, colors = CheckboxDefaults.colors(checkedColor = C.Cyan, checkmarkColor = C.Black))
                    }, onClick = { selected = if (c.id in selected) selected - c.id else selected + c.id })
                    if (i < sameChar.size - 1) GroupDivider()
                }
            }
            if (format == ExportFormat.BVH && selected.size > 1) Text("Plusieurs animations en BVH : un fichier par animation, regroupés dans un .zip.", style = T.small)
            Group {
                ToggleRow("Root motion", rootMotion, if (rootMotion) "Le déplacement est exporté" else "Le personnage reste sur place") { rootMotion = it }
                GroupDivider()
                ListRow("Images par seconde", trailing = { Row(horizontalArrangement = Arrangement.spacedBy(4.dp)) { listOf(24, 30, 60).forEach { v -> FilterChip(fps == v, { fps = v }, label = { Text("$v") }, shape = Shape4) } } })
                GroupDivider()
                ListRow("Qualité des courbes", subtitle = when (quality) { ExportQuality.LOSSLESS -> "Toutes les images"; ExportQuality.OPTIMIZED -> "Réduction de clés, écart ≤ 0,15°"; ExportQuality.COMPACT -> "Fichier léger, écart ≤ 0,6°" },
                    trailing = { TextButton({ quality = ExportQuality.entries[(quality.ordinal + 1) % 3] }) { Text(quality.label, color = C.Cyan) } })
                GroupDivider()
                ListRow("Noms des os", subtitle = "Conservés à l'identique (hiérarchie d'origine)", trailing = { Chip("OK", C.Cyan) })
                if (format == ExportFormat.GLB || format == ExportFormat.GLTF) { GroupDivider(); ToggleRow("Expressions du visage", face && content == ExportContent.FULL, "Poids des blendshapes (export complet)", enabled = content == ExportContent.FULL) { face = it } }
            }
            output?.let { f ->
                Group {
                    ListRow(f.name, Icons.Outlined.InsertDriveFile, "%.1f Ko · prêt".format(f.length() / 1024.0), accent = true)
                    GroupDivider()
                    Row(Modifier.padding(12.dp), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                        SecondaryButton("Enregistrer…", Modifier.weight(1f), icon = Icons.Outlined.Save) { saver.launch(f.name) }
                        SecondaryButton("Partager", Modifier.weight(1f), icon = Icons.Outlined.Share) {
                            val uri = FileProvider.getUriForFile(ctx, ctx.packageName + ".files", f)
                            ctx.startActivity(Intent.createChooser(Intent(Intent.ACTION_SEND).apply { type = "application/octet-stream"; putExtra(Intent.EXTRA_STREAM, uri); addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION) }, "Partager l'export"))
                        }
                    }
                }
            }
        }
        Column(Modifier.background(C.Bar).navigationBarsPadding().padding(16.dp)) {
            if (busy) LinearProgressIndicator(Modifier.fillMaxWidth().padding(bottom = 8.dp), color = C.Cyan, trackColor = C.Panel2)
            PrimaryButton("Exporter ${selected.size} animation${if (selected.size > 1) "s" else ""} · ${format.label}", Modifier.fillMaxWidth(), enabled = !busy && selected.isNotEmpty() && first != null) { doExport() }
        }
    }
    error?.let { msg -> AlertDialog(onDismissRequest = { error = null }, containerColor = C.Panel, text = { Text(msg, style = T.body) }, confirmButton = { TextButton({ error = null }) { Text("OK") } }) }
}

@Suppress("unused") private val align = Alignment.Center
