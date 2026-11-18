package com.julien.videohub;

import android.content.Context;
import android.content.SharedPreferences;

import java.util.HashSet;
import java.util.Set;

/** Réglages de l'appli (adresse du hub, identifiants, jeton d'envoi des rendus). */
final class Prefs {
    private final SharedPreferences p;

    Prefs(Context c) {
        p = c.getSharedPreferences("hub", Context.MODE_PRIVATE);
    }

    String url() {
        String u = p.getString("url", "").trim();
        while (u.endsWith("/")) u = u.substring(0, u.length() - 1);
        return u;
    }

    String user() { return p.getString("user", "admin"); }
    String pass() { return p.getString("pass", ""); }
    String token() { return p.getString("token", ""); }
    boolean syncEnabled() { return p.getBoolean("sync", true); }
    String folder() { return p.getString("folder", "Cuttinx"); }
    long lastSync() { return p.getLong("last_sync", 0L); }
    boolean configured() { return url().startsWith("http"); }

    void save(String url, String user, String pass, String token, boolean sync, String folder) {
        SharedPreferences.Editor e = p.edit()
                .putString("url", url.trim())
                .putString("user", user.trim())
                .putString("pass", pass)
                .putString("token", token.trim())
                .putBoolean("sync", sync)
                .putString("folder", folder.trim().isEmpty() ? "Cuttinx" : folder.trim());
        // Première activation : on ne renvoie pas tout l'historique de la galerie, seulement les nouveaux rendus.
        if (sync && p.getLong("last_sync", 0L) == 0L) {
            e.putLong("last_sync", System.currentTimeMillis() / 1000L - 3600L);
        }
        e.apply();
    }

    void setLastSync(long seconds) {
        p.edit().putLong("last_sync", seconds).apply();
    }

    boolean alreadySent(long mediaId) {
        return p.getStringSet("sent", new HashSet<String>()).contains(String.valueOf(mediaId));
    }

    void markSent(long mediaId) {
        Set<String> s = new HashSet<String>(p.getStringSet("sent", new HashSet<String>()));
        if (s.size() > 300) s.clear();
        s.add(String.valueOf(mediaId));
        p.edit().putStringSet("sent", s).apply();
    }
}
