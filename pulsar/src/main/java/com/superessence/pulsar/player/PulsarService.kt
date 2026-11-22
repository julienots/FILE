package com.superessence.pulsar.player

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
import android.os.PowerManager
import android.support.v4.media.MediaMetadataCompat
import android.support.v4.media.session.MediaSessionCompat
import android.support.v4.media.session.PlaybackStateCompat
import androidx.core.app.NotificationCompat
import androidx.core.app.ServiceCompat
import androidx.core.content.ContextCompat
import com.superessence.pulsar.MainActivity
import com.superessence.pulsar.R

/**
 * Service de premier plan : garde la génération audio active écran éteint,
 * affiche la notification multimédia et les commandes de l'écran verrouillé / Bluetooth.
 */
class PulsarService : Service() {

    private lateinit var session: MediaSessionCompat
    private var wakeLock: PowerManager.WakeLock? = null

    override fun onCreate() {
        super.onCreate()
        Player.init(this)
        createChannel()
        session = MediaSessionCompat(this, "Pulsar").apply {
            setCallback(object : MediaSessionCompat.Callback() {
                override fun onPlay() = Player.play()
                override fun onPause() = Player.pause()
                override fun onSkipToNext() = Player.next()
                override fun onSkipToPrevious() = Player.previous()
                override fun onStop() = Player.stopAll()
            })
            isActive = true
        }
        instance = this
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        when (intent?.action) {
            ACTION_TOGGLE -> Player.toggle()
            ACTION_NEXT -> Player.next()
            ACTION_PREV -> Player.previous()
            ACTION_STOP -> { Player.stopAll(); return START_NOT_STICKY }
        }
        updateNotification(startForeground = true)
        return START_NOT_STICKY
    }

    override fun onBind(intent: Intent?): IBinder? = null

    override fun onTaskRemoved(rootIntent: Intent?) {
        if (!Player.state.value.playing) { Player.stopAll() }
        super.onTaskRemoved(rootIntent)
    }

    override fun onDestroy() {
        instance = null
        session.isActive = false
        session.release()
        wakeLock?.let { if (it.isHeld) it.release() }
        super.onDestroy()
    }

    private fun createChannel() {
        val nm = getSystemService(NotificationManager::class.java)
        if (nm.getNotificationChannel(CHANNEL) == null) {
            nm.createNotificationChannel(NotificationChannel(CHANNEL, "Lecture", NotificationManager.IMPORTANCE_LOW).apply {
                description = "Commandes de lecture de PULSAR"
                setShowBadge(false)
            })
        }
    }

    private fun pending(action: String, code: Int): PendingIntent =
        PendingIntent.getService(this, code, Intent(this, PulsarService::class.java).setAction(action), PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT)

    fun updateNotification(startForeground: Boolean = false) {
        val st = Player.state.value
        val song = st.song
        val playing = st.playing
        val title = song?.title ?: "PULSAR"
        val sub = song?.let { "${it.genre.emoji} ${it.genre.name} · ${it.bpm.toInt()} BPM · ${it.keyName}" } ?: "Générateur de musique"

        session.setMetadata(
            MediaMetadataCompat.Builder()
                .putString(MediaMetadataCompat.METADATA_KEY_TITLE, title)
                .putString(MediaMetadataCompat.METADATA_KEY_ARTIST, "PULSAR · ${song?.genre?.name ?: ""}")
                .putString(MediaMetadataCompat.METADATA_KEY_ALBUM, song?.genre?.name ?: "")
                .putLong(MediaMetadataCompat.METADATA_KEY_DURATION, ((song?.durationSeconds() ?: 0f) * 1000).toLong())
                .build()
        )
        session.setPlaybackState(
            PlaybackStateCompat.Builder()
                .setActions(PlaybackStateCompat.ACTION_PLAY or PlaybackStateCompat.ACTION_PAUSE or PlaybackStateCompat.ACTION_PLAY_PAUSE or
                    PlaybackStateCompat.ACTION_SKIP_TO_NEXT or PlaybackStateCompat.ACTION_SKIP_TO_PREVIOUS or PlaybackStateCompat.ACTION_STOP)
                .setState(if (playing) PlaybackStateCompat.STATE_PLAYING else PlaybackStateCompat.STATE_PAUSED, PlaybackStateCompat.PLAYBACK_POSITION_UNKNOWN, 1f)
                .build()
        )

        val open = PendingIntent.getActivity(this, 0, Intent(this, MainActivity::class.java).addFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP), PendingIntent.FLAG_IMMUTABLE)
        val n: Notification = NotificationCompat.Builder(this, CHANNEL)
            .setSmallIcon(R.drawable.ic_notif)
            .setContentTitle(title)
            .setContentText(sub)
            .setContentIntent(open)
            .setDeleteIntent(pending(ACTION_STOP, 9))
            .setOngoing(playing)
            .setOnlyAlertOnce(true)
            .setVisibility(NotificationCompat.VISIBILITY_PUBLIC)
            .setColor(song?.genre?.colorA?.toInt() ?: 0xFF7B2FF7.toInt())
            .addAction(R.drawable.ic_prev, "Précédent", pending(ACTION_PREV, 1))
            .addAction(if (playing) R.drawable.ic_pause else R.drawable.ic_play, if (playing) "Pause" else "Lecture", pending(ACTION_TOGGLE, 2))
            .addAction(R.drawable.ic_next, "Nouveau morceau", pending(ACTION_NEXT, 3))
            .addAction(R.drawable.ic_close, "Arrêter", pending(ACTION_STOP, 4))
            .setStyle(androidx.media.app.NotificationCompat.MediaStyle().setMediaSession(session.sessionToken).setShowActionsInCompactView(0, 1, 2))
            .build()

        if (startForeground || playing) {
            runCatching { ServiceCompat.startForeground(this, NOTIF_ID, n, if (Build.VERSION.SDK_INT >= 29) ServiceInfo.FOREGROUND_SERVICE_TYPE_MEDIA_PLAYBACK else 0) }
                .onFailure { getSystemService(NotificationManager::class.java).notify(NOTIF_ID, n) }
        } else {
            getSystemService(NotificationManager::class.java).notify(NOTIF_ID, n)
        }
        if (!playing) ServiceCompat.stopForeground(this, ServiceCompat.STOP_FOREGROUND_DETACH)

        val pm = getSystemService(PowerManager::class.java)
        if (playing) {
            if (wakeLock == null) wakeLock = pm.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "pulsar:play").apply { setReferenceCounted(false) }
            wakeLock?.acquire(6 * 60 * 60 * 1000L)
        } else wakeLock?.let { if (it.isHeld) it.release() }
    }

    companion object {
        private const val CHANNEL = "pulsar_playback"
        private const val NOTIF_ID = 42
        const val ACTION_TOGGLE = "pulsar.toggle"
        const val ACTION_NEXT = "pulsar.next"
        const val ACTION_PREV = "pulsar.prev"
        const val ACTION_STOP = "pulsar.stop"
        @Volatile private var instance: PulsarService? = null

        fun start(ctx: Context) {
            val i = instance
            if (i != null) { i.updateNotification(startForeground = true); return }
            runCatching { ContextCompat.startForegroundService(ctx, Intent(ctx, PulsarService::class.java)) }
        }

        fun refresh(ctx: Context) { instance?.updateNotification() }

        fun stop(ctx: Context) {
            instance?.let {
                ServiceCompat.stopForeground(it, ServiceCompat.STOP_FOREGROUND_REMOVE)
                it.stopSelf()
            }
        }
    }
}
