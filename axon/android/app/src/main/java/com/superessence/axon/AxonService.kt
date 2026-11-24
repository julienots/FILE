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
        if (event?.eventType != AccessibilityEvent.TYPE_WINDOW_STATE_CHANGED) return
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
        val req = Request.Builder().url("$base/device?key=${prefs.key}&model=$model").build()
        ws = http.newWebSocket(req, object : WebSocketListener() {
            override fun onOpen(webSocket: WebSocket, response: Response) {
                retry = 0
                connected = true
                emitState()
                send(JSONObject().put("type", "state").put("info", JSONObject()
                    .put("model", "${Build.MANUFACTURER} ${Build.MODEL}").put("app", currentApp).put("paused", prefs.paused)))
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
        streaming.set(false)
        emitState()
        if (stopped) return
        val delay = min(15_000L, 1000L * (1 shl min(retry++, 4)))
        main.postDelayed({ if (ws === socket) { ws = null; connect() } }, delay)
    }

    private fun send(o: JSONObject) { ws?.send(o.toString()) }

    private fun emitUi(o: JSONObject) { uiListener?.let { l -> main.post { l(o.toString()) } } }
    private fun emitState() = emitUi(JSONObject().put("type", "connection").put("connected", connected).put("viewers", viewers))

    private fun handleMessage(text: String) {
        val m = try { JSONObject(text) } catch (e: Exception) { return }
        when (m.optString("type")) {
            "call" -> worker.execute { runCall(m) }
            "manual" -> worker.execute { runManual(m) }
            "stream" -> {
                viewers = m.optInt("viewers")
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
            res.put("content", tool(tool, args))
        } catch (e: Exception) {
            res.put("isError", true).put("content", JSONArray().put(textPart("Erreur : ${e.message ?: e.javaClass.simpleName}")))
        }
        send(res)
    }

    private fun textPart(t: String) = JSONObject().put("type", "text").put("text", t)
    private fun ok(t: String = "OK") = JSONArray().put(textPart(t))

    private fun tool(name: String, a: JSONObject): JSONArray = when (name) {
        "screenshot" -> {
            if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) throw ToolError("La capture d'écran nécessite Android 11 ou plus. Utilisez get_ui_elements.")
            val bmp = capture() ?: throw ToolError("Capture impossible (écran éteint ou contenu protégé).")
            val (data, w, h) = encode(bmp, 1080, 70)
            JSONArray()
                .put(JSONObject().put("type", "image").put("mimeType", "image/jpeg").put("data", data))
                .put(textPart("Écran ${w}x${h} px. App au premier plan : ${currentApp.ifBlank { "?" }}."))
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
        "launch_app" -> {
            val q = a.getString("app").trim()
            val apps = launcherApps()
            val pkg = apps.firstOrNull { it.second.equals(q, true) }?.second
                ?: apps.firstOrNull { it.first.equals(q, true) }?.second
                ?: apps.firstOrNull { it.first.lowercase().contains(q.lowercase()) }?.second
                ?: throw ToolError("Application \"$q\" introuvable. Utilisez list_apps.")
            val intent = packageManager.getLaunchIntentForPackage(pkg) ?: throw ToolError("$pkg ne peut pas être ouverte.")
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_RESET_TASK_IF_NEEDED)
            main.post { startActivity(intent) }
            Thread.sleep(900)
            activity("app", "Ouverture de ${appLabel(pkg)}")
            ok("${appLabel(pkg)} ($pkg) ouverte. Faites un screenshot pour vérifier.")
        }
        "open_url" -> {
            val intent = Intent(Intent.ACTION_VIEW, Uri.parse(a.getString("url"))).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
            if (intent.resolveActivity(packageManager) == null) throw ToolError("Aucune app ne sait ouvrir ce lien.")
            main.post { startActivity(intent) }
            Thread.sleep(900)
            activity("app", "Ouverture de ${a.getString("url")}")
            ok()
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
        return head + els.joinToString("\n") { e ->
            val flags = listOfNotNull(if (e.clickable) "cliquable" else null, if (e.editable) "champ" else null,
                if (e.scrollable) "défilant" else null, if (e.checked) "coché" else null).joinToString(",")
            val label = listOfNotNull(e.text.takeIf { it.isNotEmpty() }?.let { "\"$it\"" },
                e.desc.takeIf { it.isNotEmpty() }?.let { "desc=\"$it\"" }, e.id.takeIf { it.isNotEmpty() }?.let { "#$it" }).joinToString(" ")
            "[${e.index}] ${e.type} $label ${if (flags.isNotEmpty()) "($flags)" else ""} @${e.bounds.centerX()},${e.bounds.centerY()}"
        }
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
