package com.julien.videohub;

import android.app.Activity;
import android.content.ClipData;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.view.Menu;
import android.view.MenuItem;
import android.webkit.JavascriptInterface;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.widget.Toast;

import org.json.JSONObject;

import java.util.ArrayList;

/** Tableau de bord (page locale qui lit la file du relais) + bouton de dépôt. */
public class MainActivity extends Activity {
    private static final int REQ_PICK = 12;
    private WebView web;

    /** Pont JavaScript pour la page du tableau de bord. */
    public class Bridge {
        @JavascriptInterface
        public String config() {
            Prefs p = new Prefs(MainActivity.this);
            try {
                return new JSONObject().put("url", p.url()).put("key", p.key()).put("sync", p.syncEnabled()).toString();
            } catch (Exception e) {
                return "{}";
            }
        }

        @JavascriptInterface
        public void pick() {
            runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    Intent i = new Intent(Intent.ACTION_GET_CONTENT).addCategory(Intent.CATEGORY_OPENABLE)
                            .setType("video/*").putExtra(Intent.EXTRA_ALLOW_MULTIPLE, true);
                    startActivityForResult(Intent.createChooser(i, "Choisir des vidéos"), REQ_PICK);
                }
            });
        }

        @JavascriptInterface
        public void open(final String url) {
            runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    try {
                        startActivity(new Intent(Intent.ACTION_VIEW, Uri.parse(url)));
                    } catch (Exception ignored) {
                    }
                }
            });
        }

        @JavascriptInterface
        public void sync() {
            RenderSyncJob.runNow(MainActivity.this);
        }

        @JavascriptInterface
        public void settings() {
            startActivity(new Intent(MainActivity.this, SettingsActivity.class));
        }
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        Notifs.ensureChannels(this);
        web = new WebView(this);
        WebSettings s = web.getSettings();
        s.setJavaScriptEnabled(true);
        s.setDomStorageEnabled(true);
        web.addJavascriptInterface(new Bridge(), "Hub");
        setContentView(web);
        Perms.requestAll(this);
        if (!new Prefs(this).configured()) {
            startActivity(new Intent(this, SettingsActivity.class));
        }
        RenderSyncJob.schedule(this);
        web.loadUrl("file:///android_asset/dashboard.html");
    }

    @Override
    protected void onResume() {
        super.onResume();
        web.evaluateJavascript("window.refresh && refresh()", null);
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        if (requestCode != REQ_PICK || resultCode != RESULT_OK || data == null) {
            super.onActivityResult(requestCode, resultCode, data);
            return;
        }
        ArrayList<Uri> uris = new ArrayList<Uri>();
        ClipData clip = data.getClipData();
        if (clip != null) {
            for (int i = 0; i < clip.getItemCount(); i++) uris.add(clip.getItemAt(i).getUri());
        } else if (data.getData() != null) {
            uris.add(data.getData());
        }
        if (uris.isEmpty()) return;
        Intent share = new Intent(this, ShareActivity.class)
                .setAction(Intent.ACTION_SEND_MULTIPLE)
                .setType("video/*")
                .putParcelableArrayListExtra(Intent.EXTRA_STREAM, uris)
                .addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
        startActivity(share);
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        menu.add(0, 1, 0, "Actualiser");
        menu.add(0, 2, 1, "Envoyer les rendus maintenant");
        menu.add(0, 3, 2, "Réglages");
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        switch (item.getItemId()) {
            case 1:
                web.reload();
                return true;
            case 2:
                RenderSyncJob.runNow(this);
                Toast.makeText(this, "Recherche de nouveaux rendus CUTTINX…", Toast.LENGTH_SHORT).show();
                return true;
            case 3:
                startActivity(new Intent(this, SettingsActivity.class));
                return true;
            default:
                return super.onOptionsItemSelected(item);
        }
    }
}
