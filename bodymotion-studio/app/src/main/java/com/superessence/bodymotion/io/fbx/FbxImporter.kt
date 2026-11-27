package com.superessence.bodymotion.io.fbx

import com.superessence.bodymotion.io.gltf.GltfDocument
import com.superessence.bodymotion.math.Mat4
import com.superessence.bodymotion.math.Quat
import com.superessence.bodymotion.math.Vec3
import kotlin.math.abs

/**
 * Convertit un FBX binaire (7.x) en document glTF interne : hiérarchie, maillages, skin (clusters),
 * matériaux (couleur + texture diffuse + normal map), textures intégrées et blendshapes.
 * Les textures externes sont retrouvées via [resolve] (ex. fichiers d'un .zip importé).
 */
class FbxImporter(private val resolve: (String) -> ByteArray? = { null }) {

    private class Model(val id: Long, val name: String, val type: String, val node: FbxNode) {
        var parent = 0L
        val children = ArrayList<Long>()
        var gltfIndex = -1
    }

    private class Conn(val child: Long, val parent: Long, val prop: String?)

    fun convert(bytes: ByteArray): GltfDocument {
        val top = FbxBinary.read(bytes)
        val objects = top.firstOrNull { it.name == "Objects" } ?: throw IllegalArgumentException("FBX sans objets.")
        val conns = (top.firstOrNull { it.name == "Connections" }?.children ?: emptyList())
            .filter { it.name == "C" && it.props.size >= 3 }
            .map { Conn((it.props[1] as Number).toLong(), (it.props[2] as Number).toLong(), it.props.getOrNull(3) as? String) }
        val byId = HashMap<Long, FbxNode>()
        objects.children.forEach { o -> (o.props.firstOrNull() as? Number)?.let { byId[it.toLong()] = o } }
        fun name(n: FbxNode) = (n.props.getOrNull(1) as? String)?.substringBefore('\u0000')?.substringAfter("::")?.ifEmpty { null } ?: n.name
        fun childrenOf(parent: Long, type: String? = null) = conns.filter { it.parent == parent && (type == null || byId[it.child]?.name == type) }
        fun parentsOf(child: Long, type: String? = null) = conns.filter { it.child == child && (type == null || byId[it.parent]?.name == type) }

        // ---------- Modèles et hiérarchie
        val models = LinkedHashMap<Long, Model>()
        objects.all("Model").forEach { m -> val id = (m.props[0] as Number).toLong(); models[id] = Model(id, name(m), m.props.getOrNull(2) as? String ?: "", m) }
        for (c in conns) if (models.containsKey(c.child) && (models.containsKey(c.parent) || c.parent == 0L)) {
            models[c.child]!!.parent = c.parent
        }
        models.values.forEach { m -> if (m.parent != 0L) models[m.parent]?.children?.add(m.id) }

        val doc = GltfDocument(linkedMapOf("asset" to linkedMapOf<String, Any?>("version" to "2.0", "generator" to "BodyMotion Studio FBX import")), ByteArray(0))
        val nodes = doc.nodes

        // Nœud racine de conversion (axes et unités).
        val gs = top.firstOrNull { it.name == "GlobalSettings" }
        val conv = axisConversion(gs)
        nodes.add(linkedMapOf<String, Any?>("name" to "FBX_Root", "matrix" to conv.m.toList(), "children" to mutableListOf<Any?>()))
        val rootChildren = mutableListOf<Any?>()

        // Création des nœuds glTF (ordre topologique).
        fun create(m: Model) {
            m.gltfIndex = nodes.size
            val local = localMatrix(m.node)
            val trs = local.decompose()
            nodes.add(linkedMapOf<String, Any?>(
                "name" to m.name,
                "translation" to listOf(trs.t.x, trs.t.y, trs.t.z),
                "rotation" to listOf(trs.r.x, trs.r.y, trs.r.z, trs.r.w),
                "scale" to listOf(trs.s.x, trs.s.y, trs.s.z),
            ))
            m.children.mapNotNull { models[it] }.forEach { create(it) }
            if (m.children.isNotEmpty()) (nodes[m.gltfIndex] as MutableMap<String, Any?>)["children"] = m.children.mapNotNull { models[it]?.gltfIndex }.toMutableList<Any?>()
        }
        models.values.filter { it.parent == 0L || !models.containsKey(it.parent) }.forEach { create(it); rootChildren.add(it.gltfIndex) }
        (nodes[0] as MutableMap<String, Any?>)["children"] = rootChildren

        // Matrices globales au repos des modèles (dans l'espace FBX converti).
        val global = HashMap<Long, Mat4>()
        fun globalOf(id: Long): Mat4 = global.getOrPut(id) {
            val m = models[id]!!
            val l = localMatrix(m.node)
            if (m.parent != 0L && models.containsKey(m.parent)) globalOf(m.parent) * l else conv * l
        }

        // ---------- Matériaux et textures
        val images = mutableListOf<Any?>(); val textures = mutableListOf<Any?>(); val materials = mutableListOf<Any?>()
        val imageCache = HashMap<Long, Int>()
        fun textureFor(texId: Long): Int? {
            val tex = byId[texId] ?: return null
            imageCache[texId]?.let { return it }
            val video = childrenOf(texId, "Video").firstOrNull()?.let { byId[it.child] }
            var data = (video?.child("Content")?.props?.firstOrNull() as? FbxRaw)?.bytes?.takeIf { it.isNotEmpty() }
            val fileName = (tex.child("RelativeFilename")?.props?.firstOrNull() as? String) ?: (tex.child("FileName")?.props?.firstOrNull() as? String)
                ?: (video?.child("RelativeFilename")?.props?.firstOrNull() as? String)
            if (data == null && fileName != null) {
                val base = fileName.replace('\\', '/').substringAfterLast('/')
                data = resolve(base) ?: resolve(fileName)
            }
            if (data == null) return null
            val mime = when {
                data.size > 4 && data[0] == 0x89.toByte() && data[1] == 'P'.code.toByte() -> "image/png"
                data.size > 2 && data[0] == 0xFF.toByte() && data[1] == 0xD8.toByte() -> "image/jpeg"
                else -> return null // formats non pris en charge par glTF (TGA, PSD, DDS…)
            }
            val bv = doc.addBufferView(data)
            images.add(linkedMapOf<String, Any?>("bufferView" to bv, "mimeType" to mime, "name" to (fileName ?: "texture")))
            textures.add(linkedMapOf<String, Any?>("source" to images.size - 1))
            return (textures.size - 1).also { imageCache[texId] = it }
        }
        val materialIndex = HashMap<Long, Int>()
        objects.all("Material").forEach { mat ->
            val id = (mat.props[0] as Number).toLong()
            val color = (mat.prop70("DiffuseColor") ?: mat.prop70("Diffuse"))?.mapNotNull { (it as? Number)?.toDouble() }?.take(3) ?: listOf(0.8, 0.8, 0.8)
            val factor = mat.prop70("DiffuseFactor")?.firstOrNull()?.let { (it as Number).toDouble() } ?: 1.0
            val opacity = mat.prop70("Opacity")?.firstOrNull()?.let { (it as Number).toDouble() }
                ?: mat.prop70("TransparencyFactor")?.firstOrNull()?.let { 1.0 - (it as Number).toDouble() } ?: 1.0
            val texConns = childrenOf(id, "Texture")
            val diffuse = texConns.firstOrNull { it.prop?.contains("Diffuse", true) == true || it.prop?.contains("baseColor", true) == true || it.prop?.contains("base_color", true) == true }
                ?: texConns.firstOrNull { it.prop?.contains("Normal", true) != true && it.prop?.contains("Bump", true) != true }
            val normal = texConns.firstOrNull { it.prop?.contains("Normal", true) == true || it.prop == "Bump" }
            val pbr = linkedMapOf<String, Any?>(
                "baseColorFactor" to listOf(color[0] * factor, color[1] * factor, color[2] * factor, opacity.coerceIn(0.0, 1.0)),
                "metallicFactor" to 0.0, "roughnessFactor" to 0.75,
            )
            diffuse?.let { c -> textureFor(c.child)?.let { t -> pbr["baseColorTexture"] = linkedMapOf("index" to t); pbr["baseColorFactor"] = listOf(1.0, 1.0, 1.0, opacity.coerceIn(0.0, 1.0)) } }
            val gm = linkedMapOf<String, Any?>("name" to name(mat), "pbrMetallicRoughness" to pbr)
            normal?.let { c -> textureFor(c.child)?.let { t -> gm["normalTexture"] = linkedMapOf("index" to t) } }
            if (opacity < 0.99) gm["alphaMode"] = "BLEND"
            materials.add(gm); materialIndex[id] = materials.size - 1
        }
        var defaultMaterial = -1
        fun defaultMat(): Int {
            if (defaultMaterial < 0) { materials.add(linkedMapOf("name" to "Default", "pbrMetallicRoughness" to linkedMapOf("baseColorFactor" to listOf(0.75, 0.77, 0.8, 1.0), "metallicFactor" to 0.0, "roughnessFactor" to 0.7))); defaultMaterial = materials.size - 1 }
            return defaultMaterial
        }

        // ---------- Maillages
        val meshes = mutableListOf<Any?>(); val skins = mutableListOf<Any?>()
        for (geo in objects.all("Geometry").filter { it.props.getOrNull(2) == "Mesh" }) {
            val gid = (geo.props[0] as Number).toLong()
            val ownerId = parentsOf(gid).firstOrNull { models.containsKey(it.parent) }?.parent ?: continue
            val owner = models[ownerId]!!
            val built = buildMesh(geo, owner.node, gid, ::childrenOf, byId, models, global = ::globalOf) ?: continue
            val (prims, skinData, morphNames) = built
            val matIds = childrenOf(ownerId, "Material").map { it.child }
            val gPrims = mutableListOf<Any?>()
            for (p in prims) {
                val attrs = linkedMapOf<String, Any?>(
                    "POSITION" to doc.addFloatAccessor(p.pos, "VEC3", withMinMax = true, target = 34962),
                    "NORMAL" to doc.addFloatAccessor(p.nrm, "VEC3", target = 34962),
                )
                if (p.uv != null) attrs["TEXCOORD_0"] = doc.addFloatAccessor(p.uv, "VEC2", target = 34962)
                if (p.joints != null && p.weights != null) {
                    attrs["JOINTS_0"] = doc.addUShortVec4Accessor(p.joints)
                    attrs["WEIGHTS_0"] = doc.addFloatAccessor(p.weights, "VEC4", target = 34962)
                }
                val prim = linkedMapOf<String, Any?>("attributes" to attrs, "indices" to doc.addIndexAccessor(p.indices),
                    "material" to (matIds.getOrNull(p.material)?.let { materialIndex[it] } ?: defaultMat()))
                if (p.morphs.isNotEmpty()) prim["targets"] = p.morphs.map { linkedMapOf<String, Any?>("POSITION" to doc.addFloatAccessor(it, "VEC3", withMinMax = true)) }.toMutableList<Any?>()
                gPrims.add(prim)
            }
            val mesh = linkedMapOf<String, Any?>("name" to owner.name, "primitives" to gPrims)
            if (morphNames.isNotEmpty()) {
                mesh["extras"] = linkedMapOf("targetNames" to morphNames)
                mesh["weights"] = morphNames.map { 0.0 }
            }
            meshes.add(mesh)
            // Nœud du maillage : enfant du modèle propriétaire (sans transformation propre si skinné).
            val meshNode = linkedMapOf<String, Any?>("name" to owner.name + "_mesh", "mesh" to meshes.size - 1)
            if (skinData != null) {
                val ibm = FloatArray(skinData.joints.size * 16)
                skinData.ibm.forEachIndexed { j, m -> m.toFloatArray().copyInto(ibm, j * 16) }
                skins.add(linkedMapOf<String, Any?>(
                    "joints" to skinData.joints.map { models[it]!!.gltfIndex }.toMutableList<Any?>(),
                    "inverseBindMatrices" to doc.addFloatAccessor(ibm, "MAT4"),
                    "name" to owner.name + "_skin",
                ))
                meshNode["skin"] = skins.size - 1
                // Un maillage skinné est indépendant de la transformation de son nœud : on le place sous la racine.
                nodes.add(meshNode); rootChildren.add(nodes.size - 1)
            } else {
                nodes.add(meshNode)
                val ownerNode = nodes[owner.gltfIndex] as MutableMap<String, Any?>
                @Suppress("UNCHECKED_CAST")
                val ch = (ownerNode["children"] as? MutableList<Any?>) ?: mutableListOf<Any?>().also { ownerNode["children"] = it }
                ch.add(nodes.size - 1)
            }
        }
        (nodes[0] as MutableMap<String, Any?>)["children"] = rootChildren
        doc.json["scenes"] = mutableListOf<Any?>(linkedMapOf<String, Any?>("nodes" to mutableListOf<Any?>(0)))
        doc.json["scene"] = 0
        if (meshes.isNotEmpty()) doc.json["meshes"] = meshes
        if (skins.isNotEmpty()) doc.json["skins"] = skins
        if (materials.isNotEmpty()) doc.json["materials"] = materials
        if (textures.isNotEmpty()) { doc.json["textures"] = textures; doc.json["images"] = images }
        if (meshes.isEmpty() && models.isEmpty()) throw IllegalArgumentException("Aucun objet exploitable dans ce FBX.")
        return doc
    }

    private class Prim(
        val pos: FloatArray, val nrm: FloatArray, val uv: FloatArray?, val joints: IntArray?, val weights: FloatArray?,
        val indices: IntArray, val material: Int, val morphs: List<FloatArray>,
    )

    private class SkinData(val joints: List<Long>, val ibm: List<Mat4>)

    private fun buildMesh(
        geo: FbxNode, modelNode: FbxNode, gid: Long,
        childrenOf: (Long, String?) -> List<Conn>, byId: Map<Long, FbxNode>, models: Map<Long, Model>, global: (Long) -> Mat4,
    ): Triple<List<Prim>, SkinData?, List<String>>? {
        val verts = geo.child("Vertices")?.props?.firstOrNull()?.asDoubles() ?: return null
        val pvi = geo.child("PolygonVertexIndex")?.props?.firstOrNull()?.asInts() ?: return null
        val geomM = geometricMatrix(modelNode)
        val cpCount = verts.size / 3
        val cps = Array(cpCount) { geomM.transformPoint(Vec3(verts[it * 3], verts[it * 3 + 1], verts[it * 3 + 2])) }
        val normalM = geomM.inverse().let { inv -> Mat4(DoubleArray(16) { i -> val c = i / 4; val r = i % 4; inv.m[r * 4 + c] }) }

        // Polygones -> triangles (éventail)
        val polys = ArrayList<IntArray>() // indices de polygon-vertex
        run {
            var cur = ArrayList<Int>()
            for (i in pvi.indices) { cur.add(i); if (pvi[i] < 0) { polys.add(cur.toIntArray()); cur = ArrayList() } }
        }
        fun cpOf(pv: Int) = pvi[pv].let { if (it < 0) -it - 1 else it }

        fun layer(name: String, dataName: String, indexName: String, comps: Int): ((Int, Int, Int) -> DoubleArray?)? {
            val l = geo.child(name) ?: return null
            val data = l.child(dataName)?.props?.firstOrNull()?.asDoubles() ?: return null
            val mapping = l.child("MappingInformationType")?.props?.firstOrNull() as? String ?: "ByPolygonVertex"
            val ref = l.child("ReferenceInformationType")?.props?.firstOrNull() as? String ?: "Direct"
            val idx = l.child(indexName)?.props?.firstOrNull()?.asInts()
            return { pv, cp, poly ->
                var k = when (mapping) { "ByPolygonVertex" -> pv; "ByPolygon" -> poly; "AllSame" -> 0; else -> cp }
                if (ref != "Direct" && idx != null) k = idx.getOrElse(k) { 0 }
                if (k >= 0 && (k + 1) * comps <= data.size) DoubleArray(comps) { data[k * comps + it] } else null
            }
        }
        val normalL = layer("LayerElementNormal", "Normals", "NormalsIndex", 3) ?: layer("LayerElementNormal", "Normals", "NormalIndex", 3)
        val uvL = layer("LayerElementUV", "UV", "UVIndex", 2)
        val matLayer = geo.child("LayerElementMaterial")
        val matData = matLayer?.child("Materials")?.props?.firstOrNull()?.asInts()
        val matAllSame = (matLayer?.child("MappingInformationType")?.props?.firstOrNull() as? String) == "AllSame"

        // Normales lissées de secours
        val smooth = if (normalL == null) {
            val acc = Array(cpCount) { Vec3.ZERO }
            for (p in polys) { if (p.size < 3) continue
                val a = cps[cpOf(p[0])]; for (k in 1 until p.size - 1) { val n = (cps[cpOf(p[k])] - a) cross (cps[cpOf(p[k + 1])] - a); for (pv in intArrayOf(p[0], p[k], p[k + 1])) acc[cpOf(pv)] = acc[cpOf(pv)] + n } }
            acc.map { it.normalized() }
        } else null

        // Skin
        val skin = childrenOf(gid, "Deformer").map { byId[it.child]!! }.firstOrNull { it.props.getOrNull(2) == "Skin" }
        var skinData: SkinData? = null
        val cpWeights = Array(cpCount) { ArrayList<Pair<Int, Double>>(4) }
        if (skin != null) {
            val skinId = (skin.props[0] as Number).toLong()
            val joints = ArrayList<Long>(); val ibms = ArrayList<Mat4>()
            for (cc in childrenOf(skinId, "Deformer")) {
                val cl = byId[cc.child]!!
                if (cl.props.getOrNull(2) != "Cluster") continue
                val bone = childrenOf(cc.child, "Model").firstOrNull()?.child ?: continue
                if (!models.containsKey(bone)) continue
                val idx = cl.child("Indexes")?.props?.firstOrNull()?.asInts() ?: IntArray(0)
                val w = cl.child("Weights")?.props?.firstOrNull()?.asDoubles() ?: DoubleArray(0)
                // Dans le fichier, « Transform » = inverse(TransformLink) × matrice du maillage au moment de la liaison :
                // c'est exactement la matrice inverse de liaison glTF.
                val tr = cl.child("Transform")?.props?.firstOrNull()?.asDoubles()?.let { Mat4(it.copyOf(16)) }
                val ibm = tr ?: global(bone).inverse()
                val j = joints.size
                joints.add(bone); ibms.add(ibm)
                for (k in idx.indices) if (idx[k] in 0 until cpCount && k < w.size && w[k] > 1e-5) cpWeights[idx[k]].add(j to w[k])
            }
            if (joints.isNotEmpty()) skinData = SkinData(joints, ibms)
        }

        // Blendshapes
        val morphNames = ArrayList<String>(); val morphDeltas = ArrayList<Array<Vec3>>()
        childrenOf(gid, "Deformer").map { byId[it.child]!! }.filter { it.props.getOrNull(2) == "BlendShape" }.forEach { bs ->
            for (chc in childrenOf((bs.props[0] as Number).toLong(), "Deformer")) {
                val ch = byId[chc.child]!!
                if (ch.props.getOrNull(2) != "BlendShapeChannel") continue
                val shape = childrenOf(chc.child, "Geometry").firstOrNull()?.let { byId[it.child] } ?: continue
                val idx = shape.child("Indexes")?.props?.firstOrNull()?.asInts() ?: continue
                val v = shape.child("Vertices")?.props?.firstOrNull()?.asDoubles() ?: continue
                val d = Array(cpCount) { Vec3.ZERO }
                for (k in idx.indices) if (idx[k] in 0 until cpCount && k * 3 + 2 < v.size) d[idx[k]] = geomM.transformDir(Vec3(v[k * 3], v[k * 3 + 1], v[k * 3 + 2]))
                val nm = ((ch.props.getOrNull(1) as? String) ?: "shape").substringBefore('\u0000').substringAfter("::").substringAfterLast('.')
                morphNames.add(nm); morphDeltas.add(d)
            }
        }

        // Assemblage par matériau
        val groups = HashMap<Int, MutableList<Int>>() // matériau -> liste de polygones
        polys.forEachIndexed { pi, p -> if (p.size >= 3) groups.getOrPut(if (matData == null) 0 else if (matAllSame) matData.getOrElse(0) { 0 } else matData.getOrElse(pi) { 0 }) { ArrayList() }.add(pi) }
        val prims = ArrayList<Prim>()
        for ((mat, plist) in groups.toSortedMap()) {
            val pos = ArrayList<Float>(); val nrm = ArrayList<Float>(); val uv = if (uvL != null) ArrayList<Float>() else null
            val jnt = if (skinData != null) ArrayList<Int>() else null; val wts = if (skinData != null) ArrayList<Float>() else null
            val cpList = ArrayList<Int>()
            val indices = ArrayList<Int>()
            val dedup = HashMap<String, Int>()
            fun vertex(pv: Int, poly: Int): Int {
                val cp = cpOf(pv)
                val n = normalL?.invoke(pv, cp, poly)?.let { normalM.transformDir(Vec3(it[0], it[1], it[2])).normalized() } ?: smooth!![cp]
                val t = uvL?.invoke(pv, cp, poly)
                val key = "$cp|${"%.4f".format(n.x)},${"%.4f".format(n.y)},${"%.4f".format(n.z)}|${t?.let { "%.5f,%.5f".format(it[0], it[1]) }}"
                return dedup.getOrPut(key) {
                    val p = cps[cp]
                    pos.add(p.x.toFloat()); pos.add(p.y.toFloat()); pos.add(p.z.toFloat())
                    nrm.add(n.x.toFloat()); nrm.add(n.y.toFloat()); nrm.add(n.z.toFloat())
                    if (uv != null) { uv.add((t?.get(0) ?: 0.0).toFloat()); uv.add((1.0 - (t?.get(1) ?: 0.0)).toFloat()) }
                    if (jnt != null && wts != null) {
                        val top4 = cpWeights[cp].sortedByDescending { it.second }.take(4)
                        val sum = top4.sumOf { it.second }.takeIf { it > 1e-9 } ?: 1.0
                        for (k in 0 until 4) { val e = top4.getOrNull(k); jnt.add(e?.first ?: 0); wts.add(((e?.second ?: 0.0) / sum).toFloat()) }
                        if (top4.isEmpty()) wts[wts.size - 4] = 1f
                    }
                    cpList.add(cp)
                    cpList.size - 1
                }
            }
            for (pi in plist) {
                val p = polys[pi]
                val v0 = vertex(p[0], pi)
                for (k in 1 until p.size - 1) { indices.add(v0); indices.add(vertex(p[k], pi)); indices.add(vertex(p[k + 1], pi)) }
            }
            val morphs = morphDeltas.map { d -> FloatArray(cpList.size * 3).also { o -> cpList.forEachIndexed { i, cp -> o[i * 3] = d[cp].x.toFloat(); o[i * 3 + 1] = d[cp].y.toFloat(); o[i * 3 + 2] = d[cp].z.toFloat() } } }
            prims.add(Prim(pos.toFloatArray(), nrm.toFloatArray(), uv?.toFloatArray(), jnt?.toIntArray(), wts?.toFloatArray(), indices.toIntArray(), mat, morphs))
        }
        return Triple(prims, skinData, morphNames)
    }

    // ---------- Transformations FBX

    private fun vec(n: FbxNode, prop: String, def: Vec3): Vec3 = n.prop70(prop)?.let { v ->
        val d = v.mapNotNull { (it as? Number)?.toDouble() }
        if (d.size >= 3) Vec3(d[0], d[1], d[2]) else def
    } ?: def

    private fun eulerToQuat(e: Vec3, order: Int): Quat {
        val qx = Quat.axisAngle(Vec3.X, Math.toRadians(e.x)); val qy = Quat.axisAngle(Vec3.Y, Math.toRadians(e.y)); val qz = Quat.axisAngle(Vec3.Z, Math.toRadians(e.z))
        // eEulerXYZ=0, XZY=1, YZX=2, YXZ=3, ZXY=4, ZYX=5 (première lettre appliquée en premier)
        return when (order) {
            1 -> qy * qz * qx; 2 -> qx * qz * qy; 3 -> qz * qx * qy; 4 -> qy * qx * qz; 5 -> qx * qy * qz
            else -> qz * qy * qx
        }.normalized()
    }

    private fun localMatrix(n: FbxNode): Mat4 {
        val t = vec(n, "Lcl Translation", Vec3.ZERO)
        val r = vec(n, "Lcl Rotation", Vec3.ZERO)
        val s = vec(n, "Lcl Scaling", Vec3.ONE)
        val order = n.prop70("RotationOrder")?.firstOrNull()?.let { (it as Number).toInt() } ?: 0
        val active = n.prop70("RotationActive")?.firstOrNull()?.let { (it as Number).toInt() != 0 } ?: true
        val pre = if (active) vec(n, "PreRotation", Vec3.ZERO) else Vec3.ZERO
        val post = if (active) vec(n, "PostRotation", Vec3.ZERO) else Vec3.ZERO
        val roff = vec(n, "RotationOffset", Vec3.ZERO); val rp = vec(n, "RotationPivot", Vec3.ZERO)
        val soff = vec(n, "ScalingOffset", Vec3.ZERO); val sp = vec(n, "ScalingPivot", Vec3.ZERO)
        fun T(v: Vec3) = Mat4.fromTrs(v, Quat.IDENTITY, Vec3.ONE)
        fun R(q: Quat) = Mat4.fromTrs(Vec3.ZERO, q, Vec3.ONE)
        val rq = eulerToQuat(r, order)
        val preQ = eulerToQuat(pre, 0); val postQ = eulerToQuat(post, 0)
        return T(t) * T(roff) * T(rp) * R(preQ) * R(rq) * R(postQ.conjugate()) * T(-rp) * T(soff) * T(sp) * Mat4.fromTrs(Vec3.ZERO, Quat.IDENTITY, s) * T(-sp)
    }

    private fun geometricMatrix(n: FbxNode): Mat4 {
        val t = vec(n, "GeometricTranslation", Vec3.ZERO); val r = vec(n, "GeometricRotation", Vec3.ZERO); val s = vec(n, "GeometricScaling", Vec3.ONE)
        return Mat4.fromTrs(t, eulerToQuat(r, 0), s)
    }

    /** Conversion vers Y-up, mètres, repère direct. */
    private fun axisConversion(gs: FbxNode?): Mat4 {
        fun ip(n: String, d: Int) = gs?.prop70(n)?.firstOrNull()?.let { (it as Number).toInt() } ?: d
        val unit = gs?.prop70("UnitScaleFactor")?.firstOrNull()?.let { (it as Number).toDouble() } ?: 1.0
        val up = ip("UpAxis", 1); val upS = ip("UpAxisSign", 1).let { if (it < 0) -1.0 else 1.0 }
        val front = ip("FrontAxis", 2); val frontS = ip("FrontAxisSign", 1).let { if (it < 0) -1.0 else 1.0 }
        val coord = ip("CoordAxis", 0); val coordS = ip("CoordAxisSign", 1).let { if (it < 0) -1.0 else 1.0 }
        fun axis(i: Int, s: Double) = when (i) { 0 -> Vec3.X * s; 1 -> Vec3.Y * s; else -> Vec3.Z * s }
        // Colonnes de la matrice inverse : où vont les axes glTF dans l'espace FBX.
        val ax = axis(coord, coordS); val ay = axis(up, upS); var az = axis(front, frontS)
        if (((ax cross ay) dot az) < 0) az = -az
        // M (FBX -> glTF) : lignes = axes ax, ay, az.
        val sc = unit / 100.0
        val m = DoubleArray(16)
        val rows = listOf(ax, ay, az)
        for (r in 0 until 3) { m[0 * 4 + r] = rows[r].x * sc; m[1 * 4 + r] = rows[r].y * sc; m[2 * 4 + r] = rows[r].z * sc }
        m[15] = 1.0
        if (abs(sc) < 1e-12) return Mat4()
        return Mat4(m)
    }
}
