package com.superessence.axon

import android.annotation.SuppressLint
import android.app.Activity
import android.content.ClipData
import android.content.ClipboardManager
import android.content.ComponentName
import android.content.Intent
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.provider.Settings
import android.view.View
import android.view.WindowInsets
import android.webkit.JavascriptInterface
import android.webkit.WebChromeClient
import android.webkit.WebResourceRequest
import android.webkit.WebView
import android.webkit.WebViewClient
import android.widget.Toast
import org.json.JSONObject

/** Interface AXON : écran de chargement SuperEssence Studio + tableau de bord (WebView locale). */
class MainActivity : Activity() {

    private lateinit var web: WebView
    private lateinit var prefs: Prefs

    @SuppressLint("SetJavaScriptEnabled")
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        prefs = Prefs(this)
        web = WebView(this)
        web.setBackgroundColor(0xFF000000.toInt())
        web.settings.apply {
            javaScriptEnabled = true
            domStorageEnabled = true
            mediaPlaybackRequiresUserGesture = false
        }
        web.webChromeClient = WebChromeClient()
        web.webViewClient = object : WebViewClient() {
            override fun shouldOverrideUrlLoading(view: WebView, req: WebResourceRequest): Boolean {
                val url = req.url.toString()
                // Le relais (écran de contrôle) et les pages locales restent dans l'app ; le reste s'ouvre à l'extérieur.
                if (url.startsWith("file:///android_asset/") || (prefs.relay.isNotBlank() && url.startsWith(prefs.relay))) return false
                try { startActivity(Intent(Intent.ACTION_VIEW, Uri.parse(url))) } catch (_: Exception) {}
                return true
            }
        }
        web.addJavascriptInterface(Bridge(), "AxonNative")
        setContentView(web)
        applyInsets()
        handleLink(intent)
        web.loadUrl("file:///android_asset/www/index.html")
    }

    private fun applyInsets() {
        web.setOnApplyWindowInsetsListener { v, insets ->
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                val b = insets.getInsets(WindowInsets.Type.systemBars() or WindowInsets.Type.ime())
                v.setPadding(b.left, b.top, b.right, b.bottom)
            }
            insets
        }
    }

    override fun onNewIntent(intent: Intent) {
        super.onNewIntent(intent)
        handleLink(intent)
    }

    /** Lien de jumelage partagé (https://relais/#k=CLÉ) ouvert avec AXON. */
    private fun handleLink(i: Intent?) {
        val data = i?.getStringExtra(Intent.EXTRA_TEXT) ?: i?.dataString ?: return
        if (data.contains("#k=")) prefs.viewerLink = data.trim()
    }

    override fun onResume() {
        super.onResume()
        AxonService.uiListener = { json -> web.evaluateJavascript("window.axonEvent&&axonEvent($json)", null) }
        web.evaluateJavascript("window.axonEvent&&axonEvent({type:'resume'})", null)
    }

    override fun onPause() {
        AxonService.uiListener = null
        super.onPause()
    }

    @Deprecated("Deprecated in Java")
    override fun onBackPressed() {
        if (web.canGoBack()) web.goBack() else {
            @Suppress("DEPRECATION") super.onBackPressed()
        }
    }

    private fun serviceEnabled(): Boolean {
        val enabled = Settings.Secure.getString(contentResolver, Settings.Secure.ENABLED_ACCESSIBILITY_SERVICES) ?: return false
        val me = ComponentName(this, AxonService::class.java)
        return enabled.split(':').any { ComponentName.unflattenFromString(it) == me }
    }

    inner class Bridge {
        @JavascriptInterface
        fun state(): String = JSONObject()
            .put("role", prefs.role)
            .put("relay", prefs.relay)
            .put("key", prefs.key)
            .put("connector", prefs.connectorUrl)
            .put("viewerUrl", prefs.viewerUrl)
            .put("viewerLink", prefs.viewerLink)
            .put("paused", prefs.paused)
            .put("serviceEnabled", serviceEnabled())
            .put("serviceRunning", AxonService.instance != null)
            .put("connected", AxonService.connected)
            .put("viewers", AxonService.viewers)
            .put("devices", AxonService.devices)
            .put("deviceId", prefs.deviceId)
            .put("deviceName", prefs.deviceName)
            .put("sdk", Build.VERSION.SDK_INT)
            .put("xiaomi", Build.MANUFACTURER.lowercase().let { it.contains("xiaomi") || it.contains("redmi") || it.contains("poco") })
            .put("model", "${Build.MANUFACTURER} ${Build.MODEL}")
            .put("version", packageManager.getPackageInfo(packageName, 0).versionName)
            .toString()

        @JavascriptInterface
        fun setRole(role: String) { prefs.role = role }

        @JavascriptInterface
        fun setRelay(url: String) {
            prefs.relay = url
            runOnUiThread { AxonService.instance?.reconnect() }
        }

        @JavascriptInterface
        fun setViewerLink(link: String) { prefs.viewerLink = link.trim() }

        @JavascriptInterface
        fun setDeviceName(n: String) { prefs.deviceName = n; AxonService.instance?.setName(prefs.deviceName) }

        /** Rejoint le salon d'un autre appareil : lien https://relais/#k=CLÉ (ou clé seule). */
        @JavascriptInterface
        fun joinRoom(link: String): Boolean {
            val v = link.trim()
            val m = Regex("""(https://[^/#?\s]+)[^#\s]*#k=([A-Za-z0-9_-]{16,64})""").find(v)
            val k = m?.groupValues?.get(2) ?: Regex("""^[A-Za-z0-9_-]{16,64}$""").find(v)?.value ?: return false
            if (m != null) prefs.relay = m.groupValues[1]
            prefs.setKey(k)
            prefs.viewerLink = ""
            runOnUiThread { AxonService.instance?.reconnect() }
            return true
        }

        @JavascriptInterface
        fun newKey(): String {
            val k = prefs.newKey()
            runOnUiThread { AxonService.instance?.reconnect() }
            return k
        }

        @JavascriptInterface
        fun setPaused(p: Boolean) { prefs.paused = p; AxonService.instance?.setPaused(p) }

        @JavascriptInterface
        fun sendRequest(text: String) { AxonService.instance?.sendRequest(text) }

        /** Xiaomi/Redmi/POCO : écran « Autres autorisations » (ouverture en arrière-plan). Sinon : infos de l'app. */
        @JavascriptInterface
        fun openBackgroundStart() = runOnUiThread {
            val tries = listOf(
                Intent("miui.intent.action.APP_PERM_EDITOR").setClassName("com.miui.securitycenter", "com.miui.permcenter.permissions.PermissionsEditorActivity").putExtra("extra_pkgname", packageName),
                Intent("miui.intent.action.APP_PERM_EDITOR").setClassName("com.miui.securitycenter", "com.miui.permcenter.permissions.AppPermissionsEditorActivity").putExtra("extra_pkgname", packageName),
                Intent("miui.intent.action.APP_PERM_EDITOR").putExtra("extra_pkgname", packageName),
                Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS, Uri.parse("package:$packageName")),
            )
            for (i in tries) {
                try { startActivity(i.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)); return@runOnUiThread } catch (_: Exception) {}
            }
        }

        @JavascriptInterface
        fun openAccessibility() = runOnUiThread {
            startActivity(Intent(Settings.ACTION_ACCESSIBILITY_SETTINGS).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK))
        }

        @JavascriptInterface
        fun copy(text: String) = runOnUiThread {
            (getSystemService(CLIPBOARD_SERVICE) as ClipboardManager).setPrimaryClip(ClipData.newPlainText("AXON", text))
            if (Build.VERSION.SDK_INT < 33) Toast.makeText(this@MainActivity, "Copié", Toast.LENGTH_SHORT).show()
        }

        @JavascriptInterface
        fun share(text: String) = runOnUiThread {
            val i = Intent(Intent.ACTION_SEND).setType("text/plain").putExtra(Intent.EXTRA_TEXT, text)
            startActivity(Intent.createChooser(i, "Partager"))
        }

        @JavascriptInterface
        fun openClaude() = runOnUiThread {
            val i = packageManager.getLaunchIntentForPackage("com.anthropic.claude")
                ?: Intent(Intent.ACTION_VIEW, Uri.parse("https://claude.ai/settings/connectors"))
            startActivity(i.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK))
        }

        @JavascriptInterface
        fun openUrl(url: String) = runOnUiThread { web.loadUrl(url) }
    }
}
