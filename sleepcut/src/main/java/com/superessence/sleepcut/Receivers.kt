package com.superessence.sleepcut

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.service.notification.NotificationListenerService
import androidx.core.content.ContextCompat

/** Réveil exact (même en veille profonde) : relance le service pour le fondu et la coupure. */
class AlarmReceiver : BroadcastReceiver() {
    override fun onReceive(context: Context, intent: Intent) {
        TimerController.load(context)
        if (TimerController.state.value.endAt == 0L) return
        runCatching {
            ContextCompat.startForegroundService(context, Intent(context, TimerService::class.java).setAction(TimerService.ACTION_WAKE))
        }
    }
}

/** Service vide : sa seule présence (une fois autorisée) donne accès aux lecteurs actifs. */
class MediaListener : NotificationListenerService()
