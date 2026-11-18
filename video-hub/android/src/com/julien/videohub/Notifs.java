package com.julien.videohub;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;

/** Canaux et construction des notifications. */
final class Notifs {
    static final String CH_UPLOAD = "upload";
    static final String CH_SYNC = "sync";

    private Notifs() {}

    static void ensureChannels(Context c) {
        NotificationManager nm = c.getSystemService(NotificationManager.class);
        nm.createNotificationChannel(new NotificationChannel(CH_UPLOAD, "Envoi des vidéos", NotificationManager.IMPORTANCE_LOW));
        nm.createNotificationChannel(new NotificationChannel(CH_SYNC, "Rendus CUTTINX envoyés", NotificationManager.IMPORTANCE_LOW));
    }

    static Notification.Builder builder(Context c, String channel, String title, String text) {
        ensureChannels(c);
        Intent open = new Intent(c, MainActivity.class).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        PendingIntent pi = PendingIntent.getActivity(c, 0, open, PendingIntent.FLAG_IMMUTABLE | PendingIntent.FLAG_UPDATE_CURRENT);
        return new Notification.Builder(c, channel)
                .setSmallIcon(R.drawable.ic_notif)
                .setContentTitle(title)
                .setContentText(text)
                .setStyle(new Notification.BigTextStyle().bigText(text))
                .setContentIntent(pi);
    }

    static void show(Context c, int id, String channel, String title, String text) {
        try {
            c.getSystemService(NotificationManager.class).notify(id, builder(c, channel, title, text).setAutoCancel(true).build());
        } catch (SecurityException ignored) {
            // notifications refusées par l'utilisateur
        }
    }
}
