package com.julien.videohub;

import android.app.Notification;
import android.app.NotificationManager;
import android.app.Service;
import android.content.ClipData;
import android.content.Intent;
import android.net.Uri;
import android.os.IBinder;
import android.os.PowerManager;

import java.util.ArrayList;
import java.util.List;

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
        try {
            ClipData clip = intent.getClipData();
            List<HubClient.FilePart> files = new ArrayList<HubClient.FilePart>();
            for (int i = 0; i < clip.getItemCount(); i++) {
                Uri u = clip.getItemAt(i).getUri();
                if (u != null) files.add(HubClient.describe(this, "files", u));
            }
            final String label = files.size() == 1 ? files.get(0).name : files.size() + " vidéos";
            HubClient.Result r = new HubClient(this).uploadVideos(
                    files,
                    intent.getStringExtra(EXTRA_CATEGORY),
                    intent.getStringExtra(EXTRA_ASPECT),
                    intent.getStringExtra(EXTRA_MODE),
                    intent.getStringExtra(EXTRA_NOTES),
                    new HubClient.Progress() {
                        @Override
                        public void onProgress(long sent, long total) {
                            Notification.Builder b = Notifs.builder(UploadService.this, Notifs.CH_UPLOAD,
                                    "Envoi au hub : " + label, (sent / 1048576) + " Mo envoyés").setOngoing(true);
                            if (total > 0) b.setProgress(1000, (int) (sent * 1000 / total), false);
                            else b.setProgress(0, 0, true);
                            nm.notify(NOTIF_ID, b.build());
                        }
                    });
            if (r.ok()) {
                Notifs.show(this, 42, Notifs.CH_UPLOAD, "✅ Dans la file du hub",
                        label + " : montage, légende et publication se font automatiquement.");
            } else {
                Notifs.show(this, 42, Notifs.CH_UPLOAD, "❌ Envoi refusé (" + r.code + ")", shorten(r.body));
            }
        } catch (Exception e) {
            Notifs.show(this, 42, Notifs.CH_UPLOAD, "❌ Envoi impossible", String.valueOf(e.getMessage()));
        } finally {
            if (wl.isHeld()) wl.release();
        }
    }

    static String shorten(String s) {
        if (s == null) return "";
        return s.length() > 300 ? s.substring(0, 300) + "…" : s;
    }
}
