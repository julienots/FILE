package com.superessence.focus.core

import android.app.Notification
import android.content.ComponentName
import android.content.Context
import android.service.notification.NotificationListenerService
import android.service.notification.StatusBarNotification
import androidx.core.app.NotificationManagerCompat

/** Fait taire les notifications des autres conversations (WhatsApp, Snap, SMS…) pendant la session. */
class NotifBlocker : NotificationListenerService() {
    override fun onListenerConnected() { Store.init(this); instance = this }
    override fun onListenerDisconnected() { if (instance === this) instance = null }

    override fun onNotificationPosted(sbn: StatusBarNotification) {
        if (Session.blocking && shouldSilence(sbn)) runCatching { cancelNotification(sbn.key) }
    }

    private fun shouldSilence(sbn: StatusBarNotification): Boolean {
        if (Apps.notifAllowed(this, sbn.packageName)) return false
        if (sbn.notification.flags and Notification.FLAG_FOREGROUND_SERVICE != 0) return false
        return true
    }

    companion object {
        @Volatile private var instance: NotifBlocker? = null

        fun silenceAll() {
            val s = instance ?: return
            runCatching { s.activeNotifications?.filter { s.shouldSilence(it) }?.forEach { s.cancelNotification(it.key) } }
        }

        fun isEnabled(c: Context): Boolean =
            NotificationManagerCompat.getEnabledListenerPackages(c).contains(c.packageName)

        fun component(c: Context) = ComponentName(c, NotifBlocker::class.java)
    }
}
