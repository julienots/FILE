package com.superessence.pleinphare.playback

import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import androidx.media3.common.AudioAttributes
import androidx.media3.common.C
import androidx.media3.common.MediaItem
import androidx.media3.common.MediaMetadata
import androidx.media3.common.Player
import androidx.media3.exoplayer.ExoPlayer
import androidx.media3.session.LibraryResult
import androidx.media3.session.MediaLibraryService
import androidx.media3.session.MediaSession
import androidx.media3.session.MediaSession.MediaItemsWithStartPosition
import com.google.common.collect.ImmutableList
import com.google.common.util.concurrent.Futures
import com.google.common.util.concurrent.ListenableFuture
import com.superessence.pleinphare.MainActivity
import com.superessence.pleinphare.data.MusicLibrary
import com.superessence.pleinphare.data.Song
import com.superessence.pleinphare.data.day
import com.superessence.pleinphare.data.monthLabel

/**
 * Service de lecture : continue en arrière-plan et écran éteint, commandes sur l'écran
 * verrouillé, Bluetooth / commandes au volant, et navigation dans Android Auto.
 */
class PlaybackService : MediaLibraryService() {

    private lateinit var player: ExoPlayer
    private var session: MediaLibrarySession? = null

    override fun onCreate() {
        super.onCreate()
        player = ExoPlayer.Builder(this)
            .setAudioAttributes(
                AudioAttributes.Builder().setUsage(C.USAGE_MEDIA).setContentType(C.AUDIO_CONTENT_TYPE_MUSIC).build(),
                /* handleAudioFocus = */ true,
            )
            .setHandleAudioBecomingNoisy(true) // pause quand le Bluetooth de la voiture se déconnecte
            .setWakeMode(C.WAKE_MODE_LOCAL)
            .build()
        player.addListener(object : Player.Listener {
            override fun onMediaItemTransition(mediaItem: MediaItem?, reason: Int) = saveState()
            override fun onIsPlayingChanged(isPlaying: Boolean) { if (!isPlaying) saveState() }
            override fun onTimelineChanged(timeline: androidx.media3.common.Timeline, reason: Int) {
                if (reason == Player.TIMELINE_CHANGE_REASON_PLAYLIST_CHANGED) saveState()
            }
        })
        val open = PendingIntent.getActivity(
            this, 0, Intent(this, MainActivity::class.java),
            PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT,
        )
        session = MediaLibrarySession.Builder(this, player, LibraryCallback())
            .setSessionActivity(open)
            .build()
        restoreState()
    }

    override fun onGetSession(controllerInfo: MediaSession.ControllerInfo): MediaLibrarySession? = session

    override fun onTaskRemoved(rootIntent: Intent?) {
        if (!isPlaybackOngoing) pauseAllPlayersAndStopSelf()
    }

    override fun onDestroy() {
        saveState()
        session?.run { player.release(); release() }
        session = null
        super.onDestroy()
    }

    // ------------------------------------------------------------------ mémoire de la file
    private fun saveState() {
        if (!::player.isInitialized) return
        val ids = (0 until player.mediaItemCount).joinToString(",") { player.getMediaItemAt(it).mediaId }
        QueueStore.save(this, ids, player.currentMediaItemIndex, player.currentPosition, player.shuffleModeEnabled, player.repeatMode)
    }

    private fun restoreState() {
        val saved = QueueStore.load(this) ?: return
        val items = resolveIds(saved.ids)
        if (items.isEmpty()) return
        val index = saved.index.coerceIn(0, items.lastIndex)
        player.shuffleModeEnabled = saved.shuffle
        player.repeatMode = saved.repeat
        player.setMediaItems(items, index, saved.positionMs)
        player.prepare()
    }

    private fun resolveIds(ids: List<String>): List<MediaItem> {
        val byId = MusicLibrary.get(this).associateBy { it.id.toString() }
        return ids.mapNotNull { byId[it]?.toMediaItem() }
    }

    // ------------------------------------------------------------------ Android Auto & manettes
    private inner class LibraryCallback : MediaLibrarySession.Callback {

        private fun songs(): List<Song> = MusicLibrary.get(this@PlaybackService)

        override fun onGetLibraryRoot(
            session: MediaLibrarySession, browser: MediaSession.ControllerInfo, params: LibraryParams?,
        ): ListenableFuture<LibraryResult<MediaItem>> =
            Futures.immediateFuture(LibraryResult.ofItem(folder(ROOT, "Plein Phare"), params))

        override fun onGetChildren(
            session: MediaLibrarySession, browser: MediaSession.ControllerInfo, parentId: String,
            page: Int, pageSize: Int, params: LibraryParams?,
        ): ListenableFuture<LibraryResult<ImmutableList<MediaItem>>> {
            val all = songs()
            val children: List<MediaItem> = when {
                parentId == ROOT -> listOf(
                    playable(SHUFFLE_ALL, "Lecture aléatoire", "${all.size} titres"),
                    folder(RECENT, "Par date · les plus récentes"),
                    folder(MONTHS, "Par mois"),
                )
                parentId == RECENT -> all.map { it.toMediaItem() }
                parentId == MONTHS -> all.groupBy { monthKey(it) }.map { (key, list) ->
                    folder(MONTH_PREFIX + key, monthLabel(list.first().day()), "${list.size} titres")
                }
                parentId.startsWith(MONTH_PREFIX) -> {
                    val key = parentId.removePrefix(MONTH_PREFIX)
                    all.filter { monthKey(it) == key }.map { it.toMediaItem() }
                }
                else -> emptyList()
            }
            val from = (page.coerceAtLeast(0).toLong() * pageSize).coerceAtMost(children.size.toLong()).toInt()
            val to = (from.toLong() + pageSize).coerceAtMost(children.size.toLong()).toInt()
            return Futures.immediateFuture(LibraryResult.ofItemList(ImmutableList.copyOf(children.subList(from, to)), params))
        }

        override fun onGetItem(
            session: MediaLibrarySession, browser: MediaSession.ControllerInfo, mediaId: String,
        ): ListenableFuture<LibraryResult<MediaItem>> {
            val item = mediaId.toLongOrNull()?.let { MusicLibrary.find(this@PlaybackService, it)?.toMediaItem() }
            return Futures.immediateFuture(
                if (item != null) LibraryResult.ofItem(item, null) else LibraryResult.ofError(LibraryResult.RESULT_ERROR_BAD_VALUE),
            )
        }

        override fun onAddMediaItems(
            mediaSession: MediaSession, controller: MediaSession.ControllerInfo, mediaItems: MutableList<MediaItem>,
        ): ListenableFuture<MutableList<MediaItem>> =
            Futures.immediateFuture(resolveIds(mediaItems.map { it.mediaId }).toMutableList())

        override fun onSetMediaItems(
            mediaSession: MediaSession, controller: MediaSession.ControllerInfo, mediaItems: MutableList<MediaItem>,
            startIndex: Int, startPositionMs: Long,
        ): ListenableFuture<MediaItemsWithStartPosition> {
            val all = songs()
            // Un seul élément choisi (Android Auto, assistant) : on lance toute la liste à partir de lui.
            if (mediaItems.size == 1) {
                val id = mediaItems[0].mediaId
                if (id == SHUFFLE_ALL) {
                    player.shuffleModeEnabled = true
                    val items = all.map { it.toMediaItem() }
                    val start = if (items.isEmpty()) 0 else items.indices.random()
                    return Futures.immediateFuture(MediaItemsWithStartPosition(items, start, 0))
                }
                val idx = all.indexOfFirst { it.id.toString() == id }
                if (idx >= 0) {
                    return Futures.immediateFuture(
                        MediaItemsWithStartPosition(all.map { it.toMediaItem() }, idx, startPositionMs.coerceAtLeast(0)),
                    )
                }
            }
            val resolved = resolveIds(mediaItems.map { it.mediaId })
            val start = if (startIndex == C.INDEX_UNSET) 0 else startIndex.coerceIn(0, (resolved.size - 1).coerceAtLeast(0))
            val pos = if (startPositionMs == C.TIME_UNSET) 0 else startPositionMs
            return Futures.immediateFuture(MediaItemsWithStartPosition(resolved, start, pos))
        }

        // Bouton « lecture » de la voiture alors que l'appli était fermée : reprise de la dernière écoute.
        override fun onPlaybackResumption(
            mediaSession: MediaSession, controller: MediaSession.ControllerInfo,
        ): ListenableFuture<MediaItemsWithStartPosition> {
            val saved = QueueStore.load(this@PlaybackService)
            val items = saved?.let { resolveIds(it.ids) }.orEmpty()
            return if (saved != null && items.isNotEmpty()) {
                Futures.immediateFuture(MediaItemsWithStartPosition(items, saved.index.coerceIn(0, items.lastIndex), saved.positionMs))
            } else {
                val all = songs().map { it.toMediaItem() }
                Futures.immediateFuture(MediaItemsWithStartPosition(all, 0, 0))
            }
        }
    }

    private fun monthKey(s: Song) = s.day().let { "%04d-%02d".format(it.year, it.monthValue) }

    private fun folder(id: String, title: String, subtitle: String? = null) = MediaItem.Builder()
        .setMediaId(id)
        .setMediaMetadata(
            MediaMetadata.Builder().setTitle(title).setSubtitle(subtitle)
                .setIsBrowsable(true).setIsPlayable(false)
                .setMediaType(MediaMetadata.MEDIA_TYPE_FOLDER_MIXED).build(),
        ).build()

    private fun playable(id: String, title: String, subtitle: String) = MediaItem.Builder()
        .setMediaId(id)
        .setMediaMetadata(
            MediaMetadata.Builder().setTitle(title).setSubtitle(subtitle)
                .setIsBrowsable(false).setIsPlayable(true)
                .setMediaType(MediaMetadata.MEDIA_TYPE_PLAYLIST).build(),
        ).build()

    companion object {
        const val ROOT = "root"
        const val RECENT = "recent"
        const val MONTHS = "months"
        const val MONTH_PREFIX = "month:"
        const val SHUFFLE_ALL = "shuffle_all"
    }
}

/** Dernière file de lecture, pour reprendre là où on s'était arrêté. */
object QueueStore {
    data class Saved(val ids: List<String>, val index: Int, val positionMs: Long, val shuffle: Boolean, val repeat: Int)

    private fun prefs(ctx: Context) = ctx.getSharedPreferences("queue", Context.MODE_PRIVATE)

    fun save(ctx: Context, ids: String, index: Int, positionMs: Long, shuffle: Boolean, repeat: Int) {
        prefs(ctx).edit()
            .putString("ids", ids).putInt("index", index).putLong("pos", positionMs.coerceAtLeast(0))
            .putBoolean("shuffle", shuffle).putInt("repeat", repeat)
            .apply()
    }

    fun load(ctx: Context): Saved? {
        val p = prefs(ctx)
        val ids = p.getString("ids", null)?.split(',')?.filter { it.isNotBlank() }.orEmpty()
        if (ids.isEmpty()) return null
        return Saved(ids, p.getInt("index", 0), p.getLong("pos", 0), p.getBoolean("shuffle", false), p.getInt("repeat", Player.REPEAT_MODE_ALL))
    }
}
