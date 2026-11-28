package com.superessence.voxea.data

import java.io.File

/** Un dossier par projet : project.json, rec/NN.wav (voix brute), out/ (MP3 générés). */
class ProjectStore(private val root: File) {
    init { root.mkdirs() }

    fun dir(id: String) = File(root, id)
    fun recFile(p: Project, index: Int) = File(dir(p.id), "rec/%03d.wav".format(index))
    fun outDir(p: Project) = File(dir(p.id), "out")
    fun hasRecording(p: Project, index: Int) = recFile(p, index).let { it.exists() && it.length() > 44 }
    fun recordedCount(p: Project) = p.lines.count { hasRecording(p, it.index) }

    fun list(): List<Project> = (root.listFiles() ?: emptyArray())
        .mapNotNull { d -> runCatching { Project.fromJson(File(d, "project.json").readText()) }.getOrNull() }
        .sortedByDescending { it.createdAt }

    fun save(p: Project) {
        val d = dir(p.id); d.mkdirs()
        val tmp = File(d, "project.json.tmp")
        tmp.writeText(p.toJson())
        tmp.renameTo(File(d, "project.json"))
    }

    /**
     * Après modification du script : chaque prise suit sa réplique (même personnage + même texte),
     * les prises sans réplique correspondante sont supprimées.
     */
    fun remapRecordings(old: Project, new: Project) {
        val d = dir(old.id)
        val staging = File(d, "rec_remap").apply { deleteRecursively(); mkdirs() }
        for (l in old.lines) recFile(old, l.index).takeIf { it.exists() }?.renameTo(File(staging, "%03d.wav".format(l.index)))
        val used = HashSet<Int>()
        for (nl in new.lines) {
            val match = old.lines.firstOrNull { it.index !in used && it.character == nl.character && it.text == nl.text } ?: continue
            used += match.index
            val src = File(staging, "%03d.wav".format(match.index))
            if (src.exists()) { recFile(new, nl.index).parentFile?.mkdirs(); src.renameTo(recFile(new, nl.index)) }
        }
        staging.deleteRecursively()
    }

    fun delete(p: Project) { dir(p.id).deleteRecursively() }
}
