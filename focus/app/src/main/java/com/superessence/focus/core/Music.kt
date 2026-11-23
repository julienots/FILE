package com.superessence.focus.core

import android.app.Notification
import android.app.PendingIntent
import android.app.Service
import android.content.Context
import android.content.Intent
import android.content.pm.ServiceInfo
import android.media.AudioAttributes
import android.media.MediaPlayer
import android.os.Build
import android.os.Handler
import android.os.IBinder
import android.os.Looper
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.core.app.NotificationCompat
import androidx.core.content.ContextCompat
import com.superessence.focus.FocusApp
import com.superessence.focus.MainActivity
import com.superessence.focus.R

data class Sound(val id: String, val name: String, val emoji: String)

/** Musique douce : une boucle musicale + une ambiance naturelle, mixées avec fondu. */
object Music {
    val tracks = listOf(
        Sound("piano", "Piano doux", "🎹"),
        Sound("ambient", "Ambient", "🌙"),
        Sound("pads", "Nappes calmes", "☁️"),
        Sound("space", "Cosmos", "✨"),
        Sound("none", "Aucune", "🔇"),
    )
    val natures = listOf(
        Sound("rain_soft", "Pluie", "🌧️"),
        Sound("stream", "Ruisseau", "💧"),
        Sound("waves", "Vagues", "🌊"),
        Sound("fire", "Feu de bois", "🔥"),
        Sound("brown", "Bruit brun", "🟤"),
        Sound("none", "Aucune", "🔇"),
    )

    var playing by mutableStateOf(false); private set
    private var music: MediaPlayer? = null
    private var nature: MediaPlayer? = null
    private var fade = 0f
    private val h = Handler(Looper.getMainLooper())

    private fun player(c: Context, id: String): MediaPlayer? {
        if (id == "none") return null
        return runCatching {
            val afd = c.assets.openFd("sounds/$id.ogg")
            MediaPlayer().apply {
                setAudioAttributes(AudioAttributes.Builder().setUsage(AudioAttributes.USAGE_MEDIA).setContentType(AudioAttributes.CONTENT_TYPE_MUSIC).build())
                setDataSource(afd.fileDescriptor, afd.startOffset, afd.length)
                isLooping = true
                prepare()
                setVolume(0f, 0f)
            }
        }.getOrNull()
    }

    private fun applyVolume() {
        val m = Store.musicVol * fade
        val n = Store.natureVol * fade
        runCatching { music?.setVolume(m, m) }
        runCatching { nature?.setVolume(n, n) }
    }

    fun play(c: Context) {
        val app = c.applicationContext
        if (music == null) music = player(app, Store.track)
        if (nature == null) nature = player(app, Store.nature)
        music?.start(); nature?.start()
        playing = true
        ContextCompat.startForegroundService(app, Intent(app, MusicService::class.java))
        fadeTo(1f) {}
    }

    fun pause(c: Context) {
        val app = c.applicationContext
        playing = false
        fadeTo(0f) {
            runCatching { music?.pause() }; runCatching { nature?.pause() }
        }
        app.stopService(Intent(app, MusicService::class.java))
    }

    fun toggle(c: Context) = if (playing) pause(c) else play(c)

    fun select(c: Context, trackId: String? = null, natureId: String? = null) {
        if (trackId != null) { Store.track = trackId; music?.release(); music = null }
        if (natureId != null) { Store.nature = natureId; nature?.release(); nature = null }
        if (playing) {
            val app = c.applicationContext
            if (music == null) music = player(app, Store.track)?.also { it.start() }
            if (nature == null) nature = player(app, Store.nature)?.also { it.start() }
            applyVolume()
        }
    }

    fun setVolumes(m: Float? = null, n: Float? = null) {
        m?.let { Store.musicVol = it }; n?.let { Store.natureVol = it }
        applyVolume()
    }

    private fun fadeTo(target: Float, done: () -> Unit) {
        h.removeCallbacksAndMessages(null)
        val step = object : Runnable {
            override fun run() {
                fade += if (target > fade) 0.05f else -0.08f
                if ((target >= 1f && fade >= 1f) || (target <= 0f && fade <= 0f)) {
                    fade = target; applyVolume(); done()
                } else { applyVolume(); h.postDelayed(this, 60) }
            }
        }
        h.post(step)
    }
}

class MusicService : Service() {
    override fun onBind(i: Intent?): IBinder? = null

    override fun onStartCommand(i: Intent?, flags: Int, startId: Int): Int {
        if (i?.action == "pause") { Music.pause(this); return START_NOT_STICKY }
        val open = PendingIntent.getActivity(this, 0, Intent(this, MainActivity::class.java), PendingIntent.FLAG_IMMUTABLE)
        val pause = PendingIntent.getService(this, 1, Intent(this, MusicService::class.java).setAction("pause"), PendingIntent.FLAG_IMMUTABLE)
        val name = Music.tracks.firstOrNull { it.id == Store.track }?.name ?: "Musique"
        val n: Notification = NotificationCompat.Builder(this, FocusApp.CH_MUSIC)
            .setSmallIcon(R.drawable.ic_stat)
            .setContentTitle("Musique douce")
            .setContentText(name)
            .setOngoing(true)
            .setContentIntent(open)
            .addAction(0, "Pause", pause)
            .build()
        if (Build.VERSION.SDK_INT >= 29) startForeground(1, n, ServiceInfo.FOREGROUND_SERVICE_TYPE_MEDIA_PLAYBACK)
        else startForeground(1, n)
        return START_NOT_STICKY
    }
}
