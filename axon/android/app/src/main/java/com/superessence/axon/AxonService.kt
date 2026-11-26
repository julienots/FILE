package com.superessence.axon

import android.accessibilityservice.AccessibilityService
import android.accessibilityservice.GestureDescription
import android.content.Intent
import android.content.pm.PackageManager
import android.graphics.Bitmap
import android.graphics.Path
import android.graphics.Rect
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.util.Base64
import android.util.DisplayMetrics
import android.view.Display
import android.view.WindowManager
import android.view.accessibility.AccessibilityEvent
import android.view.accessibility.AccessibilityNodeInfo
import android.widget.Toast
import com.superessence.axon.tools.WaitForTool
import kotlinx.coroutines.runBlocking
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.Response
import okhttp3.WebSocket
import okhttp3.WebSocketListener
import org.json.JSONArray
import org.json.JSONObject
import java.io.ByteArrayOutputStream
import java.net.URLEncoder
import java.util.concurrent.CountDownLatch
import java.util.concurrent.Executors
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicBoolean
import kotlin.math.max
import kotlin.math.min
import kotlin.math.roundToInt

/**
 * Service d'accessibilité AXON : relie le téléphone au relais et exécute
 * les actions demandées par Claude (capture, lecture d'écran, gestes, saisie…).
 */
class AxonService : AccessibilityService() {

    companion object {
        @Volatile var instance: AxonService? = null
        /** Écouteur de l'interface (MainActivity) : reçoit les événements en JSON. */
        @Volatile var uiListener: ((String) -> Unit)? = null
        @Volatile var connected = false
        @Volatile var viewers = 0
        /** Nombre d'appareils pilotés connectés au même salon (celui-ci compris). */
        @Volatile var devices = 0
    }

    private lateinit var prefs: Prefs
    private val main = Handler(Looper.getMainLooper())
    private val worker = Executors.newSingleThreadExecutor()
    private val streamer = Executors.newSingleThreadScheduledExecutor()
    private val http = OkHttpClient.Builder()
        .pingInterval(20, TimeUnit.SECONDS)
        .readTimeout(0, TimeUnit.MILLISECONDS)
        .build()
    private var ws: WebSocket? = null
    private var retry = 0
    private var stopped = false
    private val streaming = AtomicBoolean(false)
    private val captureLock = Any()
    private var lastCaptureAt = 0L
    private var currentApp = ""
    private var currentPkg = ""
    private var uiCache: List<El> = emptyList()
    @Volatile private var lastEventAt = 0L
    /** Dernière image de la retransmission (réutilisée par screenshot si elle est toute récente). */
    @Volatile private var lastFrame: Triple<String, Int, Int>? = null
    @Volatile private var lastFrameAt = 0L
    @Volatile private var lastActionAt = 0L

    data class El(
        val index: Int, val text: String, val desc: String, val id: String, val type: String,
        val clickable: Boolean, val editable: Boolean, val scrollable: Boolean, val checked: Boolean,
        val bounds: Rect, val node: AccessibilityNodeInfo,
    )

    /* ------------------------------------------------------------------ */
    /* Cycle de vie                                                        */
    /* ------------------------------------------------------------------ */
    override fun onServiceConnected() {
        super.onServiceConnected()
        prefs = Prefs(this)
        instance = this
        stopped = false
        connect()
        emitUi(JSONObject().put("type", "service").put("enabled", true))
    }

    override fun onUnbind(intent: Intent?): Boolean {
        shutdown()
        return super.onUnbind(intent)
    }

    override fun onDestroy() {
        shutdown()
        super.onDestroy()
    }

    private fun shutdown() {
        stopped = true
        instance = null
        connected = false
        streaming.set(false)
        ws?.close(1000, "arrêt")
        ws = null
        emitUi(JSONObject().put("type", "service").put("enabled", false))
    }

    override fun onInterrupt() {}

    override fun onAccessibilityEvent(event: AccessibilityEvent?) {
        if (event == null) return
        lastEventAt = System.currentTimeMillis()
        if (event.eventType != AccessibilityEvent.TYPE_WINDOW_STATE_CHANGED) return
        val pkg = event.packageName?.toString() ?: return
        if (pkg == currentPkg || pkg == "com.android.systemui") return
        currentPkg = pkg
        currentApp = appLabel(pkg)
        send(JSONObject().put("type", "state").put("info", JSONObject().put("app", currentApp).put("package", pkg)))
    }

    /* ------------------------------------------------------------------ */
    /* Connexion au relais                                                 */
    /* ------------------------------------------------------------------ */
    fun reconnect() {
        retry = 0
        ws?.close(1000, "reconnexion")
        ws = null
        connect()
    }

    private fun connect() {
        if (stopped) return
        val relay = prefs.relay
        if (relay.isBlank()) { emitState(); return }
        val base = relay.replaceFirst(Regex("^http"), "ws")
        val model = URLEncoder.encode("${Build.MANUFACTURER} ${Build.MODEL}".trim(), "UTF-8")
        val name = URLEncoder.encode(prefs.deviceName, "UTF-8")
        val req = Request.Builder().url("$base/device?key=${prefs.key}&id=${prefs.deviceId}&model=$model&name=$name").build()
        ws = http.newWebSocket(req, object : WebSocketListener() {
            override fun onOpen(webSocket: WebSocket, response: Response) {
                retry = 0
                connected = true
                emitState()
                send(JSONObject().put("type", "state").put("info", JSONObject()
                    .put("model", "${Build.MANUFACTURER} ${Build.MODEL}").put("name", prefs.deviceName).put("app", currentApp).put("paused", prefs.paused)))
            }
            override fun onMessage(webSocket: WebSocket, text: String) {
                if (webSocket !== ws) return
                handleMessage(text)
            }
            override fun onClosed(webSocket: WebSocket, code: Int, reason: String) = lost(webSocket)
            override fun onFailure(webSocket: WebSocket, t: Throwable, response: Response?) = lost(webSocket)
        })
    }

    private fun lost(socket: WebSocket) {
        if (socket !== ws) return
        connected = false
        devices = 0
        streaming.set(false)
        emitState()
        if (stopped) return
        val delay = min(15_000L, 1000L * (1 shl min(retry++, 4)))
        main.postDelayed({ if (ws === socket) { ws = null; connect() } }, delay)
    }

    private fun send(o: JSONObject) { ws?.send(o.toString()) }

    private fun emitUi(o: JSONObject) { uiListener?.let { l -> main.post { l(o.toString()) } } }
    private fun emitState() = emitUi(JSONObject().put("type", "connection").put("connected", connected).put("viewers", viewers).put("devices", devices))

    private fun handleMessage(text: String) {
        val m = try { JSONObject(text) } catch (e: Exception) { return }
        when (m.optString("type")) {
            "call" -> worker.execute { runCall(m) }
            "manual" -> worker.execute { runManual(m) }
            "stream" -> {
                viewers = m.optInt("viewers")
                devices = m.optInt("devices", 1)
                emitState()
                if (m.optBoolean("on")) startStream() else streaming.set(false)
            }
            "notify" -> {
                emitUi(m)
                main.post { Toast.makeText(this, "Claude : " + m.optString("message"), Toast.LENGTH_LONG).show() }
            }
            "activity", "request" -> emitUi(m)
        }
    }

    /** Envoie une demande tapée dans l'app AXON du téléphone. */
    fun sendRequest(text: String) = send(JSONObject().put("type", "request").put("text", text))

    fun setName(n: String) {
        send(JSONObject().put("type", "state").put("info", JSONObject().put("name", n.ifBlank { "${Build.MANUFACTURER} ${Build.MODEL}" })))
    }

    fun setPaused(p: Boolean) {
        prefs.paused = p
        send(JSONObject().put("type", "state").put("info", JSONObject().put("paused", p)))
    }

    /* ------------------------------------------------------------------ */
    /* Retransmission de l'écran                                           */
    /* ------------------------------------------------------------------ */
    private fun startStream() {
        if (!streaming.compareAndSet(false, true)) return
        streamer.execute(object : Runnable {
            override fun run() {
                if (!streaming.get() || ws == null) { streaming.set(false); return }
                val t0 = System.currentTimeMillis()
                try {
                    val bmp = capture()
                    if (bmp != null) {
                        val (data, w, h) = encode(bmp, 720, 55)
                        val t = System.currentTimeMillis()
                        if (t0 > lastActionAt) { lastFrame = Triple(data, w, h); lastFrameAt = t }
                        send(JSONObject().put("type", "frame").put("mime", "image/jpeg").put("w", w).put("h", h).put("data", data))
                    }
                } catch (_: Exception) {}
                val wait = max(150L, 650L - (System.currentTimeMillis() - t0))
                streamer.schedule(this, wait, TimeUnit.MILLISECONDS)
            }
        })
    }

    /** Capture d'écran (Android 11+). Respecte la limite d'environ 3 captures/s. */
    private fun capture(): Bitmap? {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) return null
        synchronized(captureLock) {
            repeat(3) {
                val since = System.currentTimeMillis() - lastCaptureAt
                if (since < 360) Thread.sleep(360 - since)
                var result: Bitmap? = null
                var failCode = -1
                val latch = CountDownLatch(1)
                takeScreenshot(Display.DEFAULT_DISPLAY, mainExecutor, object : TakeScreenshotCallback {
                    override fun onSuccess(s: ScreenshotResult) {
                        try {
                            val hb = s.hardwareBuffer
                            result = Bitmap.wrapHardwareBuffer(hb, s.colorSpace)?.copy(Bitmap.Config.ARGB_8888, false)
                            hb.close()
                        } catch (_: Exception) {}
                        latch.countDown()
                    }
                    override fun onFailure(errorCode: Int) { failCode = errorCode; latch.countDown() }
                })
                latch.await(5, TimeUnit.SECONDS)
                lastCaptureAt = System.currentTimeMillis()
                if (result != null) return result
                if (failCode != ERROR_TAKE_SCREENSHOT_INTERVAL_TIME_SHORT) return null
            }
            return null
        }
    }

    private fun encode(bmp: Bitmap, maxW: Int, quality: Int): Triple<String, Int, Int> {
        val w = bmp.width; val h = bmp.height
        val scaled = if (w > maxW) Bitmap.createScaledBitmap(bmp, maxW, (h * maxW.toFloat() / w).roundToInt(), true) else bmp
        val out = ByteArrayOutputStream()
        scaled.compress(Bitmap.CompressFormat.JPEG, quality, out)
        if (scaled !== bmp) scaled.recycle()
        bmp.recycle()
        return Triple(Base64.encodeToString(out.toByteArray(), Base64.NO_WRAP), w, h)
    }

    private fun screenSize(): Pair<Int, Int> {
        val wm = getSystemService(WINDOW_SERVICE) as WindowManager
        return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            val b = wm.currentWindowMetrics.bounds; Pair(b.width(), b.height())
        } else {
            val dm = DisplayMetrics()
            @Suppress("DEPRECATION") wm.defaultDisplay.getRealMetrics(dm)
            Pair(dm.widthPixels, dm.heightPixels)
        }
    }

    /* ------------------------------------------------------------------ */
    /* Outils appelés par Claude                                           */
    /* ------------------------------------------------------------------ */
    private class ToolError(msg: String) : Exception(msg)

    private fun runCall(m: JSONObject) {
        val id = m.optString("id")
        val tool = m.optString("tool")
        val args = m.optJSONObject("args") ?: JSONObject()
        val res = JSONObject().put("type", "result").put("id", id)
        try {
            if (prefs.paused) throw ToolError("AXON est en pause : l'utilisateur a suspendu le contrôle. Demandez-lui de le réactiver dans l'app AXON.")
            val content = tool(tool, args)
            if (tool in ACTIONS || tool == "do_actions") observe(content, args.optString("observe", "elements"))
            res.put("content", content)
        } catch (e: Exception) {
            res.put("isError", true).put("content", JSONArray().put(textPart("Erreur : ${e.message ?: e.javaClass.simpleName}")))
        }
        send(res)
    }

    private fun textPart(t: String) = JSONObject().put("type", "text").put("text", t)

    /* ------------------------------------------------------------------ */
    /* Rapidité : attente intelligente + écran renvoyé avec chaque action   */
    /* ------------------------------------------------------------------ */
    private val ACTIONS = setOf("tap", "tap_element", "swipe", "type_text", "press_key", "launch_app", "open_url", "search", "scroll_to")

    /** Attend que l'écran ne bouge plus (aucun événement depuis quietMs), au plus maxMs. */
    private fun settle(maxMs: Long = 1500, quietMs: Long = 220) {
        val start = System.currentTimeMillis()
        Thread.sleep(90)
        while (System.currentTimeMillis() - start < maxMs) {
            if (System.currentTimeMillis() - lastEventAt >= quietMs) return
            Thread.sleep(40)
        }
    }

    /** Ajoute l'état de l'écran après une action, pour éviter à Claude un appel de plus. */
    private fun observe(content: JSONArray, mode: String) {
        if (mode == "none") return
        settle()
        when (mode) {
            "screenshot" -> {
                try { val p = screenshotParts(); for (i in 0 until p.length()) content.put(p.get(i)) }
                catch (e: Exception) { content.put(textPart("(capture impossible : ${e.message})")) }
                content.put(textPart(formatElements(collect())))
            }
            else -> content.put(textPart("Écran maintenant :\n" + formatElements(collect())))
        }
    }

    private fun screenshotParts(): JSONArray {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) throw ToolError("La capture d'écran nécessite Android 11 ou plus. Utilisez get_ui_elements.")
        val (sw, sh) = screenSize()
        // Une image de la retransmission de moins de 0,5 s, prise après la dernière action, suffit : pas de nouvelle capture.
        val recent = lastFrame?.takeIf { System.currentTimeMillis() - lastFrameAt < 500 && lastFrameAt > lastActionAt }
        val (data, w, h) = recent ?: run {
            val bmp = capture() ?: throw ToolError("Capture impossible (écran éteint ou contenu protégé).")
            encode(bmp, 720, 60)
        }
        val iw = min(720, w); val ih = (h * iw.toFloat() / w).roundToInt()
        val f = sw.toFloat() / iw
        return JSONArray()
            .put(JSONObject().put("type", "image").put("mimeType", "image/jpeg").put("data", data))
            .put(textPart("Image ${iw}x${ih} ; écran réel ${sw}x${sh} px. Pour tap : x_réel = x_image × ${"%.3f".format(java.util.Locale.US, f)} (idem pour y). Préférez tap_element. App : ${currentApp.ifBlank { "?" }}."))
    }

    /** Plusieurs actions en un seul appel (beaucoup plus rapide que des appels séparés). */
    private fun doActions(a: JSONObject): JSONArray {
        val steps = a.optJSONArray("steps") ?: throw ToolError("Donnez steps : liste d'actions.")
        if (steps.length() > 20) throw ToolError("20 actions maximum par appel.")
        val log = StringBuilder()
        for (i in 0 until steps.length()) {
            val st = steps.getJSONObject(i)
            val t = st.optString("tool").ifBlank { st.optString("action") }
            try {
                when (t) {
                    "wait" -> Thread.sleep(min(5000L, st.optLong("ms", 500)))
                    WaitForTool.NAME -> {
                        val r = waitFor(st)
                        if (r.timedOut) throw ToolError("délai dépassé : ${r.texts} n'est pas ${if (st.optString("mode") == "gone") "parti" else "apparu"} après ${r.waitedMs} ms")
                        log.append("${i + 1}. wait_for ✓ ${r.matched ?: "disparu"} (${r.waitedMs} ms)\n")
                    }
                    in ACTIONS -> {
                        val r = tool(t, st)
                        log.append("${i + 1}. $t ✓ ${r.optJSONObject(0)?.optString("text")?.takeIf { it != "OK" }?.take(120) ?: ""}\n")
                        if (i < steps.length() - 1) settle(1200)
                    }
                    else -> throw ToolError("action inconnue « $t » (autorisées : ${ACTIONS.joinToString()}, wait, wait_for)")
                }
            } catch (e: Exception) {
                log.append("${i + 1}. $t ✗ ${e.message}\nArrêt : les actions suivantes n'ont pas été faites.")
                settle()
                throw ToolError(log.toString() + "\n\nÉcran maintenant :\n" + formatElements(collect()))
            }
        }
        return ok(log.toString().trimEnd())
    }
    private fun ok(t: String = "OK") = JSONArray().put(textPart(t))

    private class WaitResult(val matched: String?, val timedOut: Boolean, val waitedMs: Long, val texts: String, val screen: String)

    /** wait_for : attend sur le téléphone qu'un texte apparaisse/disparaisse (voir tools/WaitForTool.kt). */
    private fun waitFor(a: JSONObject): WaitResult {
        val r = runBlocking { WaitForTool.run(this@AxonService, a) { JSONArray().put(formatElements(collect())) } }
        return WaitResult(
            matched = if (r.isNull("matched")) null else r.getString("matched"),
            timedOut = r.getBoolean("timed_out"),
            waitedMs = r.getLong("waited_ms"),
            texts = a.optJSONArray("texts")?.toString() ?: "[]",
            screen = r.getJSONArray("elements").optString(0),
        )
    }

    private fun tool(name: String, a: JSONObject): JSONArray {
        if (name in ACTIONS) lastActionAt = System.currentTimeMillis()
        try { return toolImpl(name, a) } finally { if (name in ACTIONS) lastActionAt = System.currentTimeMillis() }
    }

    private fun toolImpl(name: String, a: JSONObject): JSONArray = when (name) {
        "do_actions" -> doActions(a)
        WaitForTool.NAME -> {
            val r = waitFor(a)
            ok("wait_for : matched=${r.matched ?: "null"}, timed_out=${r.timedOut}, waited_ms=${r.waitedMs}\n\nÉcran maintenant :\n${r.screen}")
        }
        "screenshot" -> {
            if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) throw ToolError("La capture d'écran nécessite Android 11 ou plus. Utilisez get_ui_elements.")
            screenshotParts()
        }
        "get_ui_elements" -> ok(formatElements(collect()))
        "tap_element" -> {
            val el = if (a.has("index")) {
                uiCache.getOrNull(a.getInt("index")) ?: throw ToolError("Index inconnu : relancez get_ui_elements.")
            } else {
                val q = a.optString("text").lowercase().ifBlank { throw ToolError("Donnez index ou text.") }
                val els = collect()
                els.firstOrNull { it.text.lowercase() == q || it.desc.lowercase() == q }
                    ?: els.firstOrNull { it.text.lowercase().contains(q) || it.desc.lowercase().contains(q) }
                    ?: throw ToolError("Aucun élément contenant \"${a.optString("text")}\".")
            }
            clickElement(el)
            ok("Touché [${el.index}] ${el.text.ifBlank { el.desc.ifBlank { el.id } }}")
        }
        "tap" -> {
            val x = a.getDouble("x").toFloat(); val y = a.getDouble("y").toFloat()
            gesture(x, y, x, y, if (a.optBoolean("long")) 800 else 60)
            showTap(x, y)
            ok()
        }
        "swipe" -> {
            val (w, h) = screenSize()
            val d = a.optString("direction")
            val p = when (d) {
                "up" -> floatArrayOf(w / 2f, h * .75f, w / 2f, h * .25f)
                "down" -> floatArrayOf(w / 2f, h * .25f, w / 2f, h * .75f)
                "left" -> floatArrayOf(w * .85f, h / 2f, w * .15f, h / 2f)
                "right" -> floatArrayOf(w * .15f, h / 2f, w * .85f, h / 2f)
                else -> {
                    if (!a.has("x1") || !a.has("y2")) throw ToolError("Donnez direction ou x1,y1,x2,y2.")
                    floatArrayOf(a.getDouble("x1").toFloat(), a.getDouble("y1").toFloat(), a.getDouble("x2").toFloat(), a.getDouble("y2").toFloat())
                }
            }
            gesture(p[0], p[1], p[2], p[3], a.optLong("duration_ms", 350))
            ok()
        }
        "type_text" -> {
            val node = focusedInput() ?: throw ToolError("Aucun champ de saisie actif : touchez d'abord le champ.")
            val add = a.getString("text")
            val current = if (node.isShowingHintText) "" else node.text?.toString() ?: ""
            val value = if (a.optBoolean("replace")) add else current + add
            val b = Bundle().apply { putCharSequence(AccessibilityNodeInfo.ACTION_ARGUMENT_SET_TEXT_CHARSEQUENCE, value) }
            if (!node.performAction(AccessibilityNodeInfo.ACTION_SET_TEXT, b)) throw ToolError("Le champ refuse la saisie.")
            if (a.optBoolean("submit")) { Thread.sleep(150); pressEnter() }
            ok()
        }
        "press_key" -> {
            when (val k = a.getString("key")) {
                "back" -> performGlobalAction(GLOBAL_ACTION_BACK)
                "home" -> performGlobalAction(GLOBAL_ACTION_HOME)
                "recents" -> performGlobalAction(GLOBAL_ACTION_RECENTS)
                "notifications" -> performGlobalAction(GLOBAL_ACTION_NOTIFICATIONS)
                "quick_settings" -> performGlobalAction(GLOBAL_ACTION_QUICK_SETTINGS)
                "lock_screen" -> if (Build.VERSION.SDK_INT >= 28) performGlobalAction(GLOBAL_ACTION_LOCK_SCREEN) else throw ToolError("Android 9 minimum.")
                "enter" -> pressEnter()
                else -> throw ToolError("Touche inconnue : $k")
            }
            ok()
        }
        "list_apps" -> {
            val q = a.optString("filter").lowercase()
            val apps = launcherApps().filter { q.isBlank() || it.first.lowercase().contains(q) || it.second.lowercase().contains(q) }
            ok(apps.joinToString("\n") { "${it.first} — ${it.second}" }.ifBlank { "Aucune application trouvée." })
        }
        "search" -> search(a.getString("app").trim(), a.getString("query"))
        "scroll_to" -> scrollTo(a)
        "launch_app" -> {
            val q = a.getString("app").trim()
            val pkg = findPkg(q)
            val intent = packageManager.getLaunchIntentForPackage(pkg) ?: throw ToolError("$pkg ne peut pas être ouverte.")
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_RESET_TASK_IF_NEEDED)
            val label = appLabel(pkg)
            activity("app", "Ouverture de $label")
            val how = launchVerified(pkg, label, intent)
            ok("$label ($pkg) est au premier plan$how. Faites un screenshot pour voir l'écran.")
        }
        "open_url" -> {
            val intent = Intent(Intent.ACTION_VIEW, Uri.parse(a.getString("url"))).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
            val target = intent.resolveActivity(packageManager) ?: throw ToolError("Aucune app ne sait ouvrir ce lien.")
            activity("app", "Ouverture de ${a.getString("url")}")
            // Si le lien ouvre un sélecteur d'apps (« Ouvrir avec »), on ne peut pas connaître l'app finale : on vérifie juste un changement.
            val pkg = target.packageName.takeUnless { it == "android" || it.contains("resolver") || it.contains("intentresolver") }
            val before = foregroundPkg()
            val opened = startAndWait(intent) { fg -> if (pkg != null) fg == pkg else fg != before && fg.isNotEmpty() }
                || run { goHome(); startAndWait(intent) { fg -> if (pkg != null) fg == pkg else fg != launcherPkg() && fg.isNotEmpty() } }
            if (!opened) throw ToolError(blockedMessage("le lien"))
            ok("Lien ouvert${pkg?.let { " dans ${appLabel(it)}" } ?: ""}. Faites un screenshot pour vérifier.")
        }
        else -> throw ToolError("Outil inconnu : $name")
    }

    private fun activity(kind: String, text: String, extra: JSONObject.() -> Unit = {}) {
        val o = JSONObject().put("type", "activity").put("kind", kind).put("text", text).put("at", System.currentTimeMillis())
        o.extra()
        send(o)
        emitUi(o)
    }

    private fun showTap(x: Float, y: Float) {
        val (w, h) = screenSize()
        activity("tap", "Touche ${x.roundToInt()},${y.roundToInt()}") { put("nx", x / w); put("ny", y / h) }
    }

    /* --- Actions manuelles depuis un écran de contrôle --- */
    private fun runManual(m: JSONObject) {
        try {
            val (w, h) = screenSize()
            when (m.optString("action")) {
                "tap" -> { val x = (m.getDouble("nx") * w).toFloat(); val y = (m.getDouble("ny") * h).toFloat(); gesture(x, y, x, y, 60) }
                "longpress" -> { val x = (m.getDouble("nx") * w).toFloat(); val y = (m.getDouble("ny") * h).toFloat(); gesture(x, y, x, y, 750) }
                "swipe" -> gesture((m.getDouble("nx1") * w).toFloat(), (m.getDouble("ny1") * h).toFloat(),
                    (m.getDouble("nx2") * w).toFloat(), (m.getDouble("ny2") * h).toFloat(), m.optLong("ms", 300))
                "key" -> tool("press_key", JSONObject().put("key", m.optString("key")))
                "text" -> tool("type_text", JSONObject().put("text", m.optString("text")))
            }
        } catch (_: Exception) {}
    }

    /* --- Gestes --- */
    private fun gesture(x1: Float, y1: Float, x2: Float, y2: Float, ms: Long) {
        val path = Path().apply { moveTo(max(0f, x1), max(0f, y1)); lineTo(max(0f, x2), max(0f, y2)) }
        val g = GestureDescription.Builder().addStroke(GestureDescription.StrokeDescription(path, 0, max(1L, min(ms, 5000L)))).build()
        val latch = CountDownLatch(1)
        var done = false
        main.post {
            val sent = dispatchGesture(g, object : GestureResultCallback() {
                override fun onCompleted(d: GestureDescription?) { done = true; latch.countDown() }
                override fun onCancelled(d: GestureDescription?) { latch.countDown() }
            }, null)
            if (!sent) latch.countDown()
        }
        latch.await(ms + 3000, TimeUnit.MILLISECONDS)
        if (!done) throw ToolError("Le geste a été annulé par le système.")
    }

    private fun clickElement(el: El) {
        var n: AccessibilityNodeInfo? = el.node
        var clicked = false
        try {
            while (n != null && !clicked) {
                if (n.isClickable) clicked = n.performAction(AccessibilityNodeInfo.ACTION_CLICK)
                n = if (clicked) null else n.parent
            }
        } catch (_: Exception) {}
        if (!clicked) gesture(el.bounds.exactCenterX(), el.bounds.exactCenterY(), el.bounds.exactCenterX(), el.bounds.exactCenterY(), 60)
        showTap(el.bounds.exactCenterX(), el.bounds.exactCenterY())
    }

    private fun focusedInput(): AccessibilityNodeInfo? =
        rootInActiveWindow?.findFocus(AccessibilityNodeInfo.FOCUS_INPUT)
            ?: windows.firstNotNullOfOrNull { it.root?.findFocus(AccessibilityNodeInfo.FOCUS_INPUT) }

    private fun pressEnter() {
        val node = focusedInput()
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R && node != null &&
            node.performAction(AccessibilityNodeInfo.AccessibilityAction.ACTION_IME_ENTER.id)) return
        throw ToolError("Impossible de valider (Android 11 minimum, ou pas de champ actif). Touchez le bouton de recherche/envoi.")
    }

    /* --- Lecture de l'écran --- */
    private fun collect(): List<El> {
        val out = ArrayList<El>()
        val roots = try { windows.mapNotNull { it.root } } catch (_: Exception) { emptyList() }
        val list = roots.ifEmpty { listOfNotNull(rootInActiveWindow) }
        for (root in list) walk(root, out, 0)
        uiCache = out
        return out
    }

    private fun walk(n: AccessibilityNodeInfo, out: ArrayList<El>, depth: Int) {
        if (depth > 60 || out.size > 400) return
        if (n.isVisibleToUser) {
            val text = n.text?.toString()?.trim().orEmpty()
            val desc = n.contentDescription?.toString()?.trim().orEmpty()
            val editable = n.isEditable
            if (text.isNotEmpty() || desc.isNotEmpty() || n.isClickable || editable || n.isScrollable || n.isCheckable) {
                val r = Rect(); n.getBoundsInScreen(r)
                if (r.width() > 3 && r.height() > 3) {
                    out.add(El(out.size, text.take(120), desc.take(120),
                        n.viewIdResourceName?.substringAfter(":id/").orEmpty(),
                        n.className?.toString()?.substringAfterLast('.').orEmpty(),
                        n.isClickable || n.isLongClickable, editable, n.isScrollable, n.isChecked, r, n))
                }
            }
        }
        for (i in 0 until n.childCount) n.getChild(i)?.let { walk(it, out, depth + 1) }
    }

    private fun formatElements(els: List<El>): String {
        if (els.isEmpty()) return "(aucun élément lisible — l'app dessine peut-être elle-même son interface : utilisez screenshot et tap x/y)"
        val head = "App : ${currentApp.ifBlank { "?" }}\n"
        // Format compact (moins de texte = Claude répond plus vite) : [n] "texte" desc #id {c=cliquable,s=champ,d=défilant,v=coché} @x,y
        val lines = ArrayList<String>()
        for (e in els) {
            val hasLabel = e.text.isNotEmpty() || e.desc.isNotEmpty()
            // Conteneurs sans texte ni rôle utile : bruit, on les saute.
            if (!hasLabel && !e.editable && !e.scrollable && !(e.clickable && e.id.isNotEmpty())) continue
            val flags = buildString { if (e.clickable) append('c'); if (e.editable) append('s'); if (e.scrollable) append('d'); if (e.checked) append('v') }
            val label = buildString {
                if (e.text.isNotEmpty()) append("\"").append(e.text.take(80)).append("\"")
                if (e.desc.isNotEmpty() && e.desc != e.text) append(" desc=\"").append(e.desc.take(60)).append("\"")
                if (!hasLabel && e.id.isNotEmpty()) append("#").append(e.id)
            }
            lines.add("[${e.index}] $label${if (flags.isNotEmpty()) " {$flags}" else ""} @${e.bounds.centerX()},${e.bounds.centerY()}")
            if (lines.size >= 150) { lines.add("… (liste coupée)"); break }
        }
        return head + "c=cliquable s=champ de saisie d=défilant v=coché\n" + lines.joinToString("\n")
    }

    private fun findPkg(q: String): String {
        val apps = launcherApps()
        return apps.firstOrNull { it.second.equals(q, true) }?.second
            ?: apps.firstOrNull { it.first.equals(q, true) }?.second
            ?: apps.firstOrNull { it.first.lowercase().contains(q.lowercase()) }?.second
            ?: throw ToolError("Application \"$q\" introuvable. Utilisez list_apps.")
    }

    /* --- Tâches complètes en un appel --- */
    /** Liens directs : la recherche s'ouvre instantanément, sans toucher l'interface. */
    private fun searchLink(app: String, q: String): Pair<String, String?>? {
        val e = Uri.encode(q); val a = app.lowercase()
        return when {
            a.contains("youtube") -> "https://www.youtube.com/results?search_query=$e" to "com.google.android.youtube"
            a.contains("maps") -> "geo:0,0?q=$e" to "com.google.android.apps.maps"
            a.contains("play") && a.contains("store") -> "market://search?q=$e" to "com.android.vending"
            a.contains("spotify") -> "spotify:search:$e" to "com.spotify.music"
            a == "google" || a.contains("chrome") || a == "web" || a == "internet" -> "https://www.google.com/search?q=$e" to null
            else -> null
        }
    }

    private fun search(app: String, query: String): JSONArray {
        if (query.isBlank()) throw ToolError("query vide.")
        searchLink(app, query)?.let { (url, pkg) ->
            val i = Intent(Intent.ACTION_VIEW, Uri.parse(url)).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
            val usable = pkg?.let { p -> Intent(i).setPackage(p).takeIf { it.resolveActivity(packageManager) != null } } ?: i.takeIf { it.resolveActivity(packageManager) != null }
            if (usable != null) {
                activity("app", "Recherche « $query » dans $app")
                val target = usable.`package` ?: usable.resolveActivity(packageManager)?.packageName
                val ok = startAndWait(usable) { fg -> target == null || fg == target } || run { goHome(); startAndWait(usable) { fg -> target == null || fg == target } }
                if (ok) return ok("Résultats de « $query » ouverts dans ${target?.let { appLabel(it) } ?: app} (lien direct).")
            }
        }
        // Sinon : ouvrir l'app, trouver la recherche, écrire, valider.
        val pkg = findPkg(app)
        val intent = packageManager.getLaunchIntentForPackage(pkg)?.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_RESET_TASK_IF_NEEDED)
            ?: throw ToolError("$pkg ne peut pas être ouverte.")
        activity("app", "Recherche « $query » dans ${appLabel(pkg)}")
        launchVerified(pkg, appLabel(pkg), intent)
        settle(2000)
        var field = searchField()
        if (field == null) {
            val words = listOf("search", "recherch", "rechercher", "chercher", "loupe")
            val btn = collect().firstOrNull { e -> e.clickable && words.any { w -> e.desc.lowercase().contains(w) || e.text.lowercase().contains(w) || e.id.lowercase().contains(w) } }
                ?: throw ToolError("Barre de recherche introuvable dans ${appLabel(pkg)}. Utilisez get_ui_elements puis tap_element.")
            clickElement(btn)
            settle(1500)
            field = searchField() ?: throw ToolError("Le champ de recherche ne s'est pas ouvert. Utilisez get_ui_elements.")
        }
        field.performAction(AccessibilityNodeInfo.ACTION_FOCUS)
        val b = Bundle().apply { putCharSequence(AccessibilityNodeInfo.ACTION_ARGUMENT_SET_TEXT_CHARSEQUENCE, query) }
        if (!field.performAction(AccessibilityNodeInfo.ACTION_SET_TEXT, b)) throw ToolError("Le champ refuse la saisie.")
        Thread.sleep(150)
        try { pressEnter() } catch (_: Exception) {
            // Pas de touche Entrée : on touche la première suggestion.
            settle(1000)
            collect().firstOrNull { it.clickable && it.text.lowercase().contains(query.lowercase().take(20)) }?.let { clickElement(it) }
        }
        return ok("Recherche « $query » lancée dans ${appLabel(pkg)}.")
    }

    private fun searchField(): AccessibilityNodeInfo? =
        focusedInput() ?: collect().firstOrNull { it.editable }?.node

    /** Fait défiler jusqu'à voir un texte, et le touche si tap=true. */
    private fun scrollTo(a: JSONObject): JSONArray {
        val q = a.getString("text").lowercase().trim()
        val dir = a.optString("direction", "down")
        val (w, h) = screenSize()
        fun find() = collect().firstOrNull { it.text.lowercase().contains(q) || it.desc.lowercase().contains(q) }
        var el = find()
        var n = 0
        while (el == null && n < a.optInt("max_scrolls", 10)) {
            when (dir) {
                "up" -> gesture(w / 2f, h * .3f, w / 2f, h * .75f, 280)
                "left" -> gesture(w * .2f, h / 2f, w * .85f, h / 2f, 280)
                "right" -> gesture(w * .85f, h / 2f, w * .2f, h / 2f, 280)
                else -> gesture(w / 2f, h * .75f, w / 2f, h * .3f, 280)
            }
            n++
            settle(900, 180)
            el = find()
        }
        if (el == null) throw ToolError("« ${a.getString("text")} » introuvable après $n défilements.")
        if (a.optBoolean("tap", true)) { clickElement(el); return ok("Trouvé après $n défilement(s) et touché : ${el.text.ifBlank { el.desc }}") }
        return ok("Trouvé après $n défilement(s) : [${el.index}] ${el.text.ifBlank { el.desc }}")
    }

    /* --- Ouverture d'apps vérifiée --- */
    /** Package réellement au premier plan (fenêtre active), sinon le dernier vu par les événements. */
    private fun foregroundPkg(): String = try {
        rootInActiveWindow?.packageName?.toString()
            ?: windows.firstOrNull { it.isActive }?.root?.packageName?.toString()
    } catch (_: Exception) { null } ?: currentPkg

    private fun launcherPkg(): String = packageManager.resolveActivity(
        Intent(Intent.ACTION_MAIN).addCategory(Intent.CATEGORY_HOME), PackageManager.MATCH_DEFAULT_ONLY)?.activityInfo?.packageName ?: ""

    private fun waitFor(ms: Long, ok: (String) -> Boolean): Boolean {
        val end = System.currentTimeMillis() + ms
        while (System.currentTimeMillis() < end) {
            if (ok(foregroundPkg())) return true
            Thread.sleep(150)
        }
        return ok(foregroundPkg())
    }

    private fun startAndWait(intent: Intent, ms: Long = 2500, ok: (String) -> Boolean): Boolean {
        main.post { try { startActivity(intent) } catch (_: Exception) {} }
        return waitFor(ms, ok)
    }

    private fun goHome() {
        performGlobalAction(GLOBAL_ACTION_HOME)
        val home = launcherPkg()
        waitFor(1500) { it == home }
        Thread.sleep(400)
    }

    private val isXiaomi get() = Build.MANUFACTURER.lowercase().let { it.contains("xiaomi") || it.contains("redmi") || it.contains("poco") }

    private fun blockedMessage(what: String) =
        "Le téléphone a bloqué l'ouverture de $what (AXON n'a pas le droit d'ouvrir une app depuis l'arrière-plan). " +
            (if (isXiaomi) "Sur Xiaomi/Redmi/POCO : Paramètres › Applications › Gérer les applications › AXON CONNECT › Autres autorisations › activez « Afficher des fenêtres pop-up lors de l'exécution en arrière-plan » (et « Démarrage automatique »). Le bouton « Autoriser » de l'app AXON CONNECT y mène directement. "
             else "Vérifiez les autorisations d'AXON CONNECT (ouverture en arrière-plan / superposition). ") +
            "En attendant, essayez : press_key home, puis tap_element sur l'icône de l'app."

    /**
     * Ouvre une app et vérifie qu'elle est vraiment au premier plan.
     * 1) lancement direct ; 2) depuis l'écran d'accueil ; 3) en touchant son icône (accueil puis tiroir d'apps).
     * Renvoie une précision sur la méthode utilisée, ou lève ToolError si tout a échoué.
     */
    private fun launchVerified(pkg: String, label: String, intent: Intent): String {
        if (foregroundPkg() == pkg) return ""
        if (startAndWait(intent) { it == pkg }) return ""
        goHome()
        if (startAndWait(intent) { it == pkg }) return ""
        if (tapIcon(pkg, label)) return " (ouverte en touchant son icône : le lancement direct est bloqué par le téléphone)"
        throw ToolError(blockedMessage(label))
    }

    private fun findIcon(label: String): El? {
        val q = label.lowercase().trim()
        val els = collect()
        return els.firstOrNull { it.text.lowercase().trim() == q || it.desc.lowercase().trim() == q }
            ?: els.firstOrNull { it.desc.lowercase().startsWith(q) || it.text.lowercase().startsWith(q) }
    }

    private fun tapIcon(pkg: String, label: String): Boolean {
        val (w, h) = screenSize()
        fun tryTap(): Boolean {
            val el = findIcon(label) ?: return false
            clickElement(el)
            return waitFor(3000) { it == pkg }
        }
        // Pages de l'écran d'accueil
        goHome()
        if (tryTap()) return true
        repeat(5) {
            try { gesture(w * .85f, h * .5f, w * .15f, h * .5f, 250) } catch (_: Exception) {}
            Thread.sleep(500)
            if (tryTap()) return true
        }
        // Tiroir d'applications (glisser vers le haut), puis défilement
        goHome()
        try { gesture(w / 2f, h * .85f, w / 2f, h * .25f, 300) } catch (_: Exception) {}
        Thread.sleep(700)
        if (tryTap()) return true
        repeat(6) {
            try { gesture(w / 2f, h * .75f, w / 2f, h * .3f, 350) } catch (_: Exception) {}
            Thread.sleep(500)
            if (tryTap()) return true
        }
        goHome()
        return false
    }

    /* --- Applications --- */
    private fun launcherApps(): List<Pair<String, String>> {
        val i = Intent(Intent.ACTION_MAIN).addCategory(Intent.CATEGORY_LAUNCHER)
        return packageManager.queryIntentActivities(i, 0)
            .map { Pair(it.loadLabel(packageManager).toString(), it.activityInfo.packageName) }
            .distinctBy { it.second }.sortedBy { it.first.lowercase() }
    }

    private fun appLabel(pkg: String): String = try {
        packageManager.getApplicationLabel(packageManager.getApplicationInfo(pkg, 0)).toString()
    } catch (_: PackageManager.NameNotFoundException) { pkg }
}
