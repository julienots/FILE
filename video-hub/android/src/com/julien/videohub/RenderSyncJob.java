package com.julien.videohub;

import android.app.job.JobInfo;
import android.app.job.JobParameters;
import android.app.job.JobScheduler;
import android.app.job.JobService;
import android.content.ComponentName;
import android.content.ContentUris;
import android.content.Context;
import android.database.Cursor;
import android.net.Uri;
import android.os.Build;
import android.provider.MediaStore;
import android.util.Log;

import org.json.JSONObject;

import java.util.ArrayList;
import java.util.List;

/**
 * Envoie automatiquement au hub les MP4 que CUTTINX exporte dans Movies/Cuttinx.
 * Déclenché par JobScheduler quand la galerie vidéo change (+ un passage de secours toutes les 15 min).
 */
public class RenderSyncJob extends JobService {
    private static final String TAG = "HubSync";
    static final int JOB_CONTENT = 1;
    static final int JOB_PERIODIC = 2;
    static final int JOB_NOW = 3;

    static void schedule(Context c) {
        JobScheduler js = c.getSystemService(JobScheduler.class);
        Prefs p = new Prefs(c);
        if (!p.syncEnabled() || !p.configured()) {
            js.cancel(JOB_CONTENT);
            js.cancel(JOB_PERIODIC);
            return;
        }
        ComponentName cn = new ComponentName(c, RenderSyncJob.class);
        js.schedule(new JobInfo.Builder(JOB_CONTENT, cn)
                .addTriggerContentUri(new JobInfo.TriggerContentUri(
                        MediaStore.Video.Media.EXTERNAL_CONTENT_URI, JobInfo.TriggerContentUri.FLAG_NOTIFY_FOR_DESCENDANTS))
                .setTriggerContentUpdateDelay(5000)
                .setTriggerContentMaxDelay(60000)
                .setRequiredNetworkType(JobInfo.NETWORK_TYPE_ANY)
                .build());
        if (js.getPendingJob(JOB_PERIODIC) == null) {
            js.schedule(new JobInfo.Builder(JOB_PERIODIC, cn)
                    .setPeriodic(15 * 60 * 1000L)
                    .setPersisted(true)
                    .setRequiredNetworkType(JobInfo.NETWORK_TYPE_ANY)
                    .build());
        }
    }

    static void runNow(Context c) {
        c.getSystemService(JobScheduler.class).schedule(
                new JobInfo.Builder(JOB_NOW, new ComponentName(c, RenderSyncJob.class))
                        .setRequiredNetworkType(JobInfo.NETWORK_TYPE_ANY)
                        .setOverrideDeadline(1000)
                        .build());
    }

    @Override
    public boolean onStartJob(final JobParameters params) {
        new Thread(new Runnable() {
            @Override
            public void run() {
                boolean retry = false;
                try {
                    retry = syncOnce(RenderSyncJob.this);
                } catch (Exception e) {
                    Log.w(TAG, "synchro KO", e);
                    retry = true;
                }
                jobFinished(params, retry && params.getJobId() != JOB_PERIODIC);
                if (params.getJobId() == JOB_CONTENT) schedule(RenderSyncJob.this); // continuer à surveiller
            }
        }, "hub-sync").start();
        return true;
    }

    @Override
    public boolean onStopJob(JobParameters params) {
        return true;
    }

    private static final class Item {
        long id, added, modified, size;
        String name;
        int shortSide;
    }

    /** @return true s'il faut réessayer bientôt (fichier en cours d'écriture ou envoi en échec). */
    static boolean syncOnce(Context c) throws Exception {
        Prefs p = new Prefs(c);
        if (!p.syncEnabled() || !p.configured()) return false;
        if (!Perms.canReadVideos(c)) return false;

        long since = p.lastSync();
        long now = System.currentTimeMillis() / 1000L;
        String like = "%" + p.folder() + "%";
        boolean q = Build.VERSION.SDK_INT >= 29;
        String pathCol = q ? MediaStore.Video.Media.RELATIVE_PATH : MediaStore.Video.Media.DATA;
        List<String> cols = new ArrayList<String>();
        cols.add(MediaStore.Video.Media._ID);
        cols.add(MediaStore.Video.Media.DISPLAY_NAME);
        cols.add(MediaStore.Video.Media.DATE_ADDED);
        cols.add(MediaStore.Video.Media.DATE_MODIFIED);
        cols.add(MediaStore.Video.Media.SIZE);
        cols.add(MediaStore.Video.Media.WIDTH);
        cols.add(MediaStore.Video.Media.HEIGHT);
        String sel = pathCol + " LIKE ? AND " + MediaStore.Video.Media.DATE_ADDED + " >= ?";
        if (q) sel += " AND " + MediaStore.Video.Media.IS_PENDING + " = 0";

        List<Item> items = new ArrayList<Item>();
        Cursor cur = c.getContentResolver().query(MediaStore.Video.Media.EXTERNAL_CONTENT_URI,
                cols.toArray(new String[0]), sel, new String[]{like, String.valueOf(since - 120)},
                MediaStore.Video.Media.DATE_ADDED + " ASC");
        if (cur == null) return false;
        try {
            while (cur.moveToNext()) {
                Item it = new Item();
                it.id = cur.getLong(0);
                it.name = cur.getString(1);
                it.added = cur.getLong(2);
                it.modified = cur.getLong(3);
                it.size = cur.getLong(4);
                int w = cur.getInt(5), h = cur.getInt(6);
                it.shortSide = (w > 0 && h > 0) ? Math.min(w, h) : 0;
                items.add(it);
            }
        } finally {
            cur.close();
        }

        Relay relay = new Relay(c);
        boolean retry = false;
        long newest = since;
        for (Item it : items) {
            if (p.alreadySent(it.id)) {
                newest = Math.max(newest, it.added);
                continue;
            }
            if (it.size <= 0 || now - it.modified < 20) { // export CUTTINX encore en cours
                retry = true;
                break;
            }
            String name = it.name == null ? "render.mp4" : it.name;
            boolean draft = (it.shortSide > 0 && it.shortSide < 720) || name.toLowerCase().contains("brouillon");
            if (draft) { // les brouillons 480p de vérification ne sont pas publiés
                p.markSent(it.id);
                newest = Math.max(newest, it.added);
                continue;
            }
            if (it.size > Relay.MAX_BYTES) {
                Notifs.show(c, 50, Notifs.CH_SYNC, "⚠️ Rendu trop lourd pour l'envoi",
                        name + " fait " + (it.size / 1048576) + " Mo (max 50 Mo). Demandez un montage plus court.");
                p.markSent(it.id);
                newest = Math.max(newest, it.added);
                continue;
            }
            Uri uri = ContentUris.withAppendedId(MediaStore.Video.Media.EXTERNAL_CONTENT_URI, it.id);
            try {
                String url = relay.upload("hub-renders", System.currentTimeMillis() + "_" + Relay.safe(name), uri, it.size, null);
                JSONObject row = new JSONObject();
                row.put("file_name", name);
                row.put("public_url", url);
                row.put("short_side", it.shortSide);
                row.put("size_bytes", it.size);
                relay.insert("hub_renders", row);
            } catch (Exception e) {
                Notifs.show(c, 50, Notifs.CH_SYNC, "⚠️ Rendu non envoyé", name + "\n" + e.getMessage());
                retry = true;
                break;
            }
            p.markSent(it.id);
            newest = Math.max(newest, it.added);
            Notifs.show(c, (int) (60 + (it.id % 1000)), Notifs.CH_SYNC, "🎬 Rendu envoyé",
                    name + " : Claude le publiera au prochain passage.");
        }
        if (newest > since) p.setLastSync(newest);
        return retry;
    }
}
