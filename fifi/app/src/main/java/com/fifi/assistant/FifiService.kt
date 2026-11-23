package com.fifi.assistant

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content.Context
import android.content.Intent
import android.content.pm.ServiceInfo
import android.os.Build
import android.os.IBinder
import androidx.core.app.NotificationCompat
import androidx.core.app.ServiceCompat

/** Service au premier plan : garde le micro ouvert pour entendre « Dis Fifi » même écran éteint / appli fermée. */
class FifiService : Service() {

    override fun onBind(intent: Intent?): IBinder? = null

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        if (intent?.action == ACTION_STOP) {
            FifiEngine.stopWakeMode()
            getSharedPreferences("fifi", MODE_PRIVATE).edit().putBoolean("wake", false).apply()
            ServiceCompat.stopForeground(this, ServiceCompat.STOP_FOREGROUND_REMOVE)
            stopSelf()
            return START_NOT_STICKY
        }
        try {
            ServiceCompat.startForeground(
                this, 1, notification(),
                if (Build.VERSION.SDK_INT >= 30) ServiceInfo.FOREGROUND_SERVICE_TYPE_MICROPHONE else 0
            )
        } catch (e: Exception) {
            stopSelf(); return START_NOT_STICKY
        }
        FifiEngine.init(this)
        FifiEngine.startWakeMode()
        return START_STICKY
    }

    override fun onDestroy() {
        FifiEngine.stopWakeMode()
        super.onDestroy()
    }

    private fun notification(): Notification {
        val nm = getSystemService(NotificationManager::class.java)
        nm.createNotificationChannel(
            NotificationChannel(CHANNEL, "Écoute « Dis Fifi »", NotificationManager.IMPORTANCE_LOW)
        )
        val open = PendingIntent.getActivity(
            this, 0, Intent(this, MainActivity::class.java), PendingIntent.FLAG_IMMUTABLE
        )
        val stop = PendingIntent.getService(
            this, 1, Intent(this, FifiService::class.java).setAction(ACTION_STOP), PendingIntent.FLAG_IMMUTABLE
        )
        return NotificationCompat.Builder(this, CHANNEL)
            .setSmallIcon(R.drawable.ic_stat_fifi)
            .setContentTitle("Fifi fait semblant de dormir")
            .setContentText("Dis « Dis Fifi » pour la réveiller")
            .setContentIntent(open)
            .addAction(0, "Laisse-la dormir", stop)
            .setOngoing(true)
            .build()
    }

    companion object {
        private const val CHANNEL = "fifi_wake"
        const val ACTION_STOP = "com.fifi.assistant.STOP"

        fun start(ctx: Context) {
            ctx.startForegroundService(Intent(ctx, FifiService::class.java))
        }

        fun stop(ctx: Context) {
            ctx.startService(Intent(ctx, FifiService::class.java).setAction(ACTION_STOP))
        }
    }
}
