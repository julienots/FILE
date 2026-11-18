package com.julien.videohub;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

/** Relance la surveillance des rendus après un redémarrage ou une mise à jour de l'appli. */
public class BootReceiver extends BroadcastReceiver {
    @Override
    public void onReceive(Context context, Intent intent) {
        RenderSyncJob.schedule(context);
    }
}
