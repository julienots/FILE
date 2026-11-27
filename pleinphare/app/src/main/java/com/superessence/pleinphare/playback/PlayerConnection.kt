package com.superessence.pleinphare.playback

import android.content.ComponentName
import android.content.Context
import android.net.Uri
import android.os.Handler
import android.os.Looper
import androidx.core.content.ContextCompat
import androidx.media3.common.Player
import androidx.media3.session.MediaController
import androidx.media3.session.SessionToken
import com.google.common.util.concurrent.ListenableFuture
import com.superessence.pleinphare.data.Song
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow

data class PlayerUi(
    val connected: Boolean = false,
    val hasMedia: Boolean = false,
    val currentId: Long? = null,
    val title: String = "",
    val artist: String = "",
    val artworkUri: Uri? = null,
    val isPlaying: Boolean = false,
    val positionMs: Long = 0,
    val durationMs: Long = 0,
    val shuffle: Boolean = false,
    val repeatMode: Int = Player.REPEAT_MODE_ALL,
    val index: Int = 0,
    val count: Int = 0,
)

/** Lien entre l'interface et le service de lecture. */
class PlayerConnection(private val context: Context) {
    private val _state = MutableStateFlow(PlayerUi())
    val state: StateFlow<PlayerUi> = _state

    private var future: ListenableFuture<MediaController>? = null
    private var controller: MediaController? = null
    private val handler = Handler(Looper.getMainLooper())
    private val ticker = object : Runnable {
        override fun run() {
            update()
            handler.postDelayed(this, 500)
        }
    }
    private val listener = object : Player.Listener {
        override fun onEvents(player: Player, events: Player.Events) = update()
    }

    fun connect() {
        if (future != null) return
        val token = SessionToken(context, ComponentName(context, PlaybackService::class.java))
        val f = MediaController.Builder(context, token).buildAsync()
        future = f
        f.addListener({
            val c = runCatching { f.get() }.getOrNull() ?: return@addListener
            controller = c
            c.addListener(listener)
            update()
            handler.post(ticker)
        }, ContextCompat.getMainExecutor(context))
    }

    fun release() {
        handler.removeCallbacks(ticker)
        controller?.removeListener(listener)
        future?.let { MediaController.releaseFuture(it) }
        future = null
        controller = null
        _state.value = _state.value.copy(connected = false)
    }

    fun playList(songs: List<Song>, startIndex: Int, shuffle: Boolean = false) {
        val c = controller ?: return
        if (songs.isEmpty()) return
        c.shuffleModeEnabled = shuffle
        if (c.repeatMode == Player.REPEAT_MODE_OFF) c.repeatMode = Player.REPEAT_MODE_ALL
        c.setMediaItems(songs.map { it.toMediaItem() }, startIndex.coerceIn(0, songs.lastIndex), 0)
        c.prepare()
        c.play()
    }

    fun togglePlay() {
        val c = controller ?: return
        if (c.isPlaying) c.pause() else {
            if (c.playbackState == Player.STATE_IDLE) c.prepare()
            if (c.playbackState == Player.STATE_ENDED) c.seekToDefaultPosition(0)
            c.play()
        }
    }

    fun next() = controller?.seekToNext()
    fun previous() = controller?.seekToPrevious()
    fun seekTo(ms: Long) = controller?.seekTo(ms)
    fun seekBy(deltaMs: Long) = controller?.let { it.seekTo((it.currentPosition + deltaMs).coerceAtLeast(0)) }
    fun toggleShuffle() = controller?.let { it.shuffleModeEnabled = !it.shuffleModeEnabled }
    fun cycleRepeat() = controller?.let {
        it.repeatMode = when (it.repeatMode) {
            Player.REPEAT_MODE_ALL -> Player.REPEAT_MODE_ONE
            Player.REPEAT_MODE_ONE -> Player.REPEAT_MODE_OFF
            else -> Player.REPEAT_MODE_ALL
        }
    }

    private fun update() {
        val c = controller ?: return
        val item = c.currentMediaItem
        val md = c.mediaMetadata
        _state.value = PlayerUi(
            connected = true,
            hasMedia = item != null,
            currentId = item?.mediaId?.toLongOrNull(),
            title = md.title?.toString().orEmpty(),
            artist = md.artist?.toString().orEmpty(),
            artworkUri = md.artworkUri,
            isPlaying = c.isPlaying,
            positionMs = c.currentPosition.coerceAtLeast(0),
            durationMs = c.duration.takeIf { it > 0 } ?: 0,
            shuffle = c.shuffleModeEnabled,
            repeatMode = c.repeatMode,
            index = c.currentMediaItemIndex,
            count = c.mediaItemCount,
        )
    }
}
