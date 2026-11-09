package com.superessence.noctea.notify

import android.Manifest
import android.app.AlarmManager
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.os.Build
import androidx.core.app.NotificationCompat
import androidx.core.app.NotificationManagerCompat
import androidx.core.content.ContextCompat
import com.superessence.noctea.MainActivity
import com.superessence.noctea.NocteaApp
import com.superessence.noctea.R
import com.superessence.noctea.ui.screens.WakeActivity
import java.time.LocalDateTime
import java.time.ZoneId

object Reminders {
    const val CHANNEL_RITUAL = "ritual"
    const val CHANNEL_ALARM = "alarm"
    private const val REQ_RITUAL = 41
    private const val REQ_ALARM = 42

    fun createChannels(context: Context) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) return
        val nm = context.getSystemService(NotificationManager::class.java)
        nm.createNotificationChannel(
            NotificationChannel(CHANNEL_RITUAL, "Rituel du soir", NotificationManager.IMPORTANCE_DEFAULT).apply {
                description = "Un doux rappel avant l'heure du coucher"
                setSound(null, null)
            },
        )
        nm.createNotificationChannel(
            NotificationChannel(CHANNEL_ALARM, "Réveil doux", NotificationManager.IMPORTANCE_HIGH).apply {
                description = "Réveil progressif en musique"
                setSound(null, null)
            },
        )
    }

    private fun nextTrigger(minutesOfDay: Int, days: Set<Int> = (1..7).toSet()): Long {
        val now = LocalDateTime.now()
        var t = now.withHour(minutesOfDay / 60).withMinute(minutesOfDay % 60).withSecond(0).withNano(0)
        var guard = 0
        while ((!t.isAfter(now) || t.dayOfWeek.value !in days) && guard++ < 8) t = t.plusDays(1)
        return t.atZone(ZoneId.systemDefault()).toInstant().toEpochMilli()
    }

    /** (Re)programme le rappel du rituel et le réveil selon les préférences. */
    fun reschedule(context: Context) {
        val app = context.applicationContext as NocteaApp
        val snap = app.store.value
        val am = context.getSystemService(AlarmManager::class.java)

        val ritualPi = PendingIntent.getBroadcast(
            context, REQ_RITUAL, Intent(context, ReminderReceiver::class.java),
            PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT,
        )
        am.cancel(ritualPi)
        if (snap.ritual.reminderEnabled) {
            val minutes = ((snap.ritual.bedtimeMinutes - snap.ritual.reminderOffsetMin) % 1440 + 1440) % 1440
            am.setAndAllowWhileIdle(AlarmManager.RTC_WAKEUP, nextTrigger(minutes), ritualPi)
        }

        val alarmPi = PendingIntent.getActivity(
            context, REQ_ALARM,
            Intent(context, WakeActivity::class.java).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK),
            PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT,
        )
        am.cancel(alarmPi)
        if (snap.alarm.enabled && canScheduleExact(context)) {
            val show = PendingIntent.getActivity(
                context, 43, Intent(context, MainActivity::class.java),
                PendingIntent.FLAG_IMMUTABLE,
            )
            am.setAlarmClock(AlarmManager.AlarmClockInfo(nextTrigger(snap.alarm.minutes, snap.alarm.days), show), alarmPi)
        }
    }

    fun canScheduleExact(context: Context): Boolean {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.S) return true
        return context.getSystemService(AlarmManager::class.java).canScheduleExactAlarms()
    }

    fun canNotify(context: Context): Boolean =
        Build.VERSION.SDK_INT < 33 ||
            ContextCompat.checkSelfPermission(context, Manifest.permission.POST_NOTIFICATIONS) == PackageManager.PERMISSION_GRANTED

    fun showRitualNotification(context: Context) {
        if (!canNotify(context)) return
        val app = context.applicationContext as NocteaApp
        val quote = app.catalog.quotes.randomOrNull()
        val open = PendingIntent.getActivity(
            context, 44,
            Intent(context, MainActivity::class.java).putExtra(MainActivity.EXTRA_OPEN_ROUTE, "ritual"),
            PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT,
        )
        val n = NotificationCompat.Builder(context, CHANNEL_RITUAL)
            .setSmallIcon(R.drawable.ic_notification)
            .setContentTitle("C'est bientôt l'heure de ton rituel 🌙")
            .setContentText(quote?.text ?: "Prends un moment pour toi avant de dormir.")
            .setStyle(NotificationCompat.BigTextStyle().bigText(quote?.let { "« ${it.text} »\n— ${it.author}" } ?: ""))
            .setContentIntent(open)
            .setAutoCancel(true)
            .setColor(0xFF8E7CFF.toInt())
            .build()
        try {
            NotificationManagerCompat.from(context).notify(1001, n)
        } catch (e: SecurityException) {
            // permission retirée entre-temps
        }
    }
}

class ReminderReceiver : BroadcastReceiver() {
    override fun onReceive(context: Context, intent: Intent) {
        Reminders.showRitualNotification(context)
        Reminders.reschedule(context)
    }
}

class BootReceiver : BroadcastReceiver() {
    override fun onReceive(context: Context, intent: Intent) {
        if (intent.action == Intent.ACTION_BOOT_COMPLETED ||
            intent.action == AlarmManager.ACTION_SCHEDULE_EXACT_ALARM_PERMISSION_STATE_CHANGED ||
            intent.action == Intent.ACTION_TIMEZONE_CHANGED
        ) {
            Reminders.reschedule(context)
        }
    }
}
