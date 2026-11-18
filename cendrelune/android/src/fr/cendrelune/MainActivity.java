package fr.cendrelune;

import android.app.Activity;
import android.os.Build;
import android.os.Bundle;
import android.view.View;
import android.view.WindowManager;
import android.webkit.WebResourceRequest;
import android.webkit.WebResourceResponse;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import java.io.InputStream;
import java.util.HashMap;
import java.util.Map;

public class MainActivity extends Activity {
  private static final String HOST = "appassets.androidplatform.net";
  private WebView web;

  @Override protected void onCreate(Bundle b) {
    super.onCreate(b);
    getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
    web = new WebView(this);
    WebSettings s = web.getSettings();
    s.setJavaScriptEnabled(true);
    s.setDomStorageEnabled(true);
    s.setMediaPlaybackRequiresUserGesture(false);
    web.setWebViewClient(new WebViewClient() {
      @Override public WebResourceResponse shouldInterceptRequest(WebView v, WebResourceRequest r) {
        if (!HOST.equals(r.getUrl().getHost())) return null;
        String p = r.getUrl().getPath();
        if (p == null || p.equals("/") || p.isEmpty()) p = "/index.html";
        try {
          InputStream in = getAssets().open("www" + p);
          Map<String, String> h = new HashMap<>();
          h.put("Access-Control-Allow-Origin", "*");
          return new WebResourceResponse(mime(p), mime(p).startsWith("text") || p.endsWith(".js") || p.endsWith(".json") ? "utf-8" : null, 200, "OK", h, in);
        } catch (Exception e) {
          return new WebResourceResponse("text/plain", "utf-8", 404, "Not Found", new HashMap<String, String>(), null);
        }
      }
    });
    setContentView(web);
    immersive();
    web.loadUrl("https://" + HOST + "/index.html");
  }

  private static String mime(String p) {
    if (p.endsWith(".html")) return "text/html";
    if (p.endsWith(".js")) return "application/javascript";
    if (p.endsWith(".json")) return "application/json";
    if (p.endsWith(".glb")) return "model/gltf-binary";
    if (p.endsWith(".png")) return "image/png";
    return "application/octet-stream";
  }

  private void immersive() {
    if (Build.VERSION.SDK_INT >= 19) getWindow().getDecorView().setSystemUiVisibility(
      View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY | View.SYSTEM_UI_FLAG_FULLSCREEN | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
      | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_LAYOUT_STABLE);
  }
  @Override public void onWindowFocusChanged(boolean f) { super.onWindowFocusChanged(f); if (f) immersive(); }
  @Override protected void onPause() { super.onPause(); web.onPause(); web.evaluateJavascript("document.dispatchEvent(new Event('visibilitychange'))", null); }
  @Override protected void onResume() { super.onResume(); web.onResume(); }
  @Override public void onBackPressed() { web.evaluateJavascript("window.dispatchEvent(new KeyboardEvent('keydown',{code:'Escape',key:'Escape'}))", null); }
}
