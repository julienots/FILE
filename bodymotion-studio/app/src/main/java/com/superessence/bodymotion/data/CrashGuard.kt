package com.superessence.bodymotion.data

import android.app.ActivityManager
import android.app.ApplicationExitInfo
import android.content.Context
import android.os.Build
import java.io.File
import java.io.PrintWriter
import java.io.StringWriter
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

/**
 * Diagnostic des plantages, sans réseau :
 * - exception Kotlin/Java : la trace complète est enregistrée ;
 * - plantage natif (MediaPipe, Filament, caméra) : Android le signale au lancement suivant (API 30+),
 *   et la dernière étape enregistrée (« fil d'Ariane ») indique où il s'est produit.
 * Le rapport est affiché au prochain lancement et peut être copié ou partagé.
 */
object CrashGuard {
    private lateinit var dir: File
    @Volatile private var lastStep = ""
    private var version = ""

    fun install(ctx: Context) {
        dir = File(ctx.filesDir, "diagnostic").apply { mkdirs() }
        version = runCatching { ctx.packageManager.getPackageInfo(ctx.packageName, 0).versionName }.getOrNull() ?: ""
        val previous = Thread.getDefaultUncaughtExceptionHandler()
        Thread.setDefaultUncaughtExceptionHandler { t, e ->
            runCatching {
                val sw = StringWriter(); e.printStackTrace(PrintWriter(sw))
                File(dir, "crash.txt").writeText(header(ctx) + "Thread : ${t.name}\nÉtape : ${readStep()}\n\n$sw")
            }
            previous?.uncaughtException(t, e)
        }
        // Plantage natif lors de la session précédente ?
        if (Build.VERSION.SDK_INT >= 30 && !File(dir, "crash.txt").exists()) runCatching {
            val am = ctx.getSystemService(ActivityManager::class.java)
            val last = am.getHistoricalProcessExitReasons(ctx.packageName, 0, 1).firstOrNull()
            val seenFile = File(dir, "seen_exit")
            val seen = seenFile.takeIf { it.exists() }?.readText()?.toLongOrNull() ?: 0L
            if (last != null && last.timestamp > seen) {
                seenFile.writeText(last.timestamp.toString())
                val reason = when (last.reason) {
                    ApplicationExitInfo.REASON_CRASH_NATIVE -> "Plantage natif"
                    ApplicationExitInfo.REASON_CRASH -> "Plantage"
                    ApplicationExitInfo.REASON_ANR -> "Application bloquée (ANR)"
                    ApplicationExitInfo.REASON_LOW_MEMORY -> "Mémoire insuffisante"
                    ApplicationExitInfo.REASON_EXCESSIVE_RESOURCE_USAGE -> "Ressources excessives"
                    else -> null
                }
                if (reason != null) {
                    val trace = if (Build.VERSION.SDK_INT >= 31 && last.reason == ApplicationExitInfo.REASON_CRASH_NATIVE)
                        runCatching { last.traceInputStream?.use { tombstoneSummary(it.readBytes()) } }.getOrNull() else null
                    File(dir, "crash.txt").writeText(header(ctx) +
                        "Type : $reason\nDescription : ${last.description ?: "—"}\nÉtape : ${readStep().ifEmpty { "(non enregistrée : version antérieure à 1.0.1)" }}\nDate : ${fmt(last.timestamp)}\n" +
                        (trace?.let { "\nPile d'appels native :\n$it\n" } ?: ""))
                }
            }
        }
    }

    private fun header(ctx: Context) = "BodyMotion Studio ${runCatching { ctx.packageManager.getPackageInfo(ctx.packageName, 0).versionName }.getOrNull()}\n" +
        "Appareil : ${Build.MANUFACTURER} ${Build.MODEL} · Android ${Build.VERSION.RELEASE} (API ${Build.VERSION.SDK_INT}) · ${Build.SUPPORTED_ABIS.firstOrNull()}\n"

    private fun fmt(t: Long) = SimpleDateFormat("yyyy-MM-dd HH:mm:ss", Locale.FRANCE).format(Date(t))

    /** Enregistre l'étape en cours (appelé avant les opérations natives sensibles). */
    fun step(s: String) {
        if (s == lastStep) return
        lastStep = s
        runCatching { File(dir, "step.txt").writeText("$s · v$version") }
    }

    /**
     * Le tombstone Android (format protobuf) contient la bibliothèque, la fonction et le message d'arrêt
     * sous forme de chaînes : on extrait les plus utiles, dans l'ordre (cause puis pile d'appels).
     */
    private fun tombstoneSummary(b: ByteArray): String {
        val out = ArrayList<String>(); val cur = StringBuilder()
        fun flush() { if (cur.length >= 5) out.add(cur.toString()); cur.setLength(0) }
        for (x in b) { val c = x.toInt() and 0xff; if (c in 0x20..0x7e) cur.append(c.toChar()) else flush() }
        flush()
        val keep = Regex("""\.so|SIG[A-Z]+|abort|Abort|Check failed|assert|fault|mediapipe|filament|gltfio|tflite|camera|egl|EGL|vulkan|Exception|error""")
        return out.filter { keep.containsMatchIn(it) && !it.startsWith("/system/framework") }
            .distinct().take(45).joinToString("\n") { it.take(200) }
    }

    fun readStep(): String = runCatching { File(dir, "step.txt").readText() }.getOrDefault("")

    /** Rapport en attente (puis effacé quand l'utilisateur l'a vu). */
    fun pendingReport(): String? = runCatching { File(dir, "crash.txt").takeIf { it.exists() }?.readText() }.getOrNull()
    fun clearReport() { runCatching { File(dir, "crash.txt").delete() } }

    /** Vrai si la session précédente a planté pendant l'analyse en mode GPU : on repasse alors en CPU. */
    fun lastCrashWasGpu(report: String?) = report != null && report.contains("GPU")
}
