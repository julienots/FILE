package com.superessence.studio3d;

import android.app.Activity;
import android.app.AlertDialog;
import android.app.DownloadManager;
import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.SharedPreferences;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.net.Uri;
import android.os.Bundle;
import android.os.Environment;
import android.os.Handler;
import android.os.Looper;
import android.text.InputType;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.webkit.DownloadListener;
import android.webkit.URLUtil;
import android.webkit.ValueCallback;
import android.webkit.WebChromeClient;
import android.webkit.WebResourceError;
import android.webkit.WebResourceRequest;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.widget.Button;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
import android.widget.Toast;

import java.io.BufferedReader;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.Inet4Address;
import java.net.InetAddress;
import java.net.NetworkInterface;
import java.net.URL;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Enumeration;
import java.util.List;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.TimeUnit;

import org.json.JSONObject;

/**
 * SUPERESSENCE 3D Mobile : application compagnon du studio.
 *
 * Le studio (Blender) tourne sur l'ordinateur ; cette application s'y connecte sur le Wi-Fi,
 * s'appaire avec un code à 6 chiffres, puis affiche l'interface complète du studio.
 */
public class MainActivity extends Activity {
    private static final int BG = Color.parseColor("#121318");
    private static final int PANEL = Color.parseColor("#1D1F27");
    private static final int LINE = Color.parseColor("#3A3F50");
    private static final int TEXT = Color.parseColor("#E6E8EF");
    private static final int MUTED = Color.parseColor("#9097A8");
    private static final int ACCENT = Color.parseColor("#7C5CFF");
    private static final int ACCENT2 = Color.parseColor("#FF9A3C");
    private static final int FILE_REQUEST = 42;
    private static final int DEFAULT_PORT = 8765;

    private final Handler ui = new Handler(Looper.getMainLooper());
    private SharedPreferences prefs;
    private WebView web;
    private ValueCallback<Uri[]> fileCallback;
    private EditText hostField;
    private EditText codeField;
    private TextView status;
    private LinearLayout found;

    @Override
    protected void onCreate(Bundle state) {
        super.onCreate(state);
        prefs = getSharedPreferences("se3d", MODE_PRIVATE);
        String host = prefs.getString("host", "");
        String token = prefs.getString("token", "");
        if (host.length() > 0 && token.length() > 0) {
            showConnect(null);
            setStatus("Connexion au studio " + host + "…", false);
            checkAndOpen(host, token);
        } else {
            showConnect(null);
        }
    }

    // ------------------------------------------------------------------ écran de connexion
    private int dp(float v) {
        return (int) TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, v, getResources().getDisplayMetrics());
    }

    private GradientDrawable box(int fill, int stroke, float radius) {
        GradientDrawable g = new GradientDrawable();
        g.setColor(fill);
        g.setCornerRadius(dp(radius));
        if (stroke != 0) g.setStroke(dp(1), stroke);
        return g;
    }

    private TextView text(String s, float size, int color, boolean bold) {
        TextView t = new TextView(this);
        t.setText(s);
        t.setTextSize(TypedValue.COMPLEX_UNIT_SP, size);
        t.setTextColor(color);
        if (bold) t.setTypeface(Typeface.DEFAULT_BOLD);
        t.setLineSpacing(0, 1.15f);
        return t;
    }

    private EditText field(String hint, int inputType) {
        EditText e = new EditText(this);
        e.setHint(hint);
        e.setHintTextColor(Color.parseColor("#6B7184"));
        e.setTextColor(TEXT);
        e.setTextSize(TypedValue.COMPLEX_UNIT_SP, 17);
        e.setInputType(inputType);
        e.setSingleLine(true);
        e.setBackground(box(BG, LINE, 8));
        e.setPadding(dp(12), dp(10), dp(12), dp(10));
        return e;
    }

    private Button button(String label, int fill, int textColor) {
        Button b = new Button(this);
        b.setText(label);
        b.setAllCaps(false);
        b.setTextColor(textColor);
        b.setTextSize(TypedValue.COMPLEX_UNIT_SP, 16);
        b.setBackground(box(fill, 0, 10));
        b.setPadding(dp(14), dp(12), dp(14), dp(12));
        return b;
    }

    private LinearLayout.LayoutParams lp(int top) {
        LinearLayout.LayoutParams p = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT);
        p.topMargin = dp(top);
        return p;
    }

    private void showConnect(String message) {
        destroyWeb();
        ScrollView scroll = new ScrollView(this);
        scroll.setBackgroundColor(BG);
        scroll.setFillViewport(true);
        LinearLayout col = new LinearLayout(this);
        col.setOrientation(LinearLayout.VERTICAL);
        col.setPadding(dp(22), dp(36), dp(22), dp(28));
        scroll.addView(col);

        TextView brand = text("SUPERESSENCE", 28, TEXT, true);
        brand.setLetterSpacing(0.08f);
        col.addView(brand);
        TextView sub = text("3D STUDIO · MOBILE", 15, ACCENT2, true);
        sub.setLetterSpacing(0.12f);
        col.addView(sub);

        LinearLayout card = new LinearLayout(this);
        card.setOrientation(LinearLayout.VERTICAL);
        card.setBackground(box(PANEL, LINE, 12));
        card.setPadding(dp(16), dp(14), dp(16), dp(16));
        col.addView(card, lp(22));
        card.addView(text("Comment ça marche", 15, TEXT, true));
        card.addView(text("1. Sur l'ordinateur, lancez le studio en mode mobile : start_mobile_windows.bat "
                + "(ou ./start_mobile.sh).\n2. Dans le studio : Paramètres › Connexion mobile, un code à 6 chiffres "
                + "s'affiche.\n3. Ici : touchez « Rechercher », puis entrez le code.\n\nLe téléphone et l'ordinateur "
                + "doivent être sur le même Wi-Fi. Blender calcule sur l'ordinateur ; ce téléphone affiche le studio "
                + "complet (viewport 3D, génération, sculpture, export…).", 14, MUTED, false), lp(8));

        col.addView(text("Adresse du studio", 14, MUTED, false), lp(22));
        hostField = field("ex. 192.168.1.20:8765", InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_URI);
        hostField.setText(prefs.getString("host", ""));
        col.addView(hostField, lp(6));
        Button scan = button("Rechercher le studio sur le Wi-Fi", PANEL, TEXT);
        scan.setBackground(box(PANEL, LINE, 10));
        scan.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                discover();
            }
        });
        col.addView(scan, lp(8));
        found = new LinearLayout(this);
        found.setOrientation(LinearLayout.VERTICAL);
        col.addView(found, lp(4));

        col.addView(text("Code d'appairage", 14, MUTED, false), lp(18));
        codeField = field("6 chiffres", InputType.TYPE_CLASS_NUMBER);
        codeField.setTextSize(TypedValue.COMPLEX_UNIT_SP, 26);
        codeField.setLetterSpacing(0.3f);
        codeField.setGravity(Gravity.CENTER);
        col.addView(codeField, lp(6));

        Button go = button("Se connecter", ACCENT, Color.WHITE);
        go.setTypeface(Typeface.DEFAULT_BOLD);
        go.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                pair();
            }
        });
        col.addView(go, lp(18));

        status = text("", 14, MUTED, false);
        col.addView(status, lp(14));
        if (message != null) setStatus(message, true);

        TextView note = text("Version 0.1.0 · Aucune donnée n'est envoyée sur Internet : la connexion reste sur votre "
                + "réseau local.", 12, Color.parseColor("#6B7184"), false);
        col.addView(note, lp(26));
        setContentView(scroll);
    }

    private void setStatus(final String s, final boolean error) {
        ui.post(new Runnable() {
            @Override
            public void run() {
                if (status != null) {
                    status.setText(s);
                    status.setTextColor(error ? Color.parseColor("#FF8A95") : MUTED);
                }
            }
        });
    }

    private static String normalizeHost(String h) {
        h = h.trim();
        if (h.startsWith("http://")) h = h.substring(7);
        if (h.startsWith("https://")) h = h.substring(8);
        int slash = h.indexOf('/');
        if (slash >= 0) h = h.substring(0, slash);
        if (h.length() > 0 && !h.contains(":")) h = h + ":" + DEFAULT_PORT;
        return h;
    }

    // ------------------------------------------------------------------ réseau
    private static String http(String method, String url, String body, int timeoutMs) throws Exception {
        HttpURLConnection c = (HttpURLConnection) new URL(url).openConnection();
        c.setRequestMethod(method);
        c.setConnectTimeout(timeoutMs);
        c.setReadTimeout(Math.max(timeoutMs, 4000));
        if (body != null) {
            c.setDoOutput(true);
            c.setRequestProperty("Content-Type", "application/json");
            OutputStream os = c.getOutputStream();
            os.write(body.getBytes("UTF-8"));
            os.close();
        }
        int code = c.getResponseCode();
        InputStream in = code >= 400 ? c.getErrorStream() : c.getInputStream();
        StringBuilder sb = new StringBuilder();
        if (in != null) {
            BufferedReader r = new BufferedReader(new InputStreamReader(in, "UTF-8"));
            String line;
            while ((line = r.readLine()) != null) sb.append(line);
            r.close();
        }
        c.disconnect();
        return sb.toString();
    }

    private void discover() {
        found.removeAllViews();
        setStatus("Recherche du studio sur le réseau local…", false);
        new Thread(new Runnable() {
            @Override
            public void run() {
                final List<String> hits = Collections.synchronizedList(new ArrayList<String>());
                List<String> prefixes = new ArrayList<String>();
                try {
                    Enumeration<NetworkInterface> ifs = NetworkInterface.getNetworkInterfaces();
                    while (ifs != null && ifs.hasMoreElements()) {
                        NetworkInterface ni = ifs.nextElement();
                        if (!ni.isUp() || ni.isLoopback()) continue;
                        Enumeration<InetAddress> addrs = ni.getInetAddresses();
                        while (addrs.hasMoreElements()) {
                            InetAddress a = addrs.nextElement();
                            if (a instanceof Inet4Address && a.isSiteLocalAddress()) {
                                String ip = a.getHostAddress();
                                String p = ip.substring(0, ip.lastIndexOf('.') + 1);
                                if (!prefixes.contains(p)) prefixes.add(p);
                            }
                        }
                    }
                } catch (Exception ignored) {
                }
                if (prefixes.isEmpty()) {
                    setStatus("Aucun réseau Wi-Fi détecté. Connectez le téléphone au Wi-Fi de l'ordinateur.", true);
                    return;
                }
                ExecutorService pool = Executors.newFixedThreadPool(48);
                for (final String p : prefixes) {
                    for (int i = 1; i < 255; i++) {
                        final String host = p + i + ":" + DEFAULT_PORT;
                        pool.execute(new Runnable() {
                            @Override
                            public void run() {
                                try {
                                    String r = http("GET", "http://" + host + "/ping", null, 700);
                                    if (r.contains("superessence3d")) hits.add(host);
                                } catch (Exception ignored) {
                                }
                            }
                        });
                    }
                }
                pool.shutdown();
                try {
                    pool.awaitTermination(25, TimeUnit.SECONDS);
                } catch (InterruptedException ignored) {
                }
                ui.post(new Runnable() {
                    @Override
                    public void run() {
                        if (hits.isEmpty()) {
                            setStatus("Studio introuvable. Vérifiez qu'il est lancé en mode mobile, que le pare-feu "
                                    + "autorise Python, ou saisissez l'adresse affichée dans le studio.", true);
                            return;
                        }
                        setStatus(hits.size() + " studio(s) trouvé(s). Entrez maintenant le code.", false);
                        hostField.setText(hits.get(0));
                        for (final String h : hits) {
                            Button b = button("✓  " + h, BG, ACCENT2);
                            b.setBackground(box(BG, ACCENT2, 8));
                            b.setOnClickListener(new View.OnClickListener() {
                                @Override
                                public void onClick(View v) {
                                    hostField.setText(h);
                                }
                            });
                            found.addView(b, lp(6));
                        }
                    }
                });
            }
        }).start();
    }

    private void pair() {
        final String host = normalizeHost(hostField.getText().toString());
        final String code = codeField.getText().toString().trim();
        if (host.length() == 0) {
            setStatus("Indiquez l'adresse du studio (ou touchez « Rechercher »).", true);
            return;
        }
        if (code.length() != 6) {
            setStatus("Le code d'appairage comporte 6 chiffres (Paramètres › Connexion mobile dans le studio).", true);
            return;
        }
        setStatus("Appairage avec " + host + "…", false);
        new Thread(new Runnable() {
            @Override
            public void run() {
                try {
                    JSONObject body = new JSONObject();
                    body.put("code", code);
                    JSONObject r = new JSONObject(http("POST", "http://" + host + "/pair", body.toString(), 5000));
                    if (!r.optBoolean("ok")) {
                        setStatus(r.optString("error", "Appairage refusé."), true);
                        return;
                    }
                    final String token = r.getString("token");
                    prefs.edit().putString("host", host).putString("token", token).apply();
                    ui.post(new Runnable() {
                        @Override
                        public void run() {
                            openStudio(host, token);
                        }
                    });
                } catch (Exception e) {
                    setStatus("Connexion impossible à " + host + " : " + e.getMessage(), true);
                }
            }
        }).start();
    }

    private void checkAndOpen(final String host, final String token) {
        new Thread(new Runnable() {
            @Override
            public void run() {
                try {
                    String r = http("GET", "http://" + host + "/ping", null, 2500);
                    if (!r.contains("superessence3d")) throw new Exception("réponse inattendue");
                    ui.post(new Runnable() {
                        @Override
                        public void run() {
                            openStudio(host, token);
                        }
                    });
                } catch (Exception e) {
                    setStatus("Le studio " + host + " ne répond pas. Lancez-le en mode mobile sur l'ordinateur, puis "
                            + "touchez « Se connecter » (ou recherchez-le à nouveau).", true);
                }
            }
        }).start();
    }

    // ------------------------------------------------------------------ studio (WebView)
    private void openStudio(final String host, String token) {
        destroyWeb();
        FrameLayout root = new FrameLayout(this);
        root.setBackgroundColor(BG);
        web = new WebView(this);
        WebSettings s = web.getSettings();
        s.setJavaScriptEnabled(true);
        s.setDomStorageEnabled(true);
        s.setLoadWithOverviewMode(true);
        s.setUseWideViewPort(true);
        s.setBuiltInZoomControls(false);
        s.setSupportZoom(false);
        s.setMediaPlaybackRequiresUserGesture(false);
        s.setAllowFileAccess(false);
        s.setAllowContentAccess(true);
        s.setCacheMode(WebSettings.LOAD_NO_CACHE);
        web.setBackgroundColor(BG);
        web.setWebViewClient(new WebViewClient() {
            @Override
            public boolean shouldOverrideUrlLoading(WebView view, String url) {
                Uri u = Uri.parse(url);
                String h = u.getHost() + ":" + u.getPort();
                if (h.equals(host)) return false;
                try {
                    startActivity(new Intent(Intent.ACTION_VIEW, u));
                } catch (ActivityNotFoundException ignored) {
                }
                return true;
            }

            @Override
            public void onReceivedError(WebView view, WebResourceRequest request, WebResourceError error) {
                if (request.isForMainFrame()) {
                    showConnect("Connexion perdue avec le studio (" + error.getDescription() + "). Vérifiez qu'il est "
                            + "lancé en mode mobile, puis reconnectez-vous.");
                }
            }
        });
        web.setWebChromeClient(new WebChromeClient() {
            @Override
            public boolean onShowFileChooser(WebView view, ValueCallback<Uri[]> callback, FileChooserParams params) {
                if (fileCallback != null) fileCallback.onReceiveValue(null);
                fileCallback = callback;
                Intent i = new Intent(Intent.ACTION_GET_CONTENT);
                i.addCategory(Intent.CATEGORY_OPENABLE);
                i.setType("*/*");
                if (params.getMode() == FileChooserParams.MODE_OPEN_MULTIPLE) {
                    i.putExtra(Intent.EXTRA_ALLOW_MULTIPLE, true);
                }
                try {
                    startActivityForResult(Intent.createChooser(i, "Choisir un fichier"), FILE_REQUEST);
                } catch (ActivityNotFoundException e) {
                    fileCallback = null;
                    return false;
                }
                return true;
            }
        });
        web.setDownloadListener(new DownloadListener() {
            @Override
            public void onDownloadStart(String url, String userAgent, String disposition, String mime, long length) {
                try {
                    String name = URLUtil.guessFileName(url, disposition, mime);
                    DownloadManager.Request r = new DownloadManager.Request(Uri.parse(url));
                    r.setTitle(name);
                    r.setDescription("SUPERESSENCE 3D");
                    r.setNotificationVisibility(DownloadManager.Request.VISIBILITY_VISIBLE_NOTIFY_COMPLETED);
                    r.setDestinationInExternalPublicDir(Environment.DIRECTORY_DOWNLOADS, "SuperEssence3D/" + name);
                    DownloadManager dm = (DownloadManager) getSystemService(Context.DOWNLOAD_SERVICE);
                    dm.enqueue(r);
                    Toast.makeText(MainActivity.this, "Téléchargement : " + name + " (dossier Téléchargements)",
                            Toast.LENGTH_LONG).show();
                } catch (Exception e) {
                    Toast.makeText(MainActivity.this, "Téléchargement impossible : " + e.getMessage(),
                            Toast.LENGTH_LONG).show();
                }
            }
        });
        root.addView(web, new FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT));
        setContentView(root);
        web.loadUrl("http://" + host + "/?token=" + Uri.encode(token));
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        if (requestCode == FILE_REQUEST && fileCallback != null) {
            Uri[] result = null;
            if (resultCode == RESULT_OK && data != null) {
                if (data.getClipData() != null) {
                    int n = data.getClipData().getItemCount();
                    result = new Uri[n];
                    for (int i = 0; i < n; i++) result[i] = data.getClipData().getItemAt(i).getUri();
                } else if (data.getData() != null) {
                    result = new Uri[]{data.getData()};
                }
            }
            fileCallback.onReceiveValue(result);
            fileCallback = null;
            return;
        }
        super.onActivityResult(requestCode, resultCode, data);
    }

    private void destroyWeb() {
        if (web != null) {
            web.stopLoading();
            web.destroy();
            web = null;
        }
    }

    @Override
    public void onBackPressed() {
        if (web == null) {
            super.onBackPressed();
            return;
        }
        if (web.canGoBack()) {
            web.goBack();
            return;
        }
        new AlertDialog.Builder(this)
                .setTitle("SUPERESSENCE 3D")
                .setMessage("Quitter le studio mobile ?")
                .setPositiveButton("Quitter", new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface d, int w) {
                        finish();
                    }
                })
                .setNeutralButton("Changer de studio", new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface d, int w) {
                        prefs.edit().remove("token").apply();
                        showConnect(null);
                    }
                })
                .setNegativeButton("Annuler", null)
                .show();
    }

    @Override
    protected void onDestroy() {
        destroyWeb();
        super.onDestroy();
    }
}
