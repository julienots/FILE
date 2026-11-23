package com.superessence.lastblock;

import android.app.Activity;
import android.content.Context;
import android.content.res.AssetManager;
import android.graphics.Color;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.VibrationEffect;
import android.os.Vibrator;
import android.view.View;
import android.view.Window;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.WindowManager;
import android.webkit.JavascriptInterface;
import android.webkit.WebResourceRequest;
import android.webkit.WebResourceResponse;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;

import java.io.IOException;
import java.io.InputStream;
import java.util.HashMap;
import java.util.Map;

/**
 * Hôte natif du jeu : un WebView plein écran (WebGL 2) qui sert les fichiers du jeu
 * depuis les assets de l'APK sous une origine HTTPS locale, pour que les modules ES,
 * le localStorage et l'audio fonctionnent exactement comme dans un navigateur, hors connexion.
 */
public class MainActivity extends Activity {
    private static final String HOST = "appassets.lastblock.local";
    private static final String ORIGIN = "https://" + HOST + "/";
    private WebView web;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        Window w = getWindow();
        w.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        w.getDecorView().setBackgroundColor(Color.BLACK);

        web = new WebView(this);
        web.setBackgroundColor(Color.BLACK);
        WebSettings s = web.getSettings();
        s.setJavaScriptEnabled(true);
        s.setDomStorageEnabled(true);
        s.setDatabaseEnabled(true);
        s.setMediaPlaybackRequiresUserGesture(false);
        s.setAllowFileAccess(false);
        s.setAllowContentAccess(false);
        s.setCacheMode(WebSettings.LOAD_NO_CACHE);
        s.setTextZoom(100);
        web.setOverScrollMode(View.OVER_SCROLL_NEVER);
        web.setVerticalScrollBarEnabled(false);
        web.setHorizontalScrollBarEnabled(false);
        web.setLayerType(View.LAYER_TYPE_HARDWARE, null);
        web.addJavascriptInterface(new NativeBridge(this), "AndroidHost");
        web.setWebViewClient(new AssetClient(getAssets()));
        setContentView(web);
        hideSystemUi();
        web.loadUrl(ORIGIN + "index.html");
    }

    private void hideSystemUi() {
        if (Build.VERSION.SDK_INT >= 30) {
            getWindow().setDecorFitsSystemWindows(false);
            WindowInsetsController c = getWindow().getInsetsController();
            if (c != null) {
                c.hide(WindowInsets.Type.statusBars() | WindowInsets.Type.navigationBars());
                c.setSystemBarsBehavior(WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
            }
        } else {
            getWindow().getDecorView().setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY | View.SYSTEM_UI_FLAG_FULLSCREEN
                | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN);
        }
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) hideSystemUi();
    }

    @Override
    protected void onPause() {
        super.onPause();
        if (web != null) {
            web.evaluateJavascript("window.__onHostPause && window.__onHostPause()", null);
            web.onPause();
        }
    }

    @Override
    protected void onResume() {
        super.onResume();
        if (web != null) {
            web.onResume();
            web.evaluateJavascript("window.__onHostResume && window.__onHostResume()", null);
        }
    }

    @Override
    public void onBackPressed() {
        // Le jeu décide : fermer un panneau, mettre en pause, ou quitter.
        web.evaluateJavascript("window.__onBack ? window.__onBack() : 'exit'", new android.webkit.ValueCallback<String>() {
            @Override public void onReceiveValue(String value) {
                if (value != null && value.contains("exit")) finish();
            }
        });
    }

    @Override
    protected void onDestroy() {
        if (web != null) {
            web.destroy();
            web = null;
        }
        super.onDestroy();
    }

    /** Sert https://appassets.lastblock.local/… depuis assets/www/… */
    private static class AssetClient extends WebViewClient {
        private final AssetManager assets;
        private static final Map<String, String> MIME = new HashMap<>();
        static {
            MIME.put("html", "text/html");
            MIME.put("js", "text/javascript");
            MIME.put("mjs", "text/javascript");
            MIME.put("css", "text/css");
            MIME.put("json", "application/json");
            MIME.put("png", "image/png");
            MIME.put("jpg", "image/jpeg");
            MIME.put("svg", "image/svg+xml");
            MIME.put("ttf", "font/ttf");
            MIME.put("woff2", "font/woff2");
            MIME.put("ogg", "audio/ogg");
            MIME.put("mp3", "audio/mpeg");
            MIME.put("wav", "audio/wav");
        }

        AssetClient(AssetManager a) { assets = a; }

        @Override
        public WebResourceResponse shouldInterceptRequest(WebView view, WebResourceRequest req) {
            Uri u = req.getUrl();
            if (!HOST.equals(u.getHost())) {
                // Le jeu fonctionne entièrement hors connexion : aucune requête externe.
                return new WebResourceResponse("text/plain", "utf-8", 404, "Not Found", null, null);
            }
            String path = u.getPath();
            if (path == null || path.equals("/") || path.isEmpty()) path = "/index.html";
            try {
                InputStream in = assets.open("www" + path);
                String ext = path.substring(path.lastIndexOf('.') + 1).toLowerCase();
                String mime = MIME.containsKey(ext) ? MIME.get(ext) : "application/octet-stream";
                Map<String, String> headers = new HashMap<>();
                headers.put("Access-Control-Allow-Origin", "*");
                headers.put("Cache-Control", "no-cache");
                return new WebResourceResponse(mime, "utf-8", 200, "OK", headers, in);
            } catch (IOException e) {
                return new WebResourceResponse("text/plain", "utf-8", 404, "Not Found", null, null);
            }
        }

        @Override
        public boolean shouldOverrideUrlLoading(WebView view, WebResourceRequest request) {
            return true;
        }
    }

    /** Petites fonctions natives appelées par le jeu. */
    public static class NativeBridge {
        private final Activity act;
        NativeBridge(Activity a) { act = a; }

        @JavascriptInterface
        public void vibrate(int ms, int amplitude) {
            Vibrator v = (Vibrator) act.getSystemService(Context.VIBRATOR_SERVICE);
            if (v == null || !v.hasVibrator()) return;
            int amp = Math.max(1, Math.min(255, amplitude));
            v.vibrate(VibrationEffect.createOneShot(Math.max(1, Math.min(2000, ms)), amp));
        }

        @JavascriptInterface
        public void exitApp() {
            act.runOnUiThread(new Runnable() { @Override public void run() { act.finish(); } });
        }
    }
}
