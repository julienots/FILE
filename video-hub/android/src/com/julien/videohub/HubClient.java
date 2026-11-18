package com.julien.videohub;

import android.content.ContentResolver;
import android.content.Context;
import android.database.Cursor;
import android.net.Uri;
import android.provider.OpenableColumns;
import android.util.Base64;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/** Appels HTTP vers le hub (envoi multipart en flux, sans charger la vidéo en mémoire). */
final class HubClient {

    interface Progress {
        void onProgress(long sent, long total);
    }

    static final class FilePart {
        final String field;
        final Uri uri;
        final String name;
        final long size;
        final String mime;

        FilePart(String field, Uri uri, String name, long size, String mime) {
            this.field = field;
            this.uri = uri;
            this.name = name;
            this.size = size;
            this.mime = mime;
        }
    }

    static final class Result {
        final int code;
        final String body;

        Result(int code, String body) {
            this.code = code;
            this.body = body;
        }

        boolean ok() { return code >= 200 && code < 300; }
    }

    private final Context ctx;
    private final Prefs prefs;

    HubClient(Context ctx) {
        this.ctx = ctx.getApplicationContext();
        this.prefs = new Prefs(ctx);
    }

    static FilePart describe(Context c, String field, Uri uri) {
        ContentResolver cr = c.getContentResolver();
        String name = "video.mp4";
        long size = -1;
        Cursor cur = null;
        try {
            cur = cr.query(uri, new String[]{OpenableColumns.DISPLAY_NAME, OpenableColumns.SIZE}, null, null, null);
            if (cur != null && cur.moveToFirst()) {
                if (!cur.isNull(0)) name = cur.getString(0);
                if (!cur.isNull(1)) size = cur.getLong(1);
            }
        } catch (RuntimeException ignored) {
        } finally {
            if (cur != null) cur.close();
        }
        String mime = cr.getType(uri);
        return new FilePart(field, uri, name, size, mime == null ? "video/mp4" : mime);
    }

    private String basicAuth() {
        String raw = prefs.user() + ":" + prefs.pass();
        return "Basic " + Base64.encodeToString(raw.getBytes(StandardCharsets.UTF_8), Base64.NO_WRAP);
    }

    Result get(String path) throws IOException {
        HttpURLConnection con = (HttpURLConnection) new URL(prefs.url() + path).openConnection();
        con.setConnectTimeout(15000);
        con.setReadTimeout(30000);
        con.setRequestProperty("Authorization", basicAuth());
        return read(con);
    }

    /** Dépôt de vidéos brutes (même formulaire que l'interface web). */
    Result uploadVideos(List<FilePart> files, String category, String aspect, String mode, String notes, Progress progress)
            throws IOException {
        Map<String, String> fields = new LinkedHashMap<String, String>();
        fields.put("category", category);
        fields.put("target_aspect", aspect);
        fields.put("publish_mode", mode);
        fields.put("notes", notes);
        Map<String, String> headers = new LinkedHashMap<String, String>();
        headers.put("Authorization", basicAuth());
        return multipart("/api/videos", headers, fields, files, progress);
    }

    /** Envoi d'un MP4 rendu par CUTTINX vers la boîte de réception du hub. */
    Result uploadRender(FilePart file, int shortSide, Progress progress) throws IOException {
        Map<String, String> fields = new LinkedHashMap<String, String>();
        if (shortSide > 0) fields.put("short_side", String.valueOf(shortSide));
        Map<String, String> headers = new LinkedHashMap<String, String>();
        headers.put("X-Upload-Token", prefs.token());
        List<FilePart> files = new ArrayList<FilePart>();
        files.add(file);
        return multipart("/api/renders/inbox", headers, fields, files, progress);
    }

    private Result multipart(String path, Map<String, String> headers, Map<String, String> fields, List<FilePart> files,
                             Progress progress) throws IOException {
        String boundary = "----hub" + System.nanoTime();
        List<byte[]> heads = new ArrayList<byte[]>();
        StringBuilder sb = new StringBuilder();
        for (Map.Entry<String, String> f : fields.entrySet()) {
            sb.append("--").append(boundary).append("\r\n")
                    .append("Content-Disposition: form-data; name=\"").append(f.getKey()).append("\"\r\n\r\n")
                    .append(f.getValue() == null ? "" : f.getValue()).append("\r\n");
        }
        byte[] fieldBytes = sb.toString().getBytes(StandardCharsets.UTF_8);
        long total = fieldBytes.length;
        boolean known = true;
        for (FilePart fp : files) {
            String h = "--" + boundary + "\r\n"
                    + "Content-Disposition: form-data; name=\"" + fp.field + "\"; filename=\""
                    + fp.name.replace("\"", "_") + "\"\r\n"
                    + "Content-Type: " + fp.mime + "\r\n\r\n";
            byte[] hb = h.getBytes(StandardCharsets.UTF_8);
            heads.add(hb);
            total += hb.length + 2;
            if (fp.size < 0) known = false; else total += fp.size;
        }
        byte[] tail = ("--" + boundary + "--\r\n").getBytes(StandardCharsets.UTF_8);
        total += tail.length;

        HttpURLConnection con = (HttpURLConnection) new URL(prefs.url() + path).openConnection();
        con.setDoOutput(true);
        con.setRequestMethod("POST");
        con.setConnectTimeout(20000);
        con.setReadTimeout(10 * 60 * 1000);
        con.setRequestProperty("Content-Type", "multipart/form-data; boundary=" + boundary);
        for (Map.Entry<String, String> h : headers.entrySet()) con.setRequestProperty(h.getKey(), h.getValue());
        if (known) con.setFixedLengthStreamingMode(total); else con.setChunkedStreamingMode(256 * 1024);

        long sent = 0;
        OutputStream out = con.getOutputStream();
        try {
            out.write(fieldBytes);
            sent += fieldBytes.length;
            byte[] buf = new byte[256 * 1024];
            for (int i = 0; i < files.size(); i++) {
                FilePart fp = files.get(i);
                out.write(heads.get(i));
                InputStream in = ctx.getContentResolver().openInputStream(fp.uri);
                if (in == null) throw new IOException("Fichier illisible : " + fp.name);
                try {
                    int n;
                    long last = 0;
                    while ((n = in.read(buf)) > 0) {
                        out.write(buf, 0, n);
                        sent += n;
                        if (progress != null && sent - last > 512 * 1024) {
                            last = sent;
                            progress.onProgress(sent, known ? total : -1);
                        }
                    }
                } finally {
                    in.close();
                }
                out.write("\r\n".getBytes(StandardCharsets.UTF_8));
            }
            out.write(tail);
        } finally {
            out.close();
        }
        if (progress != null) progress.onProgress(total, total);
        return read(con);
    }

    private static Result read(HttpURLConnection con) throws IOException {
        int code = con.getResponseCode();
        InputStream in = code >= 400 ? con.getErrorStream() : con.getInputStream();
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
        return new Result(code, body);
    }
}
