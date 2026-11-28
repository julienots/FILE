package com.superessence.voxea.ui

import android.app.Application
import android.content.ContentValues
import android.content.Intent
import android.net.Uri
import android.os.Build
import android.os.Environment
import android.provider.MediaStore
import android.provider.OpenableColumns
import androidx.core.content.FileProvider
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.superessence.voxea.audio.Player
import com.superessence.voxea.audio.Recorder
import com.superessence.voxea.core.Audio
import com.superessence.voxea.core.ElevenLabsConverter
import com.superessence.voxea.core.Line
import com.superessence.voxea.core.LocalConverter
import com.superessence.voxea.core.Renderer
import com.superessence.voxea.core.ScriptParser
import com.superessence.voxea.core.VoiceDsp
import com.superessence.voxea.core.VoiceSettings
import com.superessence.voxea.core.VoiceType
import com.superessence.voxea.core.Wav
import com.superessence.voxea.core.asciiTag
import com.superessence.voxea.data.Project
import com.superessence.voxea.data.ProjectStore
import com.superessence.voxea.data.Settings
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.io.File
import kotlin.math.abs

sealed interface Screen {
    data object Intro : Screen
    data object Home : Screen
    data class Edit(val id: String, val tab: Int = 0) : Screen
    data class Prompter(val id: String) : Screen
    data object SettingsScreen : Screen
}

data class PrompterState(
    val index: Int = 0,
    val recording: Boolean = false,
    val finished: Boolean = false,
    val message: String? = null,
)

data class ExportState(
    val running: Boolean = false,
    val progress: Float = 0f,
    val label: String = "",
    val result: Renderer.Result? = null,
    val savedTo: String? = null,
    val notes: List<String> = emptyList(),
)

class AppViewModel(app: Application) : AndroidViewModel(app) {
    val store = ProjectStore(File(app.filesDir, "projects"))
    val settings = Settings(app)
    val recorder = Recorder()
    val player = Player()

    private val _screen = MutableStateFlow<Screen>(Screen.Intro)
    val screen: StateFlow<Screen> = _screen
    private val _projects = MutableStateFlow<List<Project>>(emptyList())
    val projects: StateFlow<List<Project>> = _projects
    /** Incrémenté à chaque enregistrement sauvegardé, pour rafraîchir les coches. */
    private val _recVersion = MutableStateFlow(0)
    val recVersion: StateFlow<Int> = _recVersion
    private val _prompter = MutableStateFlow(PrompterState())
    val prompter: StateFlow<PrompterState> = _prompter
    private val _export = MutableStateFlow(ExportState())
    val export: StateFlow<ExportState> = _export
    private val _toast = MutableStateFlow<String?>(null)
    val toast: StateFlow<String?> = _toast
    private val _previewing = MutableStateFlow<String?>(null)
    val previewing: StateFlow<String?> = _previewing
    private val _aiEnabled = MutableStateFlow(settings.useAi)
    val aiEnabled: StateFlow<Boolean> = _aiEnabled

    init { refresh() }

    fun refresh() { _projects.value = store.list() }
    fun project(id: String): Project? = _projects.value.firstOrNull { it.id == id }
    fun go(s: Screen) { player.stop(); _screen.value = s }
    fun toast(msg: String?) { _toast.value = msg }

    fun back(): Boolean {
        when (val s = _screen.value) {
            is Screen.Prompter -> { stopSession(); go(Screen.Edit(s.id, 1)) }
            is Screen.Edit -> go(Screen.Home)
            Screen.SettingsScreen -> go(Screen.Home)
            else -> return false
        }
        return true
    }

    // ---------- Projets ----------

    fun importText(raw: String, name: String?): Project? {
        val lines = ScriptParser.parse(raw)
        if (lines.isEmpty()) { toast("Aucune réplique trouvée dans ce texte."); return null }
        val n = name?.substringBeforeLast('.')?.takeIf { it.isNotBlank() } ?: "Dialogue ${_projects.value.size + 1}"
        val p = Project.create(n, raw, lines)
        store.save(p); refresh()
        go(Screen.Edit(p.id))
        return p
    }

    fun importUri(uri: Uri) {
        val ctx = getApplication<Application>()
        val text = runCatching { ctx.contentResolver.openInputStream(uri)?.use { it.readBytes().toString(Charsets.UTF_8) } }.getOrNull()
        if (text == null) { toast("Impossible de lire ce fichier."); return }
        var name: String? = null
        runCatching {
            ctx.contentResolver.query(uri, arrayOf(OpenableColumns.DISPLAY_NAME), null, null, null)?.use { c ->
                if (c.moveToFirst()) name = c.getString(0)
            }
        }
        importText(text.removePrefix("﻿"), name)
    }

    fun update(p: Project) {
        store.save(p)
        _projects.value = _projects.value.map { if (it.id == p.id) p else it }
    }

    fun rename(p: Project, name: String) { if (name.isNotBlank()) update(p.copy(name = name.trim())) }

    fun delete(p: Project) { store.delete(p); refresh() }

    fun reparse(p: Project, raw: String) {
        val lines = ScriptParser.parse(raw)
        if (lines.isEmpty()) { toast("Aucune réplique trouvée."); return }
        val fresh = Project.create(p.name, raw, lines)
        // conserve les voix déjà choisies pour les personnages existants
        update(fresh.copy(id = p.id, createdAt = p.createdAt,
            castType = fresh.castType.mapValues { (c, t) -> p.castType[c] ?: t },
            castSettings = p.castSettings.filterKeys { it in fresh.characters }))
        _recVersion.value++
    }

    // ---------- Prompteur ----------

    fun openPrompter(p: Project, from: Int? = null) {
        val start = from ?: p.lines.firstOrNull { !store.hasRecording(p, it.index) }?.index ?: 0
        _prompter.value = PrompterState(index = start)
        go(Screen.Prompter(p.id))
    }

    fun startSession(): Boolean {
        player.stop()
        if (!recorder.start()) { _prompter.value = _prompter.value.copy(message = "Micro indisponible."); return false }
        recorder.beginSegment()
        _prompter.value = _prompter.value.copy(recording = true, finished = false, message = null)
        return true
    }

    fun stopSession() {
        if (_prompter.value.recording) recorder.endSegment()
        recorder.stop()
        _prompter.value = _prompter.value.copy(recording = false)
    }

    /** Suivant : enregistre la réplique en cours (si on enregistre) puis passe à la suivante. */
    fun prompterNext(p: Project) {
        val st = _prompter.value
        if (!st.recording) {
            if (st.index < p.lines.lastIndex) _prompter.value = st.copy(index = st.index + 1, message = null)
            return
        }
        val seg = recorder.endSegment()
        saveSegment(p, st.index, seg)
        if (st.index >= p.lines.lastIndex) {
            recorder.stop()
            _prompter.value = st.copy(recording = false, finished = true, message = null)
        } else {
            recorder.beginSegment()
            _prompter.value = st.copy(index = st.index + 1, message = null)
        }
    }

    fun prompterPrev(p: Project) {
        val st = _prompter.value
        if (st.recording) recorder.beginSegment() // jette la prise en cours
        _prompter.value = st.copy(index = (st.index - 1).coerceAtLeast(0), finished = false, message = null)
    }

    /** Refaire la réplique affichée : jette la prise en cours et recommence. */
    fun prompterRedo() {
        val st = _prompter.value
        if (st.recording) {
            recorder.beginSegment()
            _prompter.value = st.copy(message = "On reprend cette réplique.")
        } else startSession()
    }

    fun prompterJump(index: Int) {
        val st = _prompter.value
        if (st.recording) recorder.beginSegment()
        _prompter.value = st.copy(index = index, finished = false, message = null)
    }

    fun replay(p: Project, index: Int) {
        if (_prompter.value.recording) stopSession()
        val f = store.recFile(p, index)
        if (f.exists()) player.toggle(f) else toast("Pas encore enregistrée.")
    }

    private fun saveSegment(p: Project, index: Int, seg: FloatArray) {
        viewModelScope.launch(Dispatchers.IO) {
            val trimmed = Audio.trimSilence(seg, recorder.sampleRate)
            var peak = 0f
            for (v in trimmed) peak = maxOf(peak, abs(v))
            Wav.write(store.recFile(p, index), trimmed, recorder.sampleRate)
            if (peak < 0.02f || trimmed.size < recorder.sampleRate / 5) {
                toast("Réplique ${index + 1} : presque rien entendu. Tu peux la refaire.")
            }
            _recVersion.value++
        }
    }

    // ---------- Voix ----------

    fun setCharacterVoice(p: Project, c: String, t: VoiceType) = update(p.withCharacterVoice(c, t))
    fun setCharacterSettings(p: Project, c: String, s: VoiceSettings) = update(p.withCharacterSettings(c, s))
    fun setLineVoice(p: Project, index: Int, t: VoiceType?) = update(p.withLineVoice(index, t))

    private var previewJob: Job? = null

    /** Aperçu : la première réplique enregistrée de ce personnage (sinon n'importe laquelle), transformée. */
    fun preview(p: Project, character: String, settings: VoiceSettings) {
        if (_previewing.value == character) { player.stop(); _previewing.value = null; return }
        val line = p.lines.firstOrNull { it.character == character && store.hasRecording(p, it.index) }
            ?: p.lines.firstOrNull { store.hasRecording(p, it.index) }
        if (line == null) { toast("Enregistre au moins une réplique pour écouter l'aperçu."); return }
        previewJob?.cancel()
        _previewing.value = character
        previewJob = viewModelScope.launch {
            val f = withContext(Dispatchers.Default) {
                val (x, sr) = Wav.read(store.recFile(p, line.index))
                val cut = x.copyOfRange(0, minOf(x.size, sr * 8))
                val y = VoiceDsp.process(cut, sr, settings)
                File(getApplication<Application>().cacheDir, "preview.wav").also { Wav.write(it, y, sr) }
            }
            player.play(f) { _previewing.value = null }
        }
    }

    // ---------- Export ----------

    fun setUseAi(v: Boolean) { settings.useAi = v; _aiEnabled.value = settings.useAi }

    fun generate(p: Project) {
        if (_export.value.running) return
        player.stop()
        val notes = mutableListOf<String>()
        val converter = if (settings.useAi) ElevenLabsConverter(settings.apiKey, settings.voiceIds()) { synchronized(notes) { notes += it } }
        else LocalConverter
        _export.value = ExportState(running = true, label = "Préparation…")
        viewModelScope.launch(Dispatchers.Default) {
            try {
                val r = Renderer(store).render(p, converter) { f, label -> _export.value = _export.value.copy(progress = f, label = label) }
                val saved = runCatching { savePublic(p, r) }.getOrElse { notes += "Copie dans Musique impossible : ${it.message}"; null }
                _export.value = ExportState(result = r, savedTo = saved, notes = notes + r.errors, progress = 1f)
            } catch (e: Exception) {
                _export.value = ExportState(notes = listOf("Erreur : ${e.message}"))
            }
        }
    }

    fun clearExport() { _export.value = ExportState() }

    private fun folderName(p: Project) = asciiTag(p.name, "DIALOGUE").lowercase().replaceFirstChar { it.uppercase() }

    /** Copie les MP3 dans Musique/VOXEA/<projet> (visible dans les fichiers et la galerie audio). */
    private fun savePublic(p: Project, r: Renderer.Result): String {
        val ctx = getApplication<Application>()
        val files = listOfNotNull(r.full) + r.files
        val rel = "${Environment.DIRECTORY_MUSIC}/VOXEA/${folderName(p)}"
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            val resolver = ctx.contentResolver
            val coll = MediaStore.Audio.Media.getContentUri(MediaStore.VOLUME_EXTERNAL_PRIMARY)
            // supprime l'ancienne version des mêmes fichiers
            resolver.delete(coll, "${MediaStore.MediaColumns.RELATIVE_PATH}=?", arrayOf("$rel/"))
            for (f in files) {
                val v = ContentValues().apply {
                    put(MediaStore.MediaColumns.DISPLAY_NAME, f.name)
                    put(MediaStore.MediaColumns.MIME_TYPE, "audio/mpeg")
                    put(MediaStore.MediaColumns.RELATIVE_PATH, rel)
                    put(MediaStore.Audio.Media.IS_PENDING, 1)
                }
                val uri = resolver.insert(coll, v) ?: continue
                resolver.openOutputStream(uri)?.use { o -> f.inputStream().use { it.copyTo(o) } }
                v.clear(); v.put(MediaStore.Audio.Media.IS_PENDING, 0)
                resolver.update(uri, v, null, null)
            }
        } else {
            @Suppress("DEPRECATION")
            val dir = File(Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_MUSIC), "VOXEA/${folderName(p)}")
            dir.mkdirs()
            for (f in files) f.copyTo(File(dir, f.name), overwrite = true)
        }
        return "Musique/VOXEA/${folderName(p)}"
    }

    fun shareIntent(files: List<File>, title: String): Intent {
        val ctx = getApplication<Application>()
        val shareDir = File(ctx.cacheDir, "share").apply { mkdirs() }
        val uris = ArrayList(files.map { f ->
            val copy = if (f.startsWith(shareDir) || f.startsWith(File(ctx.filesDir, "projects"))) f else f.copyTo(File(shareDir, f.name), true)
            FileProvider.getUriForFile(ctx, ctx.packageName + ".files", copy)
        })
        val i = if (uris.size == 1) Intent(Intent.ACTION_SEND).putExtra(Intent.EXTRA_STREAM, uris[0])
        else Intent(Intent.ACTION_SEND_MULTIPLE).putParcelableArrayListExtra(Intent.EXTRA_STREAM, uris)
        i.type = if (files.all { it.extension == "mp3" }) "audio/mpeg" else if (files.size == 1 && files[0].extension == "zip") "application/zip" else "*/*"
        i.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
        return Intent.createChooser(i, title)
    }

    fun zipFor(p: Project, r: Renderer.Result): File {
        val ctx = getApplication<Application>()
        return Renderer(store).zip(p, r, File(ctx.cacheDir, "share/VOXEA_${folderName(p)}.zip"))
    }

    override fun onCleared() { recorder.stop(); player.stop() }

    companion object {
        val EXAMPLE = """
            # Exemple — La gare (script écrit par Claude)

            NARRATEUR : Une petite gare, tard le soir. La pluie tape sur la verrière.
            LÉA [FEMME] : Tu crois vraiment qu'il va revenir ce soir ?
            MARC [HOMME] : Je n'en sais rien… Mais il a promis.
            TOM [ENFANT] : Maman, c'est lui ? C'est son train ?
            PAPI JO [ÂGÉ] : Patience, mon petit. Les trains de nuit sont toujours en retard.
            ANNONCE [ROBOT] : Le train en provenance de Lyon entre en gare, voie deux.
            LÉA : Le voilà.
        """.trimIndent()
    }
}

/** Liste des répliques enregistrées, utile aux écrans. */
fun ProjectStore.recorded(p: Project): Set<Int> = p.lines.filter { hasRecording(p, it.index) }.map(Line::index).toSet()
