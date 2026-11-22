package com.superessence.sleepcut

import android.app.NotificationManager
import android.content.ComponentName
import android.content.Context
import android.media.AudioAttributes
import android.media.AudioFocusRequest
import android.media.AudioManager
import android.media.session.MediaController
import android.media.session.MediaSessionManager
import android.media.session.PlaybackState
import android.os.SystemClock
import android.view.KeyEvent
import androidx.core.app.NotificationManagerCompat

/**
 * Coupe la musique de n'importe quelle appli (Spotify, YouTube, Deezer, Apple Music, podcasts…)
 * en combinant trois méthodes complémentaires.
 */
object MusicStopper {

    class Result(val summary: String, val stillPlaying: Boolean, val apps: List<String>)

    fun hasSessionAccess(ctx: Context): Boolean =
        NotificationManagerCompat.getEnabledListenerPackages(ctx).contains(ctx.packageName)

    fun activeControllers(ctx: Context): List<MediaController> {
        if (!hasSessionAccess(ctx)) return emptyList()
        return runCatching {
            ctx.getSystemService(MediaSessionManager::class.java).getActiveSessions(ComponentName(ctx, MediaListener::class.java))
        }.getOrDefault(emptyList())
    }

    fun appLabel(ctx: Context, pkg: String): String = runCatching {
        val pm = ctx.packageManager
        pm.getApplicationLabel(pm.getApplicationInfo(pkg, 0)).toString()
    }.getOrDefault(pkg)

    /** Ce qui joue en ce moment (si l'accès aux sessions est accordé). */
    fun nowPlaying(ctx: Context): List<Pair<String, String>> = activeControllers(ctx)
        .filter { it.playbackState?.state == PlaybackState.STATE_PLAYING }
        .map { c ->
            val md = c.metadata
            val title = md?.getString(android.media.MediaMetadata.METADATA_KEY_TITLE) ?: ""
            val artist = md?.getString(android.media.MediaMetadata.METADATA_KEY_ARTIST) ?: ""
            appLabel(ctx, c.packageName) to listOf(title, artist).filter { it.isNotBlank() }.joinToString(" — ")
        }

    /** Millisecondes restantes du morceau en cours (ou null si inconnu). */
    fun remainingOfCurrentTrack(ctx: Context): Long? {
        val c = activeControllers(ctx).firstOrNull { it.playbackState?.state == PlaybackState.STATE_PLAYING } ?: return null
        val dur = c.metadata?.getLong(android.media.MediaMetadata.METADATA_KEY_DURATION) ?: return null
        val ps = c.playbackState ?: return null
        if (dur <= 0) return null
        val elapsed = SystemClock.elapsedRealtime() - ps.lastPositionUpdateTime
        val pos = ps.position + (elapsed * ps.playbackSpeed).toLong()
        return (dur - pos).takeIf { it > 0 }
    }

    fun stopEverything(ctx: Context): Result {
        val am = ctx.getSystemService(AudioManager::class.java)
        val stopped = LinkedHashSet<String>()

        // 1) Commande « pause » directe à chaque lecteur (nécessite l'accès aux notifications)
        for (c in activeControllers(ctx)) {
            val st = c.playbackState?.state
            if (st == PlaybackState.STATE_PLAYING || st == PlaybackState.STATE_BUFFERING || st == PlaybackState.STATE_FAST_FORWARDING) {
                runCatching { c.transportControls.pause() }
                stopped += appLabel(ctx, c.packageName)
            }
        }

        // 2) Prise du focus audio : les applis bien faites se mettent en pause définitivement
        val attrs = AudioAttributes.Builder().setUsage(AudioAttributes.USAGE_MEDIA).setContentType(AudioAttributes.CONTENT_TYPE_MUSIC).build()
        val req = AudioFocusRequest.Builder(AudioManager.AUDIOFOCUS_GAIN).setAudioAttributes(attrs).setOnAudioFocusChangeListener { }.build()
        val granted = am.requestAudioFocus(req) == AudioManager.AUDIOFOCUS_REQUEST_GRANTED

        // 3) Touche média « pause » (comme un casque Bluetooth)
        if (am.isMusicActive) {
            val t = SystemClock.uptimeMillis()
            runCatching {
                am.dispatchMediaKeyEvent(KeyEvent(t, t, KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_MEDIA_PAUSE, 0))
                am.dispatchMediaKeyEvent(KeyEvent(t, t, KeyEvent.ACTION_UP, KeyEvent.KEYCODE_MEDIA_PAUSE, 0))
            }
        }

        Thread.sleep(2500)
        // seconde passe pour les lecteurs récalcitrants
        for (c in activeControllers(ctx)) if (c.playbackState?.state == PlaybackState.STATE_PLAYING) {
            runCatching { c.transportControls.stop() }
            stopped += appLabel(ctx, c.packageName)
        }
        Thread.sleep(800)
        val still = am.isMusicActive
        if (granted) am.abandonAudioFocusRequest(req)

        val summary = when {
            stopped.isNotEmpty() -> "En pause : ${stopped.joinToString(", ")}."
            !still -> "La lecture a été arrêtée."
            else -> "Une application ignore les commandes."
        }
        return Result(summary, still, stopped.toList())
    }

    fun hasDndAccess(ctx: Context) = ctx.getSystemService(NotificationManager::class.java).isNotificationPolicyAccessGranted

    fun enableDnd(ctx: Context) {
        val nm = ctx.getSystemService(NotificationManager::class.java)
        if (nm.isNotificationPolicyAccessGranted) runCatching { nm.setInterruptionFilter(NotificationManager.INTERRUPTION_FILTER_PRIORITY) }
    }
}
