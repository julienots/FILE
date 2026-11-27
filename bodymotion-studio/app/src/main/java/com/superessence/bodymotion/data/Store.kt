package com.superessence.bodymotion.data

import android.content.Context
import com.superessence.bodymotion.anim.AnimationClip
import com.superessence.bodymotion.io.Json
import com.superessence.bodymotion.io.gltf.GltfDocument
import com.superessence.bodymotion.io.obj
import com.superessence.bodymotion.rig.BoneMap
import com.superessence.bodymotion.rig.FaceMapping
import com.superessence.bodymotion.rig.FaceRig
import com.superessence.bodymotion.rig.HumanBone
import com.superessence.bodymotion.rig.HumanoidMapper
import com.superessence.bodymotion.rig.RigReport
import com.superessence.bodymotion.rig.Skeleton
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import java.io.File
import java.util.UUID

data class CharacterInfo(
    val id: String,
    val name: String,
    val sourceFile: String,
    val format: String,
    val createdAt: Long,
    val isSample: Boolean,
    val boneMap: Map<HumanBone, Int>,
    val report: RigReport,
)

data class ClipInfo(
    val id: String, val name: String, val category: String, val characterId: String,
    val frames: Int, val fps: Double, val createdAt: Long, val loop: Boolean,
) { val duration get() = if (frames <= 1) 0.0 else (frames - 1) / fps }

data class ProjectInfo(
    val id: String, val name: String, val characterId: String?, val clipIds: List<String>, val updatedAt: Long, val engine: String,
)

/** Personnage chargé, prêt pour le rendu et le retargeting. */
class LoadedCharacter(val info: CharacterInfo, val glb: ByteArray, val doc: GltfDocument, val skeleton: Skeleton) {
    val boneMap = BoneMap(info.boneMap)
    val face: FaceRig by lazy { FaceMapping.build(doc, skeleton) }
}

data class Settings(
    val quality: Int = 2, // 0 basse, 1 moyenne, 2 haute
    val smoothing: Int = 1, // 0 faible, 1 moyen, 2 fort
    val captureFps: Int = 30,
    val keepModelFacing: Boolean = true,
    val introSeen: Boolean = false,
)

/** Stockage local (aucun serveur, aucun compte). */
class Store(private val ctx: Context) {
    private val root = File(ctx.filesDir, "studio").apply { mkdirs() }
    private val charDir = File(root, "characters").apply { mkdirs() }
    private val clipDir = File(root, "clips").apply { mkdirs() }

    private val _characters = MutableStateFlow<List<CharacterInfo>>(emptyList())
    val characters: StateFlow<List<CharacterInfo>> = _characters
    private val _clips = MutableStateFlow<List<ClipInfo>>(emptyList())
    val clips: StateFlow<List<ClipInfo>> = _clips
    private val _projects = MutableStateFlow<List<ProjectInfo>>(emptyList())
    val projects: StateFlow<List<ProjectInfo>> = _projects
    private val _settings = MutableStateFlow(Settings())
    val settings: StateFlow<Settings> = _settings

    private val cache = HashMap<String, LoadedCharacter>()

    init {
        _settings.value = loadSettings()
        _characters.value = charDir.listFiles()?.mapNotNull { readCharacter(it) }?.sortedByDescending { it.createdAt } ?: emptyList()
        _clips.value = readClipIndex()
        _projects.value = readProjects()
        ensureSample()
    }

    // ---------------------------------------------------------------- personnages

    private fun ensureSample() {
        if (_characters.value.any { it.isSample }) return
        val bytes = ctx.assets.open("mannequin.glb").use { it.readBytes() }
        addCharacter("Mannequin BodyMotion", "mannequin.glb", "GLB", GltfDocument.parseGlb(bytes), isSample = true)
    }

    fun addCharacter(name: String, source: String, format: String, doc: GltfDocument, isSample: Boolean = false, id: String = UUID.randomUUID().toString()): CharacterInfo {
        // Noms de nœuds uniques et non vides : nécessaires pour piloter les os dans le moteur 3D et dans les exports.
        val used = HashSet<String>()
        doc.nodes.forEachIndexed { i, n ->
            val m = n.obj()
            val base = (m["name"] as? String)?.takeIf { it.isNotBlank() } ?: "node_$i"
            var nm = base; var k = 1
            while (!used.add(nm)) nm = "${base}_${k++}"
            m["name"] = nm
        }
        val skel = Skeleton.fromGltf(doc)
        val map = HumanoidMapper.autoMap(doc, skel)
        val report = HumanoidMapper.report(skel, map, doc.isVrm(), format).copy(faceCompatible = !FaceMapping.build(doc, skel).isEmpty)
        val dir = File(charDir, id).apply { mkdirs() }
        val glb = doc.toGlb()
        File(dir, "model.glb").writeBytes(glb)
        val info = CharacterInfo(id, name, source, format, System.currentTimeMillis(), isSample, map.map, report)
        writeMeta(dir, info)
        _characters.value = (listOf(info) + _characters.value.filter { it.id != id })
        cache.remove(id)
        return info
    }

    fun updateBoneMap(id: String, map: Map<HumanBone, Int>) {
        val c = _characters.value.firstOrNull { it.id == id } ?: return
        val ch = load(id) ?: return
        val report = HumanoidMapper.report(ch.skeleton, BoneMap(map), ch.doc.isVrm(), c.format).copy(faceCompatible = c.report.faceCompatible)
        val n = c.copy(boneMap = map, report = report)
        writeMeta(File(charDir, id), n)
        _characters.value = _characters.value.map { if (it.id == id) n else it }
        cache.remove(id)
    }

    fun renameCharacter(id: String, name: String) {
        val c = _characters.value.firstOrNull { it.id == id } ?: return
        val n = c.copy(name = name); writeMeta(File(charDir, id), n)
        _characters.value = _characters.value.map { if (it.id == id) n else it }
        cache.remove(id)
    }

    fun deleteCharacter(id: String) {
        File(charDir, id).deleteRecursively(); cache.remove(id)
        _characters.value = _characters.value.filter { it.id != id }
        _clips.value.filter { it.characterId == id }.forEach { deleteClip(it.id) }
    }

    fun load(id: String): LoadedCharacter? = synchronized(cache) {
        cache[id]?.let { return it }
        val info = _characters.value.firstOrNull { it.id == id } ?: return null
        val glb = File(File(charDir, id), "model.glb").takeIf { it.exists() }?.readBytes() ?: return null
        val doc = GltfDocument.parseGlb(glb)
        LoadedCharacter(info, glb, doc, Skeleton.fromGltf(doc)).also { cache[id] = it }
    }

    private fun writeMeta(dir: File, c: CharacterInfo) {
        val r = c.report
        val m = linkedMapOf<String, Any?>(
            "id" to c.id, "name" to c.name, "source" to c.sourceFile, "format" to c.format, "createdAt" to c.createdAt, "sample" to c.isSample,
            "map" to c.boneMap.map { (k, v) -> k.name to v }.toMap(),
            "report" to linkedMapOf("nodes" to r.nodeCount, "joints" to r.jointCount, "mapped" to r.mapped, "missing" to r.missing.map { it.name },
                "fingers" to r.fingers, "morphs" to r.morphCount, "face" to r.faceCompatible, "vrm" to r.isVrm, "height" to r.heightMeters, "src" to r.source),
        )
        File(dir, "meta.json").writeText(Json.stringify(m))
    }

    private fun readCharacter(dir: File): CharacterInfo? = runCatching {
        val m = Json.parse(File(dir, "meta.json").readText()).obj()
        val r = m["report"].obj()
        @Suppress("UNCHECKED_CAST")
        val map = (m["map"] as Map<String, Any?>).mapNotNull { (k, v) -> runCatching { HumanBone.valueOf(k) to (v as Number).toInt() }.getOrNull() }.toMap()
        CharacterInfo(m["id"] as String, m["name"] as String, m["source"] as String, m["format"] as String, (m["createdAt"] as Number).toLong(), m["sample"] == true, map,
            RigReport((r["nodes"] as Number).toInt(), (r["joints"] as Number).toInt(), (r["mapped"] as Number).toInt(),
                (r["missing"] as List<*>).map { HumanBone.valueOf(it as String) }, (r["fingers"] as Number).toInt(), (r["morphs"] as Number).toInt(),
                r["face"] == true, r["vrm"] == true, (r["height"] as Number).toDouble(), r["src"] as String))
    }.getOrNull()

    // ---------------------------------------------------------------- animations

    fun saveClip(clip: AnimationClip) {
        File(clipDir, clip.id + ".bmac").outputStream().use { clip.write(it) }
        val info = ClipInfo(clip.id, clip.name, clip.category, clip.characterId, clip.frameCount, clip.fps, clip.createdAt, clip.loop)
        _clips.value = listOf(info) + _clips.value.filter { it.id != clip.id }
        writeClipIndex()
    }

    fun loadClip(id: String): AnimationClip? = runCatching { File(clipDir, "$id.bmac").inputStream().use { AnimationClip.read(it) } }.getOrNull()

    fun deleteClip(id: String) {
        File(clipDir, "$id.bmac").delete()
        _clips.value = _clips.value.filter { it.id != id }
        writeClipIndex()
        _projects.value.filter { id in it.clipIds }.forEach { saveProject(it.copy(clipIds = it.clipIds - id)) }
    }

    fun duplicateClip(id: String): AnimationClip? {
        val c = loadClip(id) ?: return null
        val d = c.deepCopy(UUID.randomUUID().toString()); d.name = c.name + " (copie)"; d.createdAt = System.currentTimeMillis()
        saveClip(d); return d
    }

    private fun writeClipIndex() {
        File(root, "clips.json").writeText(Json.stringify(_clips.value.map {
            linkedMapOf("id" to it.id, "name" to it.name, "category" to it.category, "characterId" to it.characterId, "frames" to it.frames,
                "fps" to it.fps, "createdAt" to it.createdAt, "loop" to it.loop)
        }))
    }

    private fun readClipIndex(): List<ClipInfo> = runCatching {
        (Json.parse(File(root, "clips.json").readText()) as List<*>).map { e ->
            val m = e.obj()
            ClipInfo(m["id"] as String, m["name"] as String, m["category"] as String, m["characterId"] as String, (m["frames"] as Number).toInt(),
                (m["fps"] as Number).toDouble(), (m["createdAt"] as Number).toLong(), m["loop"] == true)
        }.filter { File(clipDir, it.id + ".bmac").exists() }
    }.getOrDefault(emptyList())

    // ---------------------------------------------------------------- projets

    fun saveProject(p: ProjectInfo) {
        val n = p.copy(updatedAt = System.currentTimeMillis())
        _projects.value = (listOf(n) + _projects.value.filter { it.id != p.id }).sortedByDescending { it.updatedAt }
        File(root, "projects.json").writeText(Json.stringify(_projects.value.map {
            linkedMapOf("id" to it.id, "name" to it.name, "characterId" to it.characterId, "clips" to it.clipIds, "updatedAt" to it.updatedAt, "engine" to it.engine)
        }))
    }

    fun deleteProject(id: String) {
        _projects.value = _projects.value.filter { it.id != id }
        File(root, "projects.json").writeText(Json.stringify(_projects.value.map {
            linkedMapOf("id" to it.id, "name" to it.name, "characterId" to it.characterId, "clips" to it.clipIds, "updatedAt" to it.updatedAt, "engine" to it.engine)
        }))
    }

    private fun readProjects(): List<ProjectInfo> = runCatching {
        (Json.parse(File(root, "projects.json").readText()) as List<*>).map { e ->
            val m = e.obj()
            ProjectInfo(m["id"] as String, m["name"] as String, m["characterId"] as String?, (m["clips"] as List<*>).map { it as String },
                (m["updatedAt"] as Number).toLong(), m["engine"] as? String ?: "Unity")
        }
    }.getOrDefault(emptyList())

    // ---------------------------------------------------------------- réglages

    fun updateSettings(f: (Settings) -> Settings) {
        val s = f(_settings.value); _settings.value = s
        File(root, "settings.json").writeText(Json.stringify(linkedMapOf("quality" to s.quality, "smoothing" to s.smoothing, "fps" to s.captureFps,
            "facing" to s.keepModelFacing, "intro" to s.introSeen)))
    }

    private fun loadSettings(): Settings = runCatching {
        val m = Json.parse(File(root, "settings.json").readText()).obj()
        Settings((m["quality"] as Number).toInt(), (m["smoothing"] as Number).toInt(), (m["fps"] as Number).toInt(), m["facing"] != false, m["intro"] == true)
    }.getOrDefault(Settings())

    fun exportDir() = File(ctx.cacheDir, "exports").apply { mkdirs() }
}
