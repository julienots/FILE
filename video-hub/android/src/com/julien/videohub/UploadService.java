package com.julien.videohub;

import android.app.Notification;
import android.app.NotificationManager;
import android.app.Service;
import android.content.ClipData;
import android.content.Intent;
import android.net.Uri;
import android.os.IBinder;
import android.os.PowerManager;

import org.json.JSONObject;

/** Service au premier plan : envoie au hub les vidéos partagées depuis la galerie (gros fichiers). */
public class UploadService extends Service {
    static final String EXTRA_CATEGORY = "category";
    static final String EXTRA_ASPECT = "aspect";
    static final String EXTRA_MODE = "mode";
    static final String EXTRA_NOTES = "notes";
    private static final int NOTIF_ID = 41;

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    @Override
    public int onStartCommand(final Intent intent, int flags, int startId) {
        Notification n = Notifs.builder(this, Notifs.CH_UPLOAD, "Envoi au hub", "Préparation…").setOngoing(true).build();
        startForeground(NOTIF_ID, n);
        if (intent == null || intent.getClipData() == null) {
            stopSelf(startId);
            return START_NOT_STICKY;
        }
        final int sid = startId;
        new Thread(new Runnable() {
            @Override
            public void run() {
                upload(intent);
                stopForeground(true);
                stopSelf(sid);
            }
        }, "hub-upload").start();
        return START_NOT_STICKY;
    }

    private void upload(Intent intent) {
        PowerManager.WakeLock wl = getSystemService(PowerManager.class)
                .newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "videohub:upload");
        wl.acquire(60 * 60 * 1000L);
        final NotificationManager nm = getSystemService(NotificationManager.class);
        Relay relay = new Relay(this);
        ClipData clip = intent.getClipData();
        int ok = 0;
        StringBuilder problems = new StringBuilder();
        try {
            for (int i = 0; i < clip.getItemCount(); i++) {
                Uri u = clip.getItemAt(i).getUri();
                if (u == null) continue;
                final Relay.Info info = Relay.describe(this, u);
                if (info.size > Relay.MAX_BYTES) {
                    problems.append(info.name).append(" : trop lourde (").append(info.size / 1048576)
                            .append(" Mo > 50 Mo). Partagez-la dans CUTTINX → Claude, elle sera traitée pareil.\n");
                    continue;
                }
                try {
                    String object = System.currentTimeMillis() + "_" + Relay.safe(info.name);
                    String url = relay.upload("hub-raw", object, u, info.size, new Relay.Progress() {
                        @Override
                        public void onProgress(long sent, long total) {
                            Notification.Builder b = Notifs.builder(UploadService.this, Notifs.CH_UPLOAD,
                                    "Envoi : " + info.name, (sent / 1048576) + " Mo envoyés").setOngoing(true);
                            if (total > 0) b.setProgress(1000, (int) (sent * 1000 / total), false);
                            else b.setProgress(0, 0, true);
                            nm.notify(NOTIF_ID, b.build());
                        }
                    });
                    JSONObject row = new JSONObject();
                    row.put("original_name", info.name);
                    row.put("category", intent.getStringExtra(EXTRA_CATEGORY));
                    row.put("target_aspect", intent.getStringExtra(EXTRA_ASPECT));
                    row.put("publish_mode", intent.getStringExtra(EXTRA_MODE));
                    row.put("notes", intent.getStringExtra(EXTRA_NOTES));
                    row.put("raw_url", url);
                    relay.insert("hub_videos", row);
                    ok++;
                } catch (Exception e) {
                    problems.append(info.name).append(" : ").append(e.getMessage()).append("\n");
                }
            }
        } finally {
            if (wl.isHeld()) wl.release();
        }
        if (ok > 0) {
            Notifs.show(this, 42, Notifs.CH_UPLOAD, "✅ " + ok + " vidéo(s) dans la file",
                    "Claude s'occupe du montage CUTTINX, de la légende et de la publication (passage toutes les heures)."
                            + (problems.length() > 0 ? "\n" + problems : ""));
        } else {
            Notifs.show(this, 42, Notifs.CH_UPLOAD, "❌ Rien n'a été envoyé", problems.toString());
        }
    }

    static String shorten(String s) {
        if (s == null) return "";
        return s.length() > 300 ? s.substring(0, 300) + "…" : s;
    }
}
