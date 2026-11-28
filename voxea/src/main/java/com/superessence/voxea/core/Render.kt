package com.superessence.voxea.core

import com.superessence.voxea.data.Project
import com.superessence.voxea.data.ProjectStore
import java.io.File
import java.util.zip.ZipEntry
import java.util.zip.ZipOutputStream
import kotlin.math.abs
import kotlin.math.max
import kotlin.math.min

object Audio {
    /** Retire les silences de début et de fin (seuil relatif à la crête), garde 150 ms de marge. */
    fun trimSilence(x: FloatArray, sr: Int): FloatArray {
        if (x.isEmpty()) return x
        val win = max(1, sr / 100)
        var peak = 0f
        for (v in x) peak = max(peak, abs(v))
        if (peak < 0.01f) return x
        val th = max(0.006f, peak * 0.04f)
        fun loud(start: Int): Boolean {
            var m = 0f
            for (i in start until min(x.size, start + win)) m = max(m, abs(x[i]))
            return m > th
        }
        var a = 0
        while (a < x.size && !loud(a)) a += win
        var b = x.size - win
        while (b > a && !loud(b)) b -= win
        val margin = sr * 15 / 100
        val s = max(0, a - margin); val e = min(x.size, b + win + margin)
        if (e - s <= 0) return x
        val y = x.copyOfRange(s, e)
        // fondu de 10 ms aux extrémités
        val f = min(y.size / 2, sr / 100)
        for (i in 0 until f) { val g = i / f.toFloat(); y[i] *= g; y[y.size - 1 - i] *= g }
        return y
    }

    fun resample(x: FloatArray, from: Int, to: Int): FloatArray {
        if (from == to || x.isEmpty()) return x
        val n = (x.size.toLong() * to / from).toInt()
        return FloatArray(n) { i ->
            val p = i.toDouble() * from / to; val k = p.toInt()
            if (k + 1 >= x.size) x[x.size - 1] else (x[k] + (x[k + 1] - x[k]) * (p - k)).toFloat()
        }
    }
}

/** Transforme une réplique enregistrée ; renvoie un signal à [sr] Hz. */
fun interface Converter {
    fun convert(line: Line, pcm: FloatArray, sr: Int, type: VoiceType, settings: VoiceSettings): FloatArray
}

object LocalConverter : Converter {
    override fun convert(line: Line, pcm: FloatArray, sr: Int, type: VoiceType, settings: VoiceSettings) =
        VoiceDsp.process(pcm, sr, settings)
}

class Renderer(private val store: ProjectStore) {
    data class Result(val files: List<File>, val skipped: List<Line>, val full: File?, val errors: List<String> = emptyList())

    /**
     * Génère un MP3 par réplique enregistrée (les autres sont ignorées et listées), plus le dialogue complet.
     * [onProgress] reçoit (fraction, texte).
     */
    fun render(
        p: Project,
        converter: Converter,
        gapMs: Int = 450,
        onProgress: (Float, String) -> Unit = { _, _ -> },
    ): Result {
        val out = store.outDir(p)
        out.deleteRecursively(); out.mkdirs()
        val files = mutableListOf<File>(); val skipped = mutableListOf<Line>(); val errors = mutableListOf<String>()
        val full = ArrayList<FloatArray>()
        val fullSr = 44100
        val total = p.lines.size.coerceAtLeast(1)
        for ((n, line) in p.lines.withIndex()) {
            val rec = store.recFile(p, line.index)
            if (!store.hasRecording(p, line.index)) { skipped += line; continue }
            onProgress(n.toFloat() / total, "Réplique ${line.index + 1} · ${line.character}")
            try {
                val (raw, sr) = Wav.read(rec)
                val voiced = converter.convert(line, raw, sr, p.voiceTypeFor(line), p.settingsFor(line))
                val f = File(out, fileNameFor(line))
                f.outputStream().buffered().use { Mp3.encode(voiced, sr, it) }
                files += f
                full += Audio.resample(voiced, sr, fullSr)
                full += FloatArray(fullSr * gapMs / 1000)
            } catch (e: Exception) {
                errors += "Réplique ${line.index + 1} : ${e.message ?: e.javaClass.simpleName}"
            }
        }
        var fullFile: File? = null
        if (files.isNotEmpty()) {
            onProgress(0.97f, "Dialogue complet")
            val all = FloatArray(full.sumOf { it.size })
            var k = 0
            for (a in full) { a.copyInto(all, k); k += a.size }
            fullFile = File(out, "00_DIALOGUE_COMPLET.mp3")
            fullFile.outputStream().buffered().use { Mp3.encode(all, fullSr, it) }
        }
        onProgress(1f, "Terminé")
        return Result(files, skipped, fullFile, errors)
    }

    fun zip(p: Project, r: Result, dest: File): File {
        dest.parentFile?.mkdirs()
        ZipOutputStream(dest.outputStream().buffered()).use { z ->
            for (f in listOfNotNull(r.full) + r.files) {
                z.putNextEntry(ZipEntry(f.name)); f.inputStream().use { it.copyTo(z) }; z.closeEntry()
            }
        }
        return dest
    }
}
