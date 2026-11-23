package com.superessence.focus.core

import android.app.AlarmManager
import android.app.NotificationManager
import android.app.PendingIntent
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.media.MediaPlayer
import android.os.Build
import android.os.VibrationEffect
import android.os.Vibrator
import androidx.core.app.NotificationCompat
import com.superessence.focus.FocusApp
import com.superessence.focus.MainActivity
import com.superessence.focus.R

/** Session de révision : minuteur + (optionnellement) blocage des distractions. */
object Session {
    val running: Boolean get() = Store.sessionEnd > 0
    val blocking: Boolean get() = running && Store.sessionBlocking

    fun remainingMs(now: Long = System.currentTimeMillis()) = (Store.sessionEnd - now).coerceAtLeast(0)

    fun start(c: Context, minutes: Int, block: Boolean, strict: Boolean) {
        val now = System.currentTimeMillis()
        Store.lastDuration = minutes
        Store.sessionStart = now
        Store.sessionBlocking = block
        Store.sessionStrict = block && strict
        Store.sessionEnd = now + minutes * 60_000L
        scheduleEnd(c)
        if (block) NotifBlocker.silenceAll()
    }

    /** Vérifie si la session est terminée ; à appeler souvent (UI, service de blocage). */
    fun check(c: Context) {
        if (running && System.currentTimeMillis() >= Store.sessionEnd) finish(c, completed = true)
    }

    fun stop(c: Context): Boolean {
        if (!running) return true
        if (Store.sessionStrict && System.currentTimeMillis() < Store.sessionEnd) return false
        finish(c, completed = false)
        return true
    }

    private fun finish(c: Context, completed: Boolean) {
        val end = minOf(System.currentTimeMillis(), Store.sessionEnd)
        val minutes = ((end - Store.sessionStart) / 60_000L).toInt()
        Store.addMinutes(minutes)
        Store.sessionEnd = 0
        Store.sessionBlocking = false
        Store.sessionStrict = false
        cancelAlarm(c)
        if (completed) celebrate(c, minutes)
    }

    private fun celebrate(c: Context, minutes: Int) {
        runCatching {
            val afd = c.assets.openFd("sounds/bell.ogg")
            MediaPlayer().apply {
                setDataSource(afd.fileDescriptor, afd.startOffset, afd.length)
                setOnCompletionListener { it.release() }
                prepare(); start()
            }
        }
        runCatching {
            val v = c.getSystemService(Vibrator::class.java)
            v?.vibrate(VibrationEffect.createWaveform(longArrayOf(0, 250, 150, 250), -1))
        }
        val pi = PendingIntent.getActivity(c, 0, Intent(c, MainActivity::class.java), PendingIntent.FLAG_IMMUTABLE)
        val n = NotificationCompat.Builder(c, FocusApp.CH_SESSION)
            .setSmallIcon(R.drawable.ic_stat)
            .setContentTitle("Session terminée ✨")
            .setContentText("Bravo ! $minutes min de révision. Tes applis sont débloquées.")
            .setContentIntent(pi).setAutoCancel(true).build()
        runCatching { c.getSystemService(NotificationManager::class.java).notify(2, n) }
    }

    fun scheduleEnd(c: Context) {
        val am = c.getSystemService(AlarmManager::class.java)
        val pi = endIntent(c)
        val at = Store.sessionEnd
        if (Build.VERSION.SDK_INT < 31 || am.canScheduleExactAlarms()) {
            runCatching { am.setExactAndAllowWhileIdle(AlarmManager.RTC_WAKEUP, at, pi) }
                .onFailure { am.setAndAllowWhileIdle(AlarmManager.RTC_WAKEUP, at, pi) }
        } else {
            am.setAndAllowWhileIdle(AlarmManager.RTC_WAKEUP, at, pi)
        }
    }

    private fun cancelAlarm(c: Context) = c.getSystemService(AlarmManager::class.java).cancel(endIntent(c))

    private fun endIntent(c: Context) = PendingIntent.getBroadcast(
        c, 1, Intent(c, SessionEndReceiver::class.java), PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT,
    )
}

class SessionEndReceiver : BroadcastReceiver() {
    override fun onReceive(c: Context, i: Intent) { Store.init(c); Session.check(c) }
}

class BootReceiver : BroadcastReceiver() {
    override fun onReceive(c: Context, i: Intent) {
        Store.init(c)
        Session.check(c)
        if (Session.running) Session.scheduleEnd(c)
    }
}
