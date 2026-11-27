package com.superessence.bodymotion.rig

import com.superessence.bodymotion.io.doubles
import com.superessence.bodymotion.io.gltf.GltfDocument
import com.superessence.bodymotion.io.int
import com.superessence.bodymotion.io.list
import com.superessence.bodymotion.io.obj
import com.superessence.bodymotion.io.str
import com.superessence.bodymotion.math.Mat4
import com.superessence.bodymotion.math.Quat
import com.superessence.bodymotion.math.Trs
import com.superessence.bodymotion.math.Vec3

/** Un nœud du graphe de scène du personnage (os ou non). */
data class SkeletonNode(
    val index: Int,
    val name: String,
    val parent: Int,
    val children: List<Int>,
    val rest: Trs,
    val isJoint: Boolean,
)

/**
 * Squelette extrait d'un document glTF : tous les nœuds de la scène (pour la cinématique directe exacte)
 * avec les os de skin marqués. Les poses de repos et les proportions viennent du modèle lui-même :
 * aucune taille ni structure standard n'est supposée.
 */
class Skeleton(val nodes: List<SkeletonNode>, val morphTargets: List<MorphInfo>) {
    /** Ordre topologique (parents avant enfants). */
    val order: List<Int> by lazy {
        val out = ArrayList<Int>(nodes.size)
        val roots = nodes.filter { it.parent < 0 }.map { it.index }
        val stack = ArrayDeque(roots.reversed())
        while (stack.isNotEmpty()) {
            val n = stack.removeLast(); out.add(n)
            nodes[n].children.reversed().forEach { stack.addLast(it) }
        }
        out
    }

    val restGlobal: List<Mat4> = computeGlobals { nodes[it].rest.toMat4() }
    val restGlobalRot: List<Quat> = computeGlobalRotations { nodes[it].rest.r }
    val jointCount get() = nodes.count { it.isJoint }
    val hasSkin get() = jointCount > 0

    fun indexOf(name: String) = nodes.indexOfFirst { it.name == name }
    fun restPos(i: Int) = restGlobal[i].translation()

    fun computeGlobals(local: (Int) -> Mat4): List<Mat4> {
        val g = arrayOfNulls<Mat4>(nodes.size)
        for (i in order) {
            val p = nodes[i].parent
            g[i] = if (p < 0) local(i) else g[p]!! * local(i)
        }
        return g.map { it ?: Mat4() }
    }

    fun computeGlobalRotations(local: (Int) -> Quat): List<Quat> {
        val g = arrayOfNulls<Quat>(nodes.size)
        for (i in order) {
            val p = nodes[i].parent
            g[i] = if (p < 0) local(i) else (g[p]!! * local(i)).normalized()
        }
        return g.map { it ?: Quat.IDENTITY }
    }

    /** Taille approximative du personnage (hauteur de la boîte englobante des articulations au repos). */
    fun restHeight(): Double {
        val ys = nodes.filter { it.isJoint }.map { restPos(it.index).y }
        return if (ys.isEmpty()) 1.0 else (ys.max() - ys.min()).coerceAtLeast(1e-3)
    }

    companion object {
        fun fromGltf(doc: GltfDocument): Skeleton {
            val raw = doc.nodes.map { it.obj() }
            val parent = IntArray(raw.size) { -1 }
            raw.forEachIndexed { i, n -> n.list("children").forEach { c -> parent[(c as Number).toInt()] = i } }
            val joints = HashSet<Int>()
            doc.skins.forEach { s -> s.obj().list("joints").forEach { joints.add((it as Number).toInt()) } }
            val nodes = raw.mapIndexed { i, n ->
                val rest = n.doubles("matrix")?.let { Mat4.fromColumnMajor(it).decompose() } ?: Trs(
                    n.doubles("translation")?.let { Vec3(it[0], it[1], it[2]) } ?: Vec3.ZERO,
                    n.doubles("rotation")?.let { Quat(it[0], it[1], it[2], it[3]).normalized() } ?: Quat.IDENTITY,
                    n.doubles("scale")?.let { Vec3(it[0], it[1], it[2]) } ?: Vec3.ONE,
                )
                SkeletonNode(
                    index = i,
                    name = n.str("name") ?: "node_$i",
                    parent = parent[i],
                    children = n.list("children").map { (it as Number).toInt() },
                    rest = rest,
                    isJoint = i in joints,
                )
            }
            val morphs = ArrayList<MorphInfo>()
            raw.forEachIndexed { i, n ->
                val meshIdx = n.int("mesh")
                if (meshIdx >= 0) {
                    val mesh = doc.meshes[meshIdx].obj()
                    val names = ((mesh["extras"] as? Map<*, *>)?.get("targetNames") as? List<*>)?.map { it.toString() }
                    val prims = mesh.list("primitives")
                    val count = (prims.firstOrNull()?.obj()?.get("targets") as? List<*>)?.size ?: 0
                    if (count > 0) morphs.add(MorphInfo(i, mesh.str("name") ?: "mesh_$meshIdx", List(count) { k -> names?.getOrNull(k) ?: "morph_$k" }))
                }
            }
            return Skeleton(nodes, morphs)
        }
    }
}

/** Cibles de déformation (blendshapes) d'un nœud de maillage, utilisées par la capture faciale. */
data class MorphInfo(val nodeIndex: Int, val meshName: String, val targetNames: List<String>)
