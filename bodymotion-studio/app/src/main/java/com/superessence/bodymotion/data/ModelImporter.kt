package com.superessence.bodymotion.data

import android.content.Context
import android.net.Uri
import android.provider.OpenableColumns
import com.superessence.bodymotion.io.fbx.FbxBinary
import com.superessence.bodymotion.io.fbx.FbxImporter
import com.superessence.bodymotion.io.gltf.GltfDocument
import java.util.zip.ZipInputStream

/** Importation depuis le stockage Android : .glb, .gltf, .vrm, .fbx, ou .zip (glTF/FBX + textures). */
object ModelImporter {

    class Result(val doc: GltfDocument, val format: String, val fileName: String, val warnings: List<String>)

    fun displayName(ctx: Context, uri: Uri): String {
        ctx.contentResolver.query(uri, arrayOf(OpenableColumns.DISPLAY_NAME), null, null, null)?.use { c ->
            if (c.moveToFirst()) c.getString(0)?.let { return it }
        }
        return uri.lastPathSegment?.substringAfterLast('/') ?: "modele"
    }

    fun import(ctx: Context, uri: Uri): Result {
        val name = displayName(ctx, uri)
        val bytes = ctx.contentResolver.openInputStream(uri)?.use { it.readBytes() } ?: throw IllegalArgumentException("Impossible de lire le fichier.")
        return importBytes(name, bytes)
    }

    fun importBytes(name: String, bytes: ByteArray, resolve: (String) -> ByteArray? = { null }): Result {
        val lower = name.lowercase()
        val warnings = ArrayList<String>()
        return when {
            lower.endsWith(".zip") || (bytes.size > 4 && bytes[0] == 'P'.code.toByte() && bytes[1] == 'K'.code.toByte()) -> {
                val entries = HashMap<String, ByteArray>()
                ZipInputStream(bytes.inputStream()).use { z ->
                    var e = z.nextEntry
                    while (e != null) { if (!e.isDirectory) entries[e.name.replace('\\', '/')] = z.readBytes(); e = z.nextEntry }
                }
                val model = entries.keys.firstOrNull { it.lowercase().endsWith(".glb") || it.lowercase().endsWith(".vrm") }
                    ?: entries.keys.firstOrNull { it.lowercase().endsWith(".gltf") }
                    ?: entries.keys.firstOrNull { it.lowercase().endsWith(".fbx") }
                    ?: throw IllegalArgumentException("Aucun modèle (.glb, .gltf, .vrm, .fbx) dans l'archive.")
                val base = model.substringBeforeLast('/', "")
                val byName = entries.mapKeys { it.key.substringAfterLast('/').lowercase() }
                importBytes(model.substringAfterLast('/'), entries[model]!!) { rel ->
                    entries[if (base.isEmpty()) rel else "$base/$rel"] ?: entries[rel] ?: byName[rel.substringAfterLast('/').lowercase()]
                }
            }
            lower.endsWith(".fbx") || FbxBinary.isBinary(bytes) -> {
                if (!FbxBinary.isBinary(bytes)) throw IllegalArgumentException("Ce FBX est au format texte (ASCII). Réexportez-le en FBX binaire (option par défaut de Blender, Maya et Mixamo).")
                Result(FbxImporter(resolve).convert(bytes), "FBX", name, warnings)
            }
            GltfDocument.isGlb(bytes) -> {
                val doc = GltfDocument.parseGlb(bytes)
                Result(doc, if (doc.isVrm() || lower.endsWith(".vrm")) "VRM" else "GLB", name, warnings)
            }
            lower.endsWith(".gltf") || bytes.firstOrNull() == '{'.code.toByte() -> {
                val doc = GltfDocument.parseGltf(String(bytes, Charsets.UTF_8), resolve)
                Result(doc, "glTF", name, warnings)
            }
            else -> throw IllegalArgumentException("Format non reconnu. Formats pris en charge : FBX (binaire), GLB, glTF, VRM, ou un .zip.")
        }.also { r ->
            val req = (r.doc.json["extensionsRequired"] as? List<*>)?.map { it.toString() } ?: emptyList()
            val unsupported = req.filter { it in setOf("KHR_draco_mesh_compression", "EXT_meshopt_compression", "KHR_texture_basisu") }
            if (unsupported.isNotEmpty()) throw IllegalArgumentException("Compression non prise en charge : ${unsupported.joinToString()}. Réexportez le modèle sans compression Draco/Meshopt/Basis.")
        }
    }
}
