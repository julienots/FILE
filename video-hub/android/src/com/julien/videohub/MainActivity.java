package com.julien.videohub;

import android.app.Activity;
import android.content.ClipData;
import android.content.Intent;
import android.graphics.Bitmap;
import android.net.Uri;
import android.os.Bundle;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.webkit.HttpAuthHandler;
import android.webkit.ValueCallback;
import android.webkit.WebChromeClient;
import android.webkit.WebResourceError;
import android.webkit.WebResourceRequest;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.widget.Toast;

/** Tableau de bord du hub dans une WebView (dépôt, suivi, retouches, styles). */
public class MainActivity extends Activity {
    private static final int REQ_FILES = 11;
    private WebView web;
    private ValueCallback<Uri[]> pendingChooser;
    private int authTries;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        Notifs.ensureChannels(this);
        web = new WebView(this);
        setContentView(web);
        WebSettings s = web.getSettings();
        s.setJavaScriptEnabled(true);
        s.setDomStorageEnabled(true);
        s.setAllowFileAccess(false);
        s.setMediaPlaybackRequiresUserGesture(true);

        web.setWebViewClient(new WebViewClient() {
            @Override
            public void onReceivedHttpAuthRequest(WebView view, HttpAuthHandler handler, String host, String realm) {
                Prefs p = new Prefs(MainActivity.this);
                if (authTries++ < 2 && !p.pass().isEmpty()) {
                    handler.proceed(p.user(), p.pass());
                } else {
                    handler.cancel();
                    Toast.makeText(MainActivity.this, "Identifiants refusés : vérifiez les réglages.", Toast.LENGTH_LONG).show();
                    openSettings();
                }
            }

            @Override
            public void onPageStarted(WebView view, String url, Bitmap favicon) {
                setTitle("Hub vidéo — chargement…");
            }

            @Override
            public void onPageFinished(WebView view, String url) {
                authTries = 0;
                setTitle(getString(R.string.app_name));
            }

            @Override
            public void onReceivedError(WebView view, WebResourceRequest request, WebResourceError error) {
                if (request.isForMainFrame()) {
                    String html = "<html><body style='font-family:sans-serif;padding:24px'><h3>Hub injoignable</h3><p>"
                            + error.getDescription() + "</p><p>Vérifiez l'adresse dans les réglages et votre connexion.</p></body></html>";
                    view.loadDataWithBaseURL(null, html, "text/html", "utf-8", null);
                }
            }
        });

        web.setWebChromeClient(new WebChromeClient() {
            @Override
            public boolean onShowFileChooser(WebView view, ValueCallback<Uri[]> callback, FileChooserParams params) {
                if (pendingChooser != null) pendingChooser.onReceiveValue(null);
                pendingChooser = callback;
                String[] types = params.getAcceptTypes();
                String type = (types != null && types.length > 0 && types[0] != null && !types[0].isEmpty()) ? types[0] : "*/*";
                Intent i = new Intent(Intent.ACTION_GET_CONTENT).addCategory(Intent.CATEGORY_OPENABLE).setType(type);
                if (params.getMode() == FileChooserParams.MODE_OPEN_MULTIPLE) i.putExtra(Intent.EXTRA_ALLOW_MULTIPLE, true);
                try {
                    startActivityForResult(Intent.createChooser(i, "Choisir"), REQ_FILES);
                } catch (Exception e) {
                    pendingChooser = null;
                    return false;
                }
                return true;
            }
        });

        Perms.requestAll(this);
        if (savedInstanceState != null) web.restoreState(savedInstanceState);
        else load();
    }

    private void load() {
        Prefs p = new Prefs(this);
        if (!p.configured()) {
            openSettings();
            return;
        }
        RenderSyncJob.schedule(this);
        web.loadUrl(p.url() + "/");
    }

    private void openSettings() {
        startActivity(new Intent(this, SettingsActivity.class));
    }

    @Override
    protected void onResume() {
        super.onResume();
        Prefs p = new Prefs(this);
        String current = web.getUrl();
        if (p.configured() && (current == null || !current.startsWith(p.url()))) load();
    }

    @Override
    protected void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);
        web.saveState(outState);
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        if (requestCode != REQ_FILES || pendingChooser == null) {
            super.onActivityResult(requestCode, resultCode, data);
            return;
        }
        Uri[] result = null;
        if (resultCode == RESULT_OK && data != null) {
            ClipData clip = data.getClipData();
            if (clip != null) {
                result = new Uri[clip.getItemCount()];
                for (int i = 0; i < clip.getItemCount(); i++) result[i] = clip.getItemAt(i).getUri();
            } else if (data.getData() != null) {
                result = new Uri[]{data.getData()};
            }
        }
        pendingChooser.onReceiveValue(result);
        pendingChooser = null;
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
                openSettings();
                return true;
            default:
                return super.onOptionsItemSelected(item);
        }
    }

    @Override
    public void onBackPressed() {
        if (web.canGoBack()) web.goBack();
        else super.onBackPressed();
    }

    @Override
    protected void onDestroy() {
        web.setVisibility(View.GONE);
        web.destroy();
        super.onDestroy();
    }
}
