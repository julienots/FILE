package com.superessence.sleepcut

import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content.Context
import android.content.Intent
import android.content.pm.ServiceInfo
import android.hardware.Sensor
import android.hardware.SensorEvent
import android.hardware.SensorEventListener
import android.hardware.SensorManager
import android.media.AudioManager
import android.os.Build
import android.os.Handler
import android.os.IBinder
import android.os.Looper
import android.os.PowerManager
import android.os.VibrationEffect
import android.os.Vibrator
import android.os.VibratorManager
import androidx.core.app.NotificationCompat
import androidx.core.app.ServiceCompat
import java.text.DateFormat
import java.util.Date
import kotlin.math.roundToInt
import kotlin.math.sqrt

/**
 * Service de premier plan qui compte à rebours, fait le fondu de volume
 * puis coupe la musique de toutes les applications — même écran éteint.
 */
class TimerService : Service(), SensorEventListener {

    private val handler = Handler(Looper.getMainLooper())
    private var wakeLock: PowerManager.WakeLock? = null
    private lateinit var audio: AudioManager
    private var fadeStartVolume = -1
    private var lastSetVolume = -1
    private var finishing = false
    private var sensorOn = false
    private var lastShake = 0L
    private var shakeCount = 0

    private val tick = object : Runnable {
        override fun run() { onTick() }
    }

    override fun onCreate() {
        super.onCreate()
        audio = getSystemService(AudioManager::class.java)
        TimerController.load(this)
        createChannels(this)
    }

    override fun onBind(intent: Intent?): IBinder? = null

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        // toujours repasser au premier plan (obligatoire après startForegroundService)
        runCatching { goForeground() }
        when (intent?.action) {
            ACTION_CANCEL -> { cancelInternal(); return START_NOT_STICKY }
            ACTION_CANCEL_NOTIF -> { TimerController.clear(this); cancelInternal(); return START_NOT_STICKY }
            ACTION_EXTEND_5 -> TimerController.extend(this, 5)
            ACTION_EXTEND_15 -> TimerController.extend(this, 15)
            ACTION_WAKE -> acquireWake(TimerController.settings.value.fadeSeconds * 1000L + 120_000L)
        }
        val st = TimerController.state.value
        if (st.endAt == 0L) { cancelInternal(); return START_NOT_STICKY }
        // Sans alarme exacte, on garde le processeur éveillé tout le long pour être sûr de couper à l'heure.
        if (!TimerController.canExact(this)) acquireWake(st.endAt - System.currentTimeMillis() + 120_000L)
        if (intent?.action == ACTION_UPDATE || intent?.action == ACTION_EXTEND_5 || intent?.action == ACTION_EXTEND_15) abortFade()
        updateSensor()
        handler.removeCallbacks(tick)
        handler.post(tick)
        return START_STICKY
    }

    private fun goForeground() {
        val n = buildNotification(this)
        val type = if (Build.VERSION.SDK_INT >= 34) ServiceInfo.FOREGROUND_SERVICE_TYPE_SPECIAL_USE else 0
        ServiceCompat.startForeground(this, NOTIF_ID, n, type)
    }

    private fun onTick() {
        val st = TimerController.state.value
        if (st.endAt == 0L || finishing) return
        val now = System.currentTimeMillis()
        val left = st.endAt - now
        val fadeMs = TimerController.settings.value.fadeSeconds * 1000L
        if (left <= 0) { finish(); return }
        if (fadeMs > 0 && left <= fadeMs) {
            if (!st.fading) {
                acquireWake(left + 120_000L)
                fadeStartVolume = audio.getStreamVolume(AudioManager.STREAM_MUSIC)
                lastSetVolume = fadeStartVolume
                TimerController.setFading(this, true)
                notifyUpdate()
            }
            val target = (fadeStartVolume * (left.toFloat() / fadeMs)).roundToInt().coerceAtLeast(0)
            val cur = audio.getStreamVolume(AudioManager.STREAM_MUSIC)
            if (cur > lastSetVolume) {
                // l'utilisateur a remonté le volume : il ne dort pas encore → +5 min
                fadeStartVolume = cur
                TimerController.extend(this, 5)
                abortFade(); notifyUpdate()
            } else if (target < cur) {
                audio.setStreamVolume(AudioManager.STREAM_MUSIC, target, 0)
                lastSetVolume = target
            }
            handler.postDelayed(tick, 200)
        } else {
            // une fois par seconde suffit (le réveil exact s'occupe de la veille profonde)
            if (left <= fadeMs + 10_000L) acquireWake(left + 120_000L)
            handler.postDelayed(tick, if (left > 120_000L) 5_000L else 1_000L)
        }
    }

    private fun abortFade() {
        if (fadeStartVolume >= 0) {
            audio.setStreamVolume(AudioManager.STREAM_MUSIC, fadeStartVolume, 0)
        }
        fadeStartVolume = -1; lastSetVolume = -1
        if (TimerController.state.value.fading) TimerController.setFading(this, false)
    }

    private fun finish() {
        if (finishing) return
        finishing = true
        acquireWake(60_000L)
        val settings = TimerController.settings.value
        val restoreTo = if (fadeStartVolume >= 0) fadeStartVolume else audio.getStreamVolume(AudioManager.STREAM_MUSIC)
        Thread {
            val res = MusicStopper.stopEverything(this)
            var msg = res.summary
            if (settings.restoreVolume) {
                if (!res.stillPlaying) {
                    audio.setStreamVolume(AudioManager.STREAM_MUSIC, restoreTo, 0)
                } else {
                    audio.setStreamVolume(AudioManager.STREAM_MUSIC, 0, 0)
                    msg += " Une appli continuait : volume laissé à zéro."
                }
            } else if (res.stillPlaying) audio.setStreamVolume(AudioManager.STREAM_MUSIC, 0, 0)
            if (settings.dndAtEnd) MusicStopper.enableDnd(this)
            if (settings.vibrateAtEnd) vibrate(longArrayOf(0, 60, 120, 60))
            handler.post {
                TimerController.finished(this, msg)
                postDone(msg)
                cancelInternal()
            }
        }.start()
    }

    private fun cancelInternal() {
        handler.removeCallbacks(tick)
        if (!finishing) abortFade()
        if (sensorOn) { getSystemService(SensorManager::class.java).unregisterListener(this); sensorOn = false }
        wakeLock?.let { if (it.isHeld) it.release() }
        ServiceCompat.stopForeground(this, ServiceCompat.STOP_FOREGROUND_REMOVE)
        stopSelf()
    }

    override fun onDestroy() {
        handler.removeCallbacks(tick)
        if (sensorOn) getSystemService(SensorManager::class.java).unregisterListener(this)
        wakeLock?.let { if (it.isHeld) it.release() }
        super.onDestroy()
    }

    private fun acquireWake(ms: Long) {
        val pm = getSystemService(PowerManager::class.java)
        if (wakeLock == null) wakeLock = pm.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "sleepcut:timer").apply { setReferenceCounted(false) }
        wakeLock?.acquire(ms.coerceIn(10_000L, 6 * 60 * 60_000L))
    }

    private fun notifyUpdate() {
        getSystemService(NotificationManager::class.java).notify(NOTIF_ID, buildNotification(this))
    }

    // ───── Secouer pour prolonger ─────

    private fun updateSensor() {
        val want = TimerController.settings.value.shakeToExtend
        val sm = getSystemService(SensorManager::class.java)
        if (want && !sensorOn) {
            sm.getDefaultSensor(Sensor.TYPE_ACCELEROMETER)?.let { sm.registerListener(this, it, SensorManager.SENSOR_DELAY_UI); sensorOn = true }
        } else if (!want && sensorOn) { sm.unregisterListener(this); sensorOn = false }
    }

    override fun onSensorChanged(e: SensorEvent) {
        val g = sqrt(e.values[0] * e.values[0] + e.values[1] * e.values[1] + e.values[2] * e.values[2]) / SensorManager.GRAVITY_EARTH
        if (g < 2.4f) return
        val now = System.currentTimeMillis()
        if (now - lastShake > 900) shakeCount = 0
        lastShake = now
        shakeCount++
        if (shakeCount >= 3) {
            shakeCount = 0
            lastShake = now + 1500
            TimerController.extend(this, 5)
            abortFade()
            vibrate(longArrayOf(0, 40))
            notifyUpdate()
        }
    }

    override fun onAccuracyChanged(sensor: Sensor?, accuracy: Int) {}

    private fun vibrate(pattern: LongArray) {
        val v: Vibrator = if (Build.VERSION.SDK_INT >= 31) getSystemService(VibratorManager::class.java).defaultVibrator else @Suppress("DEPRECATION") getSystemService(Vibrator::class.java)
        v.vibrate(VibrationEffect.createWaveform(pattern, -1))
    }

    private fun postDone(msg: String) {
        val time = DateFormat.getTimeInstance(DateFormat.SHORT).format(Date())
        val restart = PendingIntent.getService(this, 21, Intent(this, TimerService::class.java).setAction(ACTION_EXTEND_15), PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT)
        val n = NotificationCompat.Builder(this, CHANNEL_DONE)
            .setSmallIcon(R.drawable.ic_notif)
            .setContentTitle("Musique coupée à $time 💤")
            .setContentText(msg)
            .setStyle(NotificationCompat.BigTextStyle().bigText(msg))
            .setAutoCancel(true)
            .setSilent(true)
            .setContentIntent(openApp(this))
            .addAction(0, "Relancer 15 min", restart)
            .build()
        getSystemService(NotificationManager::class.java).notify(DONE_ID, n)
    }

    companion object {
        const val ACTION_START = "sleepcut.start"
        const val ACTION_UPDATE = "sleepcut.update"
        const val ACTION_CANCEL = "sleepcut.cancel"
        const val ACTION_CANCEL_NOTIF = "sleepcut.cancel_notif"
        const val ACTION_EXTEND_5 = "sleepcut.extend5"
        const val ACTION_EXTEND_15 = "sleepcut.extend15"
        const val ACTION_WAKE = "sleepcut.wake"
        const val CHANNEL = "timer"
        const val CHANNEL_DONE = "done"
        const val NOTIF_ID = 1
        const val DONE_ID = 2

        fun createChannels(ctx: Context) {
            val nm = ctx.getSystemService(NotificationManager::class.java)
            nm.createNotificationChannel(NotificationChannel(CHANNEL, "Minuteur en cours", NotificationManager.IMPORTANCE_LOW).apply { setShowBadge(false) })
            nm.createNotificationChannel(NotificationChannel(CHANNEL_DONE, "Musique coupée", NotificationManager.IMPORTANCE_LOW))
        }

        fun openApp(ctx: Context): PendingIntent = PendingIntent.getActivity(ctx, 0, Intent(ctx, MainActivity::class.java).addFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP), PendingIntent.FLAG_IMMUTABLE)

        private fun action(ctx: Context, a: String, code: Int) =
            PendingIntent.getService(ctx, code, Intent(ctx, TimerService::class.java).setAction(a), PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT)

        fun buildNotification(ctx: Context): android.app.Notification {
            val st = TimerController.state.value
            val time = if (st.endAt > 0) DateFormat.getTimeInstance(DateFormat.SHORT).format(Date(st.endAt)) else ""
            return NotificationCompat.Builder(ctx, CHANNEL)
                .setSmallIcon(R.drawable.ic_notif)
                .setContentTitle(if (st.fading) "Fondu en cours… bonne nuit 🌙" else "Coupure de la musique à $time")
                .setContentText(if (TimerController.settings.value.shakeToExtend) "Secoue le téléphone pour +5 min" else "Touchez pour modifier")
                .setUsesChronometer(true)
                .setChronometerCountDown(true)
                .setWhen(st.endAt)
                .setShowWhen(true)
                .setOngoing(true)
                .setOnlyAlertOnce(true)
                .setSilent(true)
                .setCategory(NotificationCompat.CATEGORY_STOPWATCH)
                .setVisibility(NotificationCompat.VISIBILITY_PUBLIC)
                .setContentIntent(openApp(ctx))
                .addAction(0, "+5 min", action(ctx, ACTION_EXTEND_5, 11))
                .addAction(0, "+15 min", action(ctx, ACTION_EXTEND_15, 12))
                .addAction(0, "Annuler", action(ctx, ACTION_CANCEL_NOTIF, 13))
                .build()
        }
    }
}
