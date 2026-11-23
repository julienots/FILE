package com.superessence.focus

import android.app.Application
import android.app.NotificationChannel
import android.app.NotificationManager
import com.superessence.focus.core.Store

class FocusApp : Application() {
    override fun onCreate() {
        super.onCreate()
        Store.init(this)
        val nm = getSystemService(NotificationManager::class.java)
        nm.createNotificationChannel(NotificationChannel(CH_SESSION, "Session de révision", NotificationManager.IMPORTANCE_DEFAULT))
        nm.createNotificationChannel(NotificationChannel(CH_MUSIC, "Musique douce", NotificationManager.IMPORTANCE_LOW))
        nm.createNotificationChannel(NotificationChannel(CH_ROMANE, "Messages de Romane", NotificationManager.IMPORTANCE_HIGH))
    }

    companion object {
        const val CH_SESSION = "session"
        const val CH_MUSIC = "music"
        const val CH_ROMANE = "romane"
    }
}
