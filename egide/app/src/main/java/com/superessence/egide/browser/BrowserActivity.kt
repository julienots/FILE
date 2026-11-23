package com.superessence.egide.browser

import android.annotation.SuppressLint
import android.content.Context
import android.content.Intent
import android.graphics.Bitmap
import android.net.Uri
import android.os.Bundle
import android.os.Message
import android.view.View
import android.view.ViewGroup
import android.view.inputmethod.EditorInfo
import android.webkit.CookieManager
import android.webkit.JavascriptInterface
import android.webkit.WebChromeClient
import android.webkit.WebResourceRequest
import android.webkit.WebResourceResponse
import android.webkit.WebSettings
import android.webkit.WebView
import android.webkit.WebViewClient
import android.widget.FrameLayout
import android.widget.LinearLayout
import android.widget.Toast
import androidx.activity.ComponentActivity
import androidx.activity.OnBackPressedCallback
import androidx.activity.enableEdgeToEdge
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.BasicTextField
import androidx.compose.foundation.text.KeyboardActions
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.filled.Close
import androidx.compose.material.icons.filled.Home
import androidx.compose.material.icons.filled.Refresh
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.Text
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.SolidColor
import androidx.compose.ui.platform.ComposeView
import androidx.compose.ui.platform.LocalFocusManager
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.input.ImeAction
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat
import androidx.webkit.WebViewCompat
import androidx.webkit.WebViewFeature
import com.superessence.egide.core.Filter
import com.superessence.egide.core.Origin
import com.superessence.egide.core.Prefs
import com.superessence.egide.core.Stats
import com.superessence.egide.ui.C
import com.superessence.egide.ui.EgideTheme
import com.superessence.egide.ui.EssenceLogo
import java.io.ByteArrayInputStream
import kotlin.concurrent.thread

/**
 * Navigateur protégé : chaque requête de la page (scripts, images, iframes, vidéos de pub)
 * est filtrée, les pop-ups et les redirections non demandées sont bloquées.
 * En mode YouTube, un script supprime les pubs du lecteur et saute celles qui passent.
 */
class BrowserActivity : ComponentActivity() {

    companion object {
        private const val EXTRA_YT = "yt"
        const val HOME = "https://duckduckgo.com/"
        const val YOUTUBE = "https://m.youtube.com/"

        fun intent(ctx: Context, youtube: Boolean) =
            Intent(ctx, BrowserActivity::class.java).putExtra(EXTRA_YT, youtube)

        private val YT_ORIGINS = setOf("https://m.youtube.com", "https://www.youtube.com", "https://youtube.com", "https://music.youtube.com")

        /** Chemins publicitaires propres à YouTube (les vidéos, elles, ne sont jamais touchées). */
        private val YT_AD_PATHS = listOf("/pagead/", "/api/stats/ads", "/ptracking", "/get_midroll_", "/ad_data_204", "/api/stats/qoe?adformat")
    }

    private lateinit var web: WebView
    private lateinit var fullscreen: FrameLayout
    private var customView: View? = null
    private var customCallback: WebChromeClient.CustomViewCallback? = null
    private var ytScript: String = ""
    private var docStartScript = false

    private var urlText by mutableStateOf("")
    private var loadProgress by mutableIntStateOf(0)
    private var blockedHere by mutableIntStateOf(0)
    private var canBack by mutableStateOf(false)

    @SuppressLint("SetJavaScriptEnabled")
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        if (!Filter.loaded.value) thread { Filter.ensureLoaded() }
        ytScript = assets.open("youtube.js").bufferedReader().use { it.readText() }

        web = WebView(this)
        fullscreen = FrameLayout(this).apply { visibility = View.GONE; setBackgroundColor(0xFF000000.toInt()) }
        setupWebView(web)

        val toolbar = ComposeView(this).apply { setContent { EgideTheme { Toolbar() } } }
        val column = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setBackgroundColor(0xFF07070D.toInt())
            addView(toolbar, LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT))
            addView(web, LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, 0, 1f))
        }
        val root = FrameLayout(this).apply {
            addView(column)
            addView(fullscreen)
            setOnApplyWindowInsetsListener { v, insets ->
                val nav = WindowInsetsCompat.toWindowInsetsCompat(insets).getInsets(WindowInsetsCompat.Type.navigationBars() or WindowInsetsCompat.Type.ime())
                column.setPadding(0, 0, 0, nav.bottom)
                insets
            }
        }
        setContentView(root)

        onBackPressedDispatcher.addCallback(this, object : OnBackPressedCallback(true) {
            override fun handleOnBackPressed() {
                when {
                    customView != null -> exitFullscreen()
                    web.canGoBack() -> web.goBack()
                    else -> finish()
                }
            }
        })

        val start = intent.data?.toString()
            ?: if (intent.getBooleanExtra(EXTRA_YT, false)) YOUTUBE else HOME
        if (savedInstanceState != null) web.restoreState(savedInstanceState) else web.loadUrl(start)
    }

    override fun onSaveInstanceState(outState: Bundle) {
        super.onSaveInstanceState(outState)
        web.saveState(outState)
    }

    override fun onDestroy() {
        web.destroy()
        super.onDestroy()
    }

    private fun isYouTube(url: String?): Boolean {
        val h = url?.let { Uri.parse(it).host } ?: return false
        return h == "youtube.com" || h.endsWith(".youtube.com")
    }

    @SuppressLint("SetJavaScriptEnabled", "JavascriptInterface")
    private fun setupWebView(w: WebView) {
        w.setBackgroundColor(0xFF07070D.toInt())
        with(w.settings) {
            javaScriptEnabled = true
            domStorageEnabled = true
            mediaPlaybackRequiresUserGesture = false
            setSupportMultipleWindows(true)
            javaScriptCanOpenWindowsAutomatically = false
            loadWithOverviewMode = true
            useWideViewPort = true
            builtInZoomControls = true
            displayZoomControls = false
            mixedContentMode = WebSettings.MIXED_CONTENT_NEVER_ALLOW
            // Se présenter comme Chrome mobile : YouTube sert alors son site complet.
            userAgentString = userAgentString.replace("; wv)", ")").replace(Regex("Version/\\d+\\.\\d+ "), "")
        }
        CookieManager.getInstance().setAcceptThirdPartyCookies(w, false)
        w.addJavascriptInterface(Bridge(), "EgideBridge")

        if (WebViewFeature.isFeatureSupported(WebViewFeature.DOCUMENT_START_SCRIPT)) {
            WebViewCompat.addDocumentStartJavaScript(w, ytScript, YT_ORIGINS)
            docStartScript = true
        }

        w.webViewClient = object : WebViewClient() {
            override fun shouldInterceptRequest(view: WebView, req: WebResourceRequest): WebResourceResponse? {
                val url = req.url
                val host = url.host ?: return null
                val blocked = Filter.isBlocked(host) ||
                    ((host.endsWith("youtube.com") || host.endsWith("google.com")) && YT_AD_PATHS.any { url.toString().contains(it) })
                if (!blocked) return null
                Stats.record(host, true, Origin.BROWSER)
                runOnUiThread { blockedHere++ }
                return WebResourceResponse("text/plain", "utf-8", ByteArrayInputStream(ByteArray(0)))
            }

            override fun shouldOverrideUrlLoading(view: WebView, req: WebResourceRequest): Boolean {
                val uri = req.url
                val scheme = uri.scheme?.lowercase()
                if (scheme == "http" || scheme == "https") {
                    val host = uri.host ?: return false
                    if (Filter.isBlocked(host)) {
                        Stats.redirectBlocked(host)
                        blockedHere++
                        toast("Redirection vers $host bloquée")
                        return true
                    }
                    return false
                }
                // Liens vers des applis (intent:, market:, tel:…) : seulement si vous avez touché le lien.
                if (!req.hasGesture() && Prefs.browserBlockPopups) {
                    Stats.redirectBlocked(uri.toString().take(60))
                    blockedHere++
                    toast("Ouverture automatique d'une appli bloquée")
                    return true
                }
                openExternal(uri.toString())
                return true
            }

            override fun onPageStarted(view: WebView, url: String?, favicon: Bitmap?) {
                blockedHere = 0
                urlText = url ?: ""
                if (!docStartScript && isYouTube(url)) view.evaluateJavascript(ytScript, null)
            }

            override fun onPageFinished(view: WebView, url: String?) {
                canBack = view.canGoBack()
                urlText = url ?: urlText
                if (!docStartScript && isYouTube(url)) view.evaluateJavascript(ytScript, null)
            }

            override fun doUpdateVisitedHistory(view: WebView, url: String?, isReload: Boolean) {
                canBack = view.canGoBack()
                urlText = url ?: urlText
            }
        }

        w.webChromeClient = object : WebChromeClient() {
            override fun onProgressChanged(view: WebView, newProgress: Int) { loadProgress = newProgress }

            override fun onCreateWindow(view: WebView, isDialog: Boolean, isUserGesture: Boolean, resultMsg: Message): Boolean {
                if (!isUserGesture && Prefs.browserBlockPopups) {
                    Stats.redirectBlocked("pop-up sur " + (Uri.parse(view.url ?: "").host ?: "?"))
                    blockedHere++
                    return false
                }
                // Lien « nouvel onglet » touché par l'utilisateur : on l'ouvre dans la même page.
                val catcher = WebView(this@BrowserActivity)
                catcher.webViewClient = object : WebViewClient() {
                    override fun shouldOverrideUrlLoading(v: WebView, r: WebResourceRequest): Boolean {
                        val target = r.url
                        val h = target.host
                        if (h != null && Filter.isBlocked(h)) {
                            Stats.redirectBlocked(h)
                            toast("Pop-up vers $h bloquée")
                        } else web.loadUrl(target.toString())
                        v.destroy()
                        return true
                    }
                }
                (resultMsg.obj as WebView.WebViewTransport).webView = catcher
                resultMsg.sendToTarget()
                return true
            }

            override fun onShowCustomView(view: View, callback: CustomViewCallback) {
                customView = view
                customCallback = callback
                fullscreen.addView(view, FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT))
                fullscreen.visibility = View.VISIBLE
                WindowCompat.getInsetsController(window, window.decorView).apply {
                    hide(WindowInsetsCompat.Type.systemBars())
                    systemBarsBehavior = WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
                }
            }

            override fun onHideCustomView() = exitFullscreen()
        }
    }

    private fun exitFullscreen() {
        val v = customView ?: return
        fullscreen.removeView(v)
        fullscreen.visibility = View.GONE
        customView = null
        customCallback?.onCustomViewHidden()
        customCallback = null
        WindowCompat.getInsetsController(window, window.decorView).show(WindowInsetsCompat.Type.systemBars())
    }

    private fun openExternal(url: String) {
        runCatching {
            val i = if (url.startsWith("intent:")) Intent.parseUri(url, Intent.URI_INTENT_SCHEME).apply {
                addCategory(Intent.CATEGORY_BROWSABLE); component = null; selector = null
            } else Intent(Intent.ACTION_VIEW, Uri.parse(url))
            startActivity(i)
        }.onFailure { toast("Aucune appli pour ouvrir ce lien") }
    }

    private fun toast(msg: String) = runOnUiThread { Toast.makeText(this, msg, Toast.LENGTH_SHORT).show() }

    private fun go(input: String) {
        val t = input.trim()
        if (t.isEmpty()) return
        val url = when {
            t.startsWith("http://") || t.startsWith("https://") -> t
            !t.contains(' ') && t.contains('.') -> "https://$t"
            else -> "https://duckduckgo.com/?q=" + Uri.encode(t)
        }
        web.loadUrl(url)
    }

    inner class Bridge {
        @JavascriptInterface
        fun adSkipped() {
            Stats.record("youtube.com", true, Origin.BROWSER, "pub vidéo YouTube sautée")
            runOnUiThread { blockedHere++ }
        }
    }

    @androidx.compose.runtime.Composable
    private fun Toolbar() {
        val focus = LocalFocusManager.current
        var editing by androidx.compose.runtime.remember { mutableStateOf<String?>(null) }
        Column(Modifier.background(C.Ink).statusBarsPadding()) {
            Row(Modifier.fillMaxWidth().height(56.dp).padding(horizontal = 4.dp), verticalAlignment = Alignment.CenterVertically) {
                IconButton({ if (web.canGoBack()) web.goBack() else finish() }) {
                    Icon(if (canBack) Icons.AutoMirrored.Filled.ArrowBack else Icons.Filled.Close, "Retour", tint = C.Text)
                }
                Box(
                    Modifier.weight(1f).height(40.dp).clip(RoundedCornerShape(12.dp)).background(C.Surface2).padding(horizontal = 12.dp),
                    contentAlignment = Alignment.CenterStart,
                ) {
                    BasicTextField(
                        value = editing ?: urlText.removePrefix("https://").removeSuffix("/"),
                        onValueChange = { editing = it },
                        singleLine = true,
                        textStyle = TextStyle(color = C.Text, fontSize = 14.sp),
                        cursorBrush = SolidColor(C.Violet),
                        keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Uri, imeAction = ImeAction.Go),
                        keyboardActions = KeyboardActions(onGo = {
                            go(editing ?: urlText); editing = null; focus.clearFocus()
                        }),
                        modifier = Modifier.fillMaxWidth(),
                    )
                }
                Spacer(Modifier.width(6.dp))
                Row(
                    Modifier.clip(RoundedCornerShape(12.dp)).background(C.Violet.copy(alpha = 0.18f)).padding(horizontal = 8.dp, vertical = 6.dp),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    EssenceLogo(Modifier.width(12.dp), stroke = 10f)
                    Spacer(Modifier.width(5.dp))
                    Text("$blockedHere", color = C.Text, fontSize = 13.sp)
                }
                IconButton({ web.reload() }) { Icon(Icons.Filled.Refresh, "Recharger", tint = C.Text) }
                IconButton({ web.loadUrl(if (isYouTube(web.url)) YOUTUBE else HOME) }) { Icon(Icons.Filled.Home, "Accueil", tint = C.Text) }
            }
            if (loadProgress in 1..99) {
                LinearProgressIndicator(
                    progress = { loadProgress / 100f },
                    modifier = Modifier.fillMaxWidth().height(2.dp), color = C.Cyan, trackColor = C.Ink,
                )
            } else Spacer(Modifier.height(2.dp))
        }
    }
}
