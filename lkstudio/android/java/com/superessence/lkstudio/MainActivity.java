// LK Studio — the mod maker of LOST KINGDOM. The whole editor is a web page (assets/www, bundled three.js);
// this activity only hosts it and gives it what a page cannot do alone: pick files, save the mod into
// Downloads, share it, open the game.
package com.superessence.lkstudio;

import android.app.Activity;
import android.content.ContentValues;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.provider.MediaStore;
import android.util.Base64;
import android.view.View;
import android.webkit.JavascriptInterface;
import android.webkit.ValueCallback;
import android.webkit.WebChromeClient;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import java.io.File;
import java.io.FileOutputStream;
import java.io.OutputStream;

public class MainActivity extends Activity {
    private static final int PICK = 41;
    private WebView web;
    private ValueCallback<Uri[]> pending;

    @Override
    protected void onCreate(Bundle state) {
        super.onCreate(state);
        web = new WebView(this);
        WebSettings s = web.getSettings();
        s.setJavaScriptEnabled(true);
        s.setDomStorageEnabled(true);
        s.setDatabaseEnabled(true);
        s.setAllowFileAccess(true);
        s.setAllowContentAccess(true);
        s.setMediaPlaybackRequiresUserGesture(false);
        web.setBackgroundColor(0xff16110d);
        web.setWebViewClient(new WebViewClient());
        web.setWebChromeClient(new WebChromeClient() {
            @Override
            public boolean onShowFileChooser(WebView v, ValueCallback<Uri[]> cb, FileChooserParams p) {
                if (pending != null) pending.onReceiveValue(null);
                pending = cb;
                Intent i = new Intent(Intent.ACTION_GET_CONTENT);
                i.addCategory(Intent.CATEGORY_OPENABLE);
                i.setType("*/*");
                try { startActivityForResult(Intent.createChooser(i, "Choisir un fichier"), PICK); }
                catch (Exception e) { pending = null; return false; }
                return true;
            }
        });
        web.addJavascriptInterface(new Bridge(), "LKS");
        setContentView(web);
        web.setSystemUiVisibility(View.SYSTEM_UI_FLAG_LAYOUT_STABLE);
        if (state != null) web.restoreState(state); else web.loadUrl("file:///android_asset/www/index.html");
    }

    @Override
    protected void onSaveInstanceState(Bundle out) { super.onSaveInstanceState(out); web.saveState(out); }

    @Override
    protected void onActivityResult(int req, int res, Intent data) {
        if (req != PICK || pending == null) { super.onActivityResult(req, res, data); return; }
        Uri[] r = null;
        if (res == RESULT_OK && data != null) {
            if (data.getClipData() != null) {
                r = new Uri[data.getClipData().getItemCount()];
                for (int i = 0; i < r.length; i++) r[i] = data.getClipData().getItemAt(i).getUri();
            } else if (data.getData() != null) r = new Uri[]{data.getData()};
        }
        pending.onReceiveValue(r);
        pending = null;
    }

    @Override
    public void onBackPressed() {
        web.evaluateJavascript("window.onAndroidBack ? String(window.onAndroidBack()) : 'false'", v -> {
            if (!"\"true\"".equals(v) && !"true".equals(v)) MainActivity.super.onBackPressed();
        });
    }

    private class Bridge {
        // Saves a file (base64) into Downloads/LOST KINGDOM Mods and keeps a copy for sharing. Returns the place, or "error: ...".
        @JavascriptInterface
        public String saveFile(String name, String b64, String mime) {
            try {
                byte[] bytes = Base64.decode(b64, Base64.DEFAULT);
                File share = new File(getCacheDir(), "share");
                share.mkdirs();
                try (FileOutputStream f = new FileOutputStream(new File(share, name))) { f.write(bytes); }
                if (Build.VERSION.SDK_INT >= 29) {
                    ContentValues v = new ContentValues();
                    v.put(MediaStore.MediaColumns.DISPLAY_NAME, name);
                    v.put(MediaStore.MediaColumns.MIME_TYPE, mime);
                    v.put(MediaStore.MediaColumns.RELATIVE_PATH, Environment.DIRECTORY_DOWNLOADS + "/LOST KINGDOM Mods");
                    Uri u = getContentResolver().insert(MediaStore.Downloads.EXTERNAL_CONTENT_URI, v);
                    if (u == null) return "error: Téléchargements indisponibles";
                    try (OutputStream o = getContentResolver().openOutputStream(u)) { o.write(bytes); }
                } else {
                    File dir = new File(Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS), "LOST KINGDOM Mods");
                    dir.mkdirs();
                    try (FileOutputStream f = new FileOutputStream(new File(dir, name))) { f.write(bytes); }
                }
                return "Téléchargements/LOST KINGDOM Mods/" + name;
            } catch (Exception e) {
                return "error: " + e.getMessage();
            }
        }

        // Shares a file saved by saveFile (send it to a friend, to Drive, to the game...).
        @JavascriptInterface
        public void shareFile(String name, String mime) {
            runOnUiThread(() -> {
                Uri u = Uri.parse("content://com.superessence.lkstudio.share/" + Uri.encode(name));
                Intent i = new Intent(Intent.ACTION_SEND);
                i.setType(mime);
                i.putExtra(Intent.EXTRA_STREAM, u);
                i.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
                startActivity(Intent.createChooser(i, "Partager le mod"));
            });
        }

        // Opens LOST KINGDOM if it is installed.
        @JavascriptInterface
        public boolean openGame() {
            Intent i = getPackageManager().getLaunchIntentForPackage("com.superessence.lostkingdom");
            if (i == null) return false;
            startActivity(i);
            return true;
        }

        @JavascriptInterface
        public boolean isAndroid() { return true; }
    }
}
