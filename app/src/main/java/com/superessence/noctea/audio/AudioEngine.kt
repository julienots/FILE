package com.superessence.noctea.audio

import android.content.Context
import android.content.Intent
import androidx.annotation.OptIn
import androidx.core.content.ContextCompat
import androidx.media3.common.AudioAttributes
import androidx.media3.common.C
import androidx.media3.common.MediaItem
import androidx.media3.common.MediaMetadata
import androidx.media3.common.PlaybackException
import androidx.media3.common.Player
import androidx.media3.common.util.UnstableApi
import androidx.media3.exoplayer.ExoPlayer
import com.superessence.noctea.data.AmbienceLayer
import com.superessence.noctea.data.UserStore
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch

data class PlaybackUi(
    val playable: Playable? = null,
    val isPlaying: Boolean = false,
    val isBuffering: Boolean = false,
    val positionMs: Long = 0,
    val durationMs: Long = 0,
    val speed: Float = 1f,
    val voiceVolume: Float = 1f,
    val musicVolume: Float = 0.8f,
    val sleepTimerEndAt: Long = 0,         // horodatage de fin (0 = inactif)
    val sleepAtEndOfSession: Boolean = false,
    val queueIndex: Int = 0,
    val queueSize: Int = 0,
    val error: String? = null,
    val ended: Boolean = false,
)

/**
 * Moteur audio de NOCTÉA (un seul pour toute l'application) :
 * lecteur vocal Media3 + mixeur d'ambiances + minuteur + reprise + historique.
 * Toutes les méthodes s'appellent sur le thread principal.
 */
@OptIn(UnstableApi::class)
class AudioEngine(
    private val context: Context,
    private val store: UserStore,
    private val resolver: PlayableResolver,
    private val scope: CoroutineScope,
) {
    val player: ExoPlayer = ExoPlayer.Builder(context)
        .setMediaSourceFactory(NocteaMediaSourceFactory(context) { resolver })
        .setAudioAttributes(
            AudioAttributes.Builder().setUsage(C.USAGE_MEDIA).setContentType(C.AUDIO_CONTENT_TYPE_SPEECH).build(),
            true,
        )
        .setHandleAudioBecomingNoisy(true)
        .setWakeMode(C.WAKE_MODE_LOCAL)
        .setSeekBackIncrementMs(15_000)
        .setSeekForwardIncrementMs(15_000)
        .build()

    val mixer = AmbienceMixer(context, scope)

    private val _ui = MutableStateFlow(PlaybackUi())
    val ui: StateFlow<PlaybackUi> = _ui.asStateFlow()

    private var tickJob: Job? = null
    private var timerJob: Job? = null
    private var timerFade = 1f
    private var listenedAccum = 0L
    private var lastTick = 0L
    private var lastSave = 0L
    private var currentId: String? = null

    init {
        val s = store.value.settings
        mixer.master = s.musicVolume
        player.volume = s.voiceVolume
        player.setPlaybackSpeed(s.playbackSpeed)
        _ui.update { it.copy(voiceVolume = s.voiceVolume, musicVolume = s.musicVolume, speed = s.playbackSpeed) }

        player.addListener(object : Player.Listener {
            override fun onIsPlayingChanged(isPlaying: Boolean) {
                if (isPlaying) {
                    mixer.play()
                    startTicker()
                } else {
                    if (player.playbackState != Player.STATE_BUFFERING) mixer.pause()
                    flushListened()
                    savePosition()
                }
                refresh()
            }

            override fun onMediaItemTransition(mediaItem: MediaItem?, reason: Int) {
                flushListened()
                val id = mediaItem?.mediaId
                currentId = id
                val playable = id?.let { resolver.resolve(it) }
                if (playable != null) {
                    mixer.setLayers(playable.ambience)
                    store.addHistory(playable.mediaId, playable.title)
                    if (playable.kind == PlayableKind.MIXER) {
                        // le mixeur suit le dernier mélange de l'utilisateur
                    }
                }
                _ui.update { it.copy(playable = playable, ended = false, error = null) }
                refresh()
            }

            override fun onPlaybackStateChanged(state: Int) {
                if (state == Player.STATE_ENDED) {
                    currentId?.let { store.savePosition(it, 0) }
                    mixer.pause(4_000)
                    flushListened()
                    _ui.update { it.copy(ended = true) }
                    if (_ui.value.sleepAtEndOfSession) cancelSleepTimer()
                }
                refresh()
            }

            override fun onPlayerError(error: PlaybackException) {
                _ui.update { it.copy(error = "Lecture impossible : ${error.errorCodeName}") }
            }
        })
    }

    // ------------------------------------------------------------------ commandes
    /** Lance une séance (ou une file : rituel). Reprend là où l'on s'était arrêté si pertinent. */
    fun play(mediaIds: List<String>, startIndex: Int = 0, resume: Boolean = true) {
        if (mediaIds.isEmpty()) return
        ensureService()
        val items = mediaIds.mapNotNull { id ->
            val p = resolver.resolve(id) ?: return@mapNotNull null
            MediaItem.Builder()
                .setMediaId(id)
                .setMediaMetadata(
                    MediaMetadata.Builder()
                        .setTitle(p.title)
                        .setArtist("NOCTÉA · ${p.subtitle}")
                        .setAlbumTitle("NOCTÉA — Superessence Studio")
                        .setDurationMs(p.durationMs)
                        .build(),
                )
                .build()
        }
        if (items.isEmpty()) return
        val first = items[startIndex.coerceIn(0, items.size - 1)].mediaId
        val saved = store.value.positions[first] ?: 0L
        val dur = resolver.resolve(first)?.durationMs ?: 0L
        val startPos = if (resume && saved in 15_000 until (dur - 20_000)) saved else 0L
        timerFade = 1f
        applyVoiceVolume()
        player.setMediaItems(items, startIndex.coerceIn(0, items.size - 1), startPos)
        player.prepare()
        player.play()
        if (store.value.settings.defaultSleepTimerMin > 0 && _ui.value.sleepTimerEndAt == 0L) {
            setSleepTimer(store.value.settings.defaultSleepTimerMin)
        }
    }

    fun play(mediaId: String, resume: Boolean = true) = play(listOf(mediaId), 0, resume)

    fun toggle() {
        if (player.playbackState == Player.STATE_ENDED) {
            player.seekTo(0); player.play(); return
        }
        if (player.isPlaying) player.pause() else {
            if (player.playbackState == Player.STATE_IDLE) player.prepare()
            player.play()
        }
    }

    fun pause() = player.pause()

    fun seekBy(deltaMs: Long) {
        val d = player.duration.takeIf { it != C.TIME_UNSET } ?: _ui.value.durationMs
        player.seekTo((player.currentPosition + deltaMs).coerceIn(0, maxOf(0, d - 500)))
        refresh()
    }

    fun seekTo(ms: Long) { player.seekTo(ms.coerceAtLeast(0)); refresh() }

    fun next() { if (player.hasNextMediaItem()) player.seekToNextMediaItem() }

    fun stop() {
        savePosition()
        flushListened()
        player.stop()
        player.clearMediaItems()
        mixer.pause()
        mixer.clear()
        cancelSleepTimer()
        _ui.update { PlaybackUi(voiceVolume = it.voiceVolume, musicVolume = it.musicVolume, speed = it.speed) }
    }

    fun setSpeed(speed: Float) {
        player.setPlaybackSpeed(speed)
        store.settings { it.copy(playbackSpeed = speed) }
        _ui.update { it.copy(speed = speed) }
    }

    fun setVoiceVolume(v: Float) {
        store.settings { it.copy(voiceVolume = v) }
        _ui.update { it.copy(voiceVolume = v) }
        applyVoiceVolume()
    }

    fun setMusicVolume(v: Float) {
        mixer.master = v
        store.settings { it.copy(musicVolume = v) }
        _ui.update { it.copy(musicVolume = v) }
    }

    fun setAmbienceLevel(sound: String, v: Float) {
        mixer.setLevel(sound, v)
        if (_ui.value.playable?.kind == PlayableKind.MIXER) {
            store.edit { s -> s.copy(lastMix = (s.lastMix + (sound to v)).filterValues { it > 0f }) }
        }
    }

    /** Démarre le mixeur seul : un silence porteur garde les commandes sur l'écran verrouillé. */
    fun startMixer(minutes: Int = 600) {
        play(listOf(MediaIds.mixer(minutes)), 0, resume = false)
    }

    fun applyMix(layers: List<AmbienceLayer>) = mixer.setLayers(layers)

    // ------------------------------------------------------------------ minuteur de sommeil
    fun setSleepTimer(minutes: Int) {
        timerJob?.cancel()
        player.pauseAtEndOfMediaItems = false
        if (minutes <= 0) {
            cancelSleepTimer(); return
        }
        val end = System.currentTimeMillis() + minutes * 60_000L
        _ui.update { it.copy(sleepTimerEndAt = end, sleepAtEndOfSession = false) }
        timerJob = scope.launch {
            val fadeMs = 25_000L
            delay(maxOf(0, end - fadeMs - System.currentTimeMillis()))
            mixer.fadeGate(0f, fadeMs)
            val steps = 50
            for (i in 1..steps) {
                timerFade = 1f - i / steps.toFloat()
                applyVoiceVolume()
                delay(fadeMs / steps)
            }
            player.pause()
            timerFade = 1f
            applyVoiceVolume()
            _ui.update { it.copy(sleepTimerEndAt = 0) }
        }
    }

    fun sleepAtEndOfSession() {
        timerJob?.cancel()
        player.pauseAtEndOfMediaItems = true
        _ui.update { it.copy(sleepTimerEndAt = 0, sleepAtEndOfSession = true) }
    }

    fun cancelSleepTimer() {
        timerJob?.cancel()
        timerFade = 1f
        applyVoiceVolume()
        player.pauseAtEndOfMediaItems = false
        _ui.update { it.copy(sleepTimerEndAt = 0, sleepAtEndOfSession = false) }
    }

    // ------------------------------------------------------------------ interne
    private fun applyVoiceVolume() {
        player.volume = (_ui.value.voiceVolume * timerFade).coerceIn(0f, 1f)
    }

    private fun ensureService() {
        try {
            context.startService(Intent(context, PlaybackService::class.java))
        } catch (e: IllegalStateException) {
            // Application en arrière-plan : le contrôleur Media3 démarrera le service.
            ContextCompat.startForegroundService(context, Intent(context, PlaybackService::class.java))
        }
    }

    private fun startTicker() {
        if (tickJob?.isActive == true) return
        lastTick = System.currentTimeMillis()
        tickJob = scope.launch {
            while (isActive) {
                val now = System.currentTimeMillis()
                if (player.isPlaying) {
                    listenedAccum += now - lastTick
                    if (now - lastSave > 10_000) {
                        savePosition(); lastSave = now
                    }
                    if (listenedAccum > 30_000) flushListened()
                }
                lastTick = now
                refresh()
                if (!player.isPlaying && player.playbackState != Player.STATE_BUFFERING) break
                delay(250)
            }
        }
    }

    private fun flushListened() {
        val id = currentId ?: return
        if (listenedAccum > 1_000) store.addListened(id, listenedAccum)
        listenedAccum = 0
    }

    private fun savePosition() {
        val id = currentId ?: return
        val p = _ui.value.playable ?: return
        if (p.kind == PlayableKind.MIXER) return
        val pos = player.currentPosition
        val dur = p.durationMs
        store.savePosition(id, if (pos > dur - 20_000) 0 else pos)
    }

    private fun refresh() {
        val dur = player.duration.takeIf { it != C.TIME_UNSET && it > 0 } ?: (_ui.value.playable?.durationMs ?: 0)
        _ui.update {
            it.copy(
                isPlaying = player.isPlaying,
                isBuffering = player.playbackState == Player.STATE_BUFFERING,
                positionMs = player.currentPosition.coerceAtLeast(0),
                durationMs = dur,
                queueIndex = player.currentMediaItemIndex,
                queueSize = player.mediaItemCount,
            )
        }
    }
}
