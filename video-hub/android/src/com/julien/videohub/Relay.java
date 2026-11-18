package com.julien.videohub;

import android.content.Context;
import android.database.Cursor;
import android.net.Uri;
import android.provider.OpenableColumns;

import org.json.JSONObject;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.net.URLEncoder;
import java.nio.charset.StandardCharsets;

/**
 * Relais Supabase : stockage public des vidéos (hub-raw / hub-renders) et file d'attente (tables hub_*).
 * Une tâche Claude programmée lit cette file, monte dans CUTTINX et publie sur Instagram.
 */
final class Relay {
    static final long MAX_BYTES = 50L * 1024 * 1024; // limite par fichier du plan gratuit Supabase

    interface Progress {
        void onProgress(long sent, long total);
    }

    static final class Info {
        final String name;
        final long size;

        Info(String name, long size) {
            this.name = name;
            this.size = size;
        }
    }

    private final Context ctx;
    private final Prefs prefs;

    Relay(Context c) {
        ctx = c.getApplicationContext();
        prefs = new Prefs(c);
    }

    static Info describe(Context c, Uri uri) {
        String name = "video.mp4";
        long size = -1;
        Cursor cur = null;
        try {
            cur = c.getContentResolver().query(uri, new String[]{OpenableColumns.DISPLAY_NAME, OpenableColumns.SIZE}, null, null, null);
            if (cur != null && cur.moveToFirst()) {
                if (!cur.isNull(0)) name = cur.getString(0);
                if (!cur.isNull(1)) size = cur.getLong(1);
            }
        } catch (RuntimeException ignored) {
        } finally {
            if (cur != null) cur.close();
        }
        return new Info(name, size);
    }

    static String safe(String name) {
        String s = name.replaceAll("[^A-Za-z0-9._-]+", "_");
        if (s.length() > 80) s = s.substring(s.length() - 80);
        return s.isEmpty() ? "video.mp4" : s;
    }

    private HttpURLConnection open(String path, String method) throws IOException {
        HttpURLConnection con = (HttpURLConnection) new URL(prefs.url() + path).openConnection();
        con.setRequestMethod(method);
        con.setConnectTimeout(20000);
        con.setReadTimeout(10 * 60 * 1000);
        con.setRequestProperty("apikey", prefs.key());
        if (prefs.key().startsWith("eyJ")) con.setRequestProperty("Authorization", "Bearer " + prefs.key());
        return con;
    }

    /** Envoie un fichier dans un bucket public et renvoie son URL publique. */
    String upload(String bucket, String objectName, Uri uri, long size, Progress progress) throws IOException {
        String encoded = URLEncoder.encode(objectName, "UTF-8").replace("+", "%20").replace("%2F", "/");
        HttpURLConnection con = open("/storage/v1/object/" + bucket + "/" + encoded, "POST");
        con.setDoOutput(true);
        con.setRequestProperty("Content-Type", "video/mp4");
        con.setRequestProperty("x-upsert", "false");
        if (size > 0) con.setFixedLengthStreamingMode(size); else con.setChunkedStreamingMode(256 * 1024);
        InputStream in = ctx.getContentResolver().openInputStream(uri);
        if (in == null) throw new IOException("Fichier illisible");
        OutputStream out = con.getOutputStream();
        try {
            byte[] buf = new byte[256 * 1024];
            long sent = 0, last = 0;
            int n;
            while ((n = in.read(buf)) > 0) {
                out.write(buf, 0, n);
                sent += n;
                if (progress != null && sent - last > 512 * 1024) {
                    last = sent;
                    progress.onProgress(sent, size);
                }
            }
        } finally {
            in.close();
            out.close();
        }
        String body = read(con);
        if (lastCode >= 300) throw new IOException("Stockage " + lastCode + " : " + shorten(body));
        return prefs.url() + "/storage/v1/object/public/" + bucket + "/" + encoded;
    }

    /** Ajoute une ligne dans une table (droits limités par les règles RLS du projet). */
    void insert(String table, JSONObject row) throws IOException {
        HttpURLConnection con = open("/rest/v1/" + table, "POST");
        con.setDoOutput(true);
        con.setRequestProperty("Content-Type", "application/json");
        con.setRequestProperty("Prefer", "return=minimal");
        byte[] b = row.toString().getBytes(StandardCharsets.UTF_8);
        con.setFixedLengthStreamingMode(b.length);
        OutputStream out = con.getOutputStream();
        out.write(b);
        out.close();
        String body = read(con);
        if (lastCode >= 300) throw new IOException("Base " + lastCode + " : " + shorten(body));
    }

    /** Test de connexion : lecture de la table des réglages. */
    String ping() throws IOException {
        HttpURLConnection con = open("/rest/v1/hub_settings?select=key&limit=1", "GET");
        String body = read(con);
        if (lastCode >= 300) throw new IOException("Réponse " + lastCode + " : " + shorten(body));
        return body;
    }

    private int lastCode;

    private String read(HttpURLConnection con) throws IOException {
        lastCode = con.getResponseCode();
        InputStream in = lastCode >= 400 ? con.getErrorStream() : con.getInputStream();
        String body = "";
        if (in != null) {
            ByteArrayOutputStream bo = new ByteArrayOutputStream();
            byte[] b = new byte[8192];
            int n;
            while ((n = in.read(b)) > 0) bo.write(b, 0, n);
            in.close();
            body = new String(bo.toByteArray(), StandardCharsets.UTF_8);
        }
        con.disconnect();
        return body;
    }

    static String shorten(String s) {
        if (s == null) return "";
        return s.length() > 300 ? s.substring(0, 300) + "…" : s;
    }
}
