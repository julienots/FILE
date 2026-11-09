package com.superessence.noctea.audio

import android.app.PendingIntent
import android.content.Intent
import android.os.Bundle
import androidx.annotation.OptIn
import androidx.media3.common.MediaItem
import androidx.media3.common.Player
import androidx.media3.common.util.UnstableApi
import androidx.media3.session.CommandButton
import androidx.media3.session.DefaultMediaNotificationProvider
import androidx.media3.session.MediaSession
import androidx.media3.session.MediaSessionService
import com.google.common.util.concurrent.Futures
import com.google.common.util.concurrent.ListenableFuture
import com.superessence.noctea.MainActivity
import com.superessence.noctea.NocteaApp
import com.superessence.noctea.R

/**
 * Service de lecture en arrière-plan : notification média, commandes de l'écran verrouillé,
 * casques Bluetooth, lecture écran éteint.
 */
@OptIn(UnstableApi::class)
class PlaybackService : MediaSessionService() {

    private var session: MediaSession? = null

    override fun onCreate() {
        super.onCreate()
        val engine = (application as NocteaApp).engine
        val open = PendingIntent.getActivity(
            this, 0,
            Intent(this, MainActivity::class.java).addFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP)
                .putExtra(MainActivity.EXTRA_OPEN_PLAYER, true),
            PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT,
        )
        val back = CommandButton.Builder(CommandButton.ICON_SKIP_BACK_15)
            .setDisplayName("Reculer de 15 secondes")
            .setPlayerCommand(Player.COMMAND_SEEK_BACK)
            .build()
        val forward = CommandButton.Builder(CommandButton.ICON_SKIP_FORWARD_15)
            .setDisplayName("Avancer de 15 secondes")
            .setPlayerCommand(Player.COMMAND_SEEK_FORWARD)
            .build()
        session = MediaSession.Builder(this, engine.player)
            .setSessionActivity(open)
            .setId("noctea")
            .setCallback(object : MediaSession.Callback {
                override fun onAddMediaItems(
                    mediaSession: MediaSession,
                    controller: MediaSession.ControllerInfo,
                    mediaItems: MutableList<MediaItem>,
                ): ListenableFuture<MutableList<MediaItem>> {
                    // Nos identifiants (track:…, custom:…) sont résolus par NocteaMediaSourceFactory.
                    return Futures.immediateFuture(mediaItems)
                }
            })
            .setMediaButtonPreferences(listOf(back, forward))
            .build()
        setMediaNotificationProvider(
            DefaultMediaNotificationProvider.Builder(this).build().apply {
                setSmallIcon(R.drawable.ic_notification)
            },
        )
    }

    override fun onGetSession(controllerInfo: MediaSession.ControllerInfo): MediaSession? = session

    override fun onTaskRemoved(rootIntent: Intent?) {
        val player = session?.player
        if (player == null || !player.playWhenReady || player.mediaItemCount == 0) {
            stopSelf()
        }
    }

    override fun onDestroy() {
        // Le lecteur appartient au moteur (application) : on ne libère que la session.
        session?.release()
        session = null
        super.onDestroy()
    }

    @Suppress("unused")
    private fun unused(b: Bundle?) = b
}
