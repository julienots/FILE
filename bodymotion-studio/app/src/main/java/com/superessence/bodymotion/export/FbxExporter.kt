package com.superessence.bodymotion.export

import com.superessence.bodymotion.io.fbx.FbxBinary
import com.superessence.bodymotion.io.fbx.FbxNode
import com.superessence.bodymotion.io.fbx.FbxRaw
import com.superessence.bodymotion.math.Quat
import com.superessence.bodymotion.math.Vec3
import com.superessence.bodymotion.rig.Skeleton

/**
 * Export FBX binaire 7.4 (squelette + animations).
 * La hiérarchie d'origine est conservée (mêmes noms, mêmes rotations de repos), chaque clip devient une
 * « Take » (AnimationStack). Unités : mètres (UnitScaleFactor = 100), axe Y vers le haut.
 *
 * Limite assumée : le FBX produit ne contient pas les maillages ; pour exporter le personnage complet
 * (maillage + matériaux + animations) utilisez GLB/glTF.
 */
object FbxExporter {
    private const val KTIME = 46186158000L

    fun export(skel: Skeleton, clips: List<PreparedClip>, opt: ExportOptions): ByteArray {
        val hips = clips.first().hipsNode
        var top = hips
        while (skel.nodes[top].parent >= 0 && skel.nodes[skel.nodes[top].parent].isJoint) top = skel.nodes[top].parent
        // Nœuds exportés : ancêtres (Null) + sous-arbre d'os (LimbNode).
        val ancestors = ArrayList<Int>()
        run { var p = skel.nodes[top].parent; while (p >= 0) { ancestors.add(0, p); p = skel.nodes[p].parent } }
        val joints = ArrayList<Int>()
        fun collect(i: Int) { joints.add(i); skel.nodes[i].children.filter { skel.nodes[it].isJoint }.forEach { collect(it) } }
        collect(top)
        val addRoot = clips.any { it.rootT != null }
        val names = BvhExporter.uniqueNames(skel, ancestors + joints).toMutableMap()

        var nextId = 1_000_000L
        fun newId() = nextId++
        val modelId = HashMap<Int, Long>()
        val rootId = if (addRoot) newId() else -1L
        (ancestors + joints).forEach { modelId[it] = newId() }

        val objects = FbxNode("Objects")
        val connections = FbxNode("Connections")
        fun oo(child: Long, parent: Long) = connections.add("C", "OO", child, parent)
        fun op(child: Long, parent: Long, prop: String) = connections.add("C", "OP", child, parent, prop)

        fun model(id: Long, name: String, type: String, t: Vec3, r: Quat, s: Vec3) {
            val m = objects.add("Model", id, "$name\u0000\u0001Model", type)
            m.add("Version", 232)
            val e = r.toEulerXYZDegrees()
            m.p70("RotationOrder", "enum", "", "", 0)
            m.p70("InheritType", "enum", "", "", 1)
            m.p70("DefaultAttributeIndex", "int", "Integer", "", 0)
            m.p70("Lcl Translation", "Lcl Translation", "", "A", t.x, t.y, t.z)
            m.p70("Lcl Rotation", "Lcl Rotation", "", "A", e.x, e.y, e.z)
            m.p70("Lcl Scaling", "Lcl Scaling", "", "A", s.x, s.y, s.z)
            m.add("Shading", true)
            m.add("Culling", "CullingOff")
            if (type == "LimbNode") {
                val aid = newId()
                val a = objects.add("NodeAttribute", aid, "$name\u0000\u0001NodeAttribute", "LimbNode")
                a.p70("Size", "double", "Number", "", 33.0)
                a.add("TypeFlags", "Skeleton")
                oo(aid, id)
            }
        }

        for (i in ancestors + joints) {
            val n = skel.nodes[i]
            model(modelId[i]!!, names[i]!!, if (i in ancestors) "Null" else "LimbNode", n.rest.t, n.rest.r, n.rest.s)
        }
        if (addRoot) model(rootId, "root", "LimbNode", Vec3.ZERO, Quat.IDENTITY, Vec3.ONE)
        // Hiérarchie
        for (i in ancestors + joints) {
            val p = skel.nodes[i].parent
            val parentId = when {
                i == hips && addRoot -> rootId
                p < 0 -> 0L
                else -> modelId[p] ?: 0L
            }
            oo(modelId[i]!!, parentId)
        }
        if (addRoot) oo(rootId, skel.nodes[hips].parent.let { if (it >= 0) modelId[it]!! else 0L })

        // Animations
        var maxStop = 0L
        val tol = opt.quality.toleranceDeg
        for (c in clips) {
            val stackId = newId(); val layerId = newId()
            val stop = ((c.frames - 1).coerceAtLeast(0) * KTIME / c.fps).toLong()
            maxStop = maxOf(maxStop, stop)
            val st = objects.add("AnimationStack", stackId, "${c.name}\u0000\u0001AnimStack", "")
            st.p70("LocalStop", "KTime", "Time", "", stop)
            st.p70("ReferenceStop", "KTime", "Time", "", stop)
            objects.add("AnimationLayer", layerId, "BaseLayer\u0000\u0001AnimLayer", "")
            oo(layerId, stackId)

            fun curveNode(target: Long, prop: String, short: String, frames: IntArray, values: (Int) -> Vec3) {
                val cnId = newId()
                val v0 = values(frames.first())
                val cn = objects.add("AnimationCurveNode", cnId, "$short\u0000\u0001AnimCurveNode", "")
                cn.p70("d|X", "Number", "", "A", v0.x); cn.p70("d|Y", "Number", "", "A", v0.y); cn.p70("d|Z", "Number", "", "A", v0.z)
                oo(cnId, layerId); op(cnId, target, prop)
                val times = LongArray(frames.size) { (frames[it] * KTIME / c.fps).toLong() }
                val all = frames.map(values)
                for ((axis, sel) in listOf<Pair<String, (Vec3) -> Double>>("d|X" to { it.x }, "d|Y" to { it.y }, "d|Z" to { it.z })) {
                    val cid = newId()
                    val cv = objects.add("AnimationCurve", cid, "\u0000\u0001AnimCurve", "")
                    cv.add("Default", sel(v0))
                    cv.add("KeyVer", 4008)
                    cv.add("KeyTime", times)
                    cv.add("KeyValueFloat", FloatArray(all.size) { sel(all[it]).toFloat() })
                    cv.add("KeyAttrFlags", intArrayOf(24836))
                    cv.add("KeyAttrDataFloat", floatArrayOf(0f, 0f, 9.419963e-30f, 0f))
                    cv.add("KeyAttrRefCount", intArrayOf(times.size))
                    op(cid, cnId, axis)
                }
            }

            c.nodes.forEachIndexed { k, node ->
                val id = modelId[node] ?: return@forEachIndexed
                val frames = c.keyFramesFor(k, tol)
                var prev: Vec3? = null
                val eul = HashMap<Int, Vec3>()
                for (f in frames) {
                    var e = c.rotations[f][k].toEulerXYZDegrees()
                    prev?.let { p -> e = Vec3(unwrap(e.x, p.x), unwrap(e.y, p.y), unwrap(e.z, p.z)) }
                    prev = e; eul[f] = e
                }
                curveNode(id, "Lcl Rotation", "R", frames) { eul[it]!! }
            }
            modelId[c.hipsNode]?.let { id -> curveNode(id, "Lcl Translation", "T", c.keyFramesForVec(c.hipsT, tol)) { c.hipsT[it] } }
            if (addRoot && c.rootT != null) curveNode(rootId, "Lcl Translation", "T", c.keyFramesForVec(c.rootT, tol)) { c.rootT[it] }
        }

        val fps = opt.fps
        val timeMode = when (fps) { 24 -> 11; 25 -> 10; 30 -> 6; 48 -> 5; 50 -> 4; 60 -> 3; 120 -> 1; else -> 14 }

        val header = FbxNode("FBXHeaderExtension").apply {
            add("FBXHeaderVersion", 1003); add("FBXVersion", 7400); add("EncryptionType", 0)
            add("CreationTimeStamp").apply {
                add("Version", 1000); add("Year", 1970); add("Month", 1); add("Day", 1)
                add("Hour", 10); add("Minute", 0); add("Second", 0); add("Millisecond", 0)
            }
            add("Creator", "BodyMotion Studio (Super Essence Studio)")
        }
        val global = FbxNode("GlobalSettings").apply {
            add("Version", 1000)
            p70("UpAxis", "int", "Integer", "", 1); p70("UpAxisSign", "int", "Integer", "", 1)
            p70("FrontAxis", "int", "Integer", "", 2); p70("FrontAxisSign", "int", "Integer", "", 1)
            p70("CoordAxis", "int", "Integer", "", 0); p70("CoordAxisSign", "int", "Integer", "", 1)
            p70("OriginalUpAxis", "int", "Integer", "", 1); p70("OriginalUpAxisSign", "int", "Integer", "", 1)
            p70("UnitScaleFactor", "double", "Number", "", 100.0); p70("OriginalUnitScaleFactor", "double", "Number", "", 100.0)
            p70("AmbientColor", "ColorRGB", "Color", "", 0.0, 0.0, 0.0)
            p70("DefaultCamera", "KString", "", "", "Producer Perspective")
            p70("TimeMode", "enum", "", "", timeMode)
            p70("TimeSpanStart", "KTime", "Time", "", 0L)
            p70("TimeSpanStop", "KTime", "Time", "", maxStop)
            p70("CustomFrameRate", "double", "Number", "", fps.toDouble())
        }
        val documents = FbxNode("Documents").apply {
            add("Count", 1)
            val d = add("Document", newId(), "Scene", "Scene")
            d.p70("SourceObject", "object", "", "")
            d.p70("ActiveAnimStackName", "KString", "", "", clips.first().name)
            d.add("RootNode", 0L)
        }
        val modelCount = ancestors.size + joints.size + (if (addRoot) 1 else 0)
        val definitions = FbxNode("Definitions").apply {
            add("Version", 100)
            val curveNodes = objects.children.count { it.name == "AnimationCurveNode" }
            val curves = objects.children.count { it.name == "AnimationCurve" }
            val attrs = objects.children.count { it.name == "NodeAttribute" }
            add("Count", 1 + modelCount + attrs + clips.size * 2 + curveNodes + curves)
            add("ObjectType", "GlobalSettings").add("Count", 1)
            add("ObjectType", "Model").add("Count", modelCount)
            add("ObjectType", "NodeAttribute").add("Count", attrs)
            add("ObjectType", "AnimationStack").add("Count", clips.size)
            add("ObjectType", "AnimationLayer").add("Count", clips.size)
            add("ObjectType", "AnimationCurveNode").add("Count", curveNodes)
            add("ObjectType", "AnimationCurve").add("Count", curves)
        }
        val takes = FbxNode("Takes").apply {
            add("Current", clips.first().name)
            for (c in clips) {
                val stop = ((c.frames - 1).coerceAtLeast(0) * KTIME / c.fps).toLong()
                add("Take", c.name).apply {
                    add("FileName", c.name.replace(' ', '_') + ".tak")
                    add("LocalTime", 0L, stop); add("ReferenceTime", 0L, stop)
                }
            }
        }
        val top0 = listOf(
            header,
            FbxNode("FileId", mutableListOf(FbxRaw(FbxBinary.FILE_ID))),
            FbxNode("CreationTime", mutableListOf("1970-01-01 10:00:00:000")),
            FbxNode("Creator", mutableListOf("BodyMotion Studio (Super Essence Studio)")),
            global, documents, FbxNode("References"), definitions, objects, connections, takes,
        )
        return FbxBinary.write(top0)
    }

    private fun unwrap(a: Double, prev: Double): Double { var x = a; while (x - prev > 180) x -= 360; while (x - prev < -180) x += 360; return x }
}
