package com.superessence.pulsar.player

import android.content.Context
import android.content.Intent
import android.media.AudioAttributes
import android.media.AudioFocusRequest
import android.media.AudioFormat
import android.media.AudioManager
import android.media.AudioTrack
import android.os.Build
import android.os.Process
import androidx.core.content.ContextCompat
import com.superessence.pulsar.engine.Composer
import com.superessence.pulsar.engine.Engine
import com.superessence.pulsar.engine.Family
import com.superessence.pulsar.engine.GenParams
import com.superessence.pulsar.engine.Genres
import com.superessence.pulsar.engine.LiveParams
import com.superessence.pulsar.engine.SR
import com.superessence.pulsar.engine.SectionType
import com.superessence.pulsar.engine.Song
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import kotlin.random.Random

/** Mode radio : enchaîne automatiquement des morceaux. */
enum class RadioMode { OFF, GENRE, FAMILY }

data class PlayerState(
    val song: Song? = null,
    val playing: Boolean = false,
    val radio: RadioMode = RadioMode.OFF,
    val radioFamily: Family = Family.CHILL,
    val sleepEndAt: Long = 0L,
    val sleepAtSongEnd: Boolean = false,
    val canGoBack: Boolean = false,
    val exportProgress: Float = -1f,
    val lastExport: String? = null,
    val params: GenParams? = null,
    val generating: Boolean = false,
    val revision: Int = 0,
)

/**
 * Lecteur global : possède le moteur et le thread audio. Le service de premier plan
 * ne sert qu'à garder le processus vivant (écran éteint) et à afficher les commandes.
 */
object Player {
    lateinit var app: Context; private set
    val live = LiveParams()
    val engine = Engine(live)
    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.Main.immediate)
    private val _state = MutableStateFlow(PlayerState())
    val state: StateFlow<PlayerState> = _state.asStateFlow()
    lateinit var store: Store; private set

    private val history = ArrayList<GenParams>()
    private var historyPos = -1

    @Volatile private var audioThread: Thread? = null
    @Volatile private var running = false
    private var watchJob: Job? = null
    private var sleepJob: Job? = null
    private var focusRequest: AudioFocusRequest? = null
    private var userVolumeBeforeFade = -1f

    fun init(context: Context) {
        if (::app.isInitialized) return
        app = context.applicationContext
        store = Store(app)
        store.loadMixer(live)
    }

    // ───────────────────────── Génération ─────────────────────────

    fun newSeed(): Long = Random.nextLong(1L, 0xFFFFFFL)

    /** Charge un code partagé du type « TRAP-1A2B3C ». */
    fun loadCode(code: String): Boolean {
        val parts = code.trim().uppercase().split('-')
        if (parts.size != 2) return false
        val g = Genres.all.firstOrNull { it.id.uppercase() == parts[0] } ?: return false
        val seed = parts[1].toLongOrNull(16) ?: return false
        _state.update { it.copy(radio = RadioMode.OFF) }
        live.bpm = 0f
        generate(GenParams(g.id, seed))
        return true
    }

    fun generate(params: GenParams, autoplay: Boolean = true, addToHistory: Boolean = true) {
        _state.update { it.copy(generating = true) }
        scope.launch {
            val song = withContext(Dispatchers.Default) { Composer.compose(params) }
            if (addToHistory) {
                while (history.size > historyPos + 1) history.removeAt(history.size - 1)
                history += params; historyPos = history.lastIndex
                if (history.size > 50) { history.removeAt(0); historyPos-- }
            }
            engine.queuedSong = null
            engine.load(song)
            store.addRecent(song)
            _state.update { it.copy(song = song, params = params, generating = false, canGoBack = historyPos > 0, revision = it.revision + 1) }
            if (autoplay) play()
            PulsarService.refresh(app)
        }
    }

    fun playGenre(genreId: String, base: GenParams? = null) {
        _state.update { it.copy(radio = RadioMode.OFF) }
        val p = (base ?: GenParams(genreId, newSeed())).copy(genreId = genreId, seed = newSeed())
        live.bpm = 0f
        generate(p)
    }

    fun startRadio(family: Family) {
        _state.update { it.copy(radio = RadioMode.FAMILY, radioFamily = family) }
        val g = Genres.of(family).random()
        live.bpm = 0f
        generate(GenParams(g.id, newSeed()))
    }

    fun setRadio(mode: RadioMode) {
        _state.update { it.copy(radio = mode, radioFamily = it.song?.genre?.family ?: it.radioFamily) }
        if (mode == RadioMode.OFF) engine.queuedSong = null
    }

    /** Morceau suivant : nouvelle graine (même genre, ou genre aléatoire en mode radio famille). */
    fun next() {
        val st = _state.value
        val cur = st.params ?: return
        if (historyPos < history.lastIndex) { historyPos++; generate(history[historyPos], addToHistory = false); return }
        val genre = if (st.radio == RadioMode.FAMILY) Genres.of(st.radioFamily).random().id else cur.genreId
        live.bpm = 0f
        generate(cur.copy(genreId = genre, seed = newSeed(), bpm = if (genre == cur.genreId) cur.bpm else null))
    }

    fun previous() {
        if (historyPos > 0) { historyPos--; live.bpm = 0f; generate(history[historyPos], addToHistory = false) }
        else engine.jumpToBar(0)
    }

    fun regenerate(params: GenParams) { live.bpm = 0f; generate(params) }

    fun loadFavorite(f: Favorite) { _state.update { it.copy(radio = RadioMode.OFF) }; live.bpm = 0f; generate(f.params) }

    // ───────────────────────── Lecture ─────────────────────────

    fun toggle() { if (_state.value.playing) pause() else play() }

    fun play() {
        if (engine.song == null && _state.value.song == null) return
        if (running) return
        if (!requestFocus()) return
        running = true
        _state.update { it.copy(playing = true) }
        PulsarService.start(app)
        audioThread = Thread({ audioLoop() }, "pulsar-audio").apply { priority = Thread.MAX_PRIORITY; start() }
        startWatcher()
    }

    fun pause() {
        running = false
        audioThread?.join(400)
        audioThread = null
        _state.update { it.copy(playing = false) }
        PulsarService.refresh(app)
    }

    fun stopAll() {
        pause()
        abandonFocus()
        cancelSleep()
        PulsarService.stop(app)
    }

    private fun audioLoop() {
        Process.setThreadPriority(Process.THREAD_PRIORITY_URGENT_AUDIO)
        val frames = 1024
        val minBytes = AudioTrack.getMinBufferSize(SR, AudioFormat.CHANNEL_OUT_STEREO, AudioFormat.ENCODING_PCM_FLOAT)
        val bufBytes = maxOf(minBytes * 2, frames * 8 * 4)
        val track = AudioTrack.Builder()
            .setAudioAttributes(AudioAttributes.Builder().setUsage(AudioAttributes.USAGE_MEDIA).setContentType(AudioAttributes.CONTENT_TYPE_MUSIC).build())
            .setAudioFormat(AudioFormat.Builder().setSampleRate(SR).setEncoding(AudioFormat.ENCODING_PCM_FLOAT).setChannelMask(AudioFormat.CHANNEL_OUT_STEREO).build())
            .setBufferSizeInBytes(bufBytes)
            .setTransferMode(AudioTrack.MODE_STREAM)
            .apply { if (Build.VERSION.SDK_INT >= 26) setPerformanceMode(AudioTrack.PERFORMANCE_MODE_POWER_SAVING) }
            .build()
        val buf = FloatArray(frames * 2)
        try {
            track.play()
            // court fondu d'entrée pour éviter les clics
            var fadeIn = 0f
            while (running) {
                engine.render(buf, frames)
                if (fadeIn < 1f) {
                    for (i in 0 until frames) { val g = (fadeIn + i / (frames * 4f)).coerceAtMost(1f); buf[i * 2] *= g; buf[i * 2 + 1] *= g }
                    fadeIn += 0.25f
                }
                track.write(buf, 0, buf.size, AudioTrack.WRITE_BLOCKING)
                if (engine.finished) { running = false; scope.launch { pause() } }
            }
            // fondu de sortie
            engine.render(buf, frames)
            for (i in 0 until frames) { val g = 1f - i / frames.toFloat(); buf[i * 2] *= g; buf[i * 2 + 1] *= g }
            track.write(buf, 0, buf.size, AudioTrack.WRITE_BLOCKING)
            track.stop()
        } catch (_: Throwable) {
        } finally {
            track.release()
        }
    }

    /** Surveille la position : prépare le morceau suivant (radio), suit les changements. */
    private fun startWatcher() {
        watchJob?.cancel()
        watchJob = scope.launch {
            var lastPlayed = engine.songsPlayed
            while (isActive && running) {
                val st = _state.value
                val s = engine.song
                if (s != null) {
                    // le moteur a enchaîné un nouveau morceau
                    if (engine.songsPlayed != lastPlayed) {
                        lastPlayed = engine.songsPlayed
                        if (s !== st.song) {
                            val p = s.params
                            while (history.size > historyPos + 1) history.removeAt(history.size - 1)
                            history += p; historyPos = history.lastIndex
                            store.addRecent(s)
                            _state.update { it.copy(song = s, params = p, canGoBack = historyPos > 0, revision = it.revision + 1) }
                            PulsarService.refresh(app)
                            if (st.sleepAtSongEnd) { scope.launch { pause(); cancelSleep() } }
                        }
                    }
                    val lastSection = s.sections.last()
                    val radio = st.radio != RadioMode.OFF || st.sleepAtSongEnd
                    engine.loopSong = !radio
                    if (radio && !st.sleepAtSongEnd && engine.queuedSong == null && engine.barIndex >= lastSection.startBar) {
                        val genre = if (st.radio == RadioMode.FAMILY) Genres.of(st.radioFamily).random().id else s.genre.id
                        val p = s.params.copy(genreId = genre, seed = newSeed(), bpm = null, root = null, scale = null)
                        engine.queuedSong = withContext(Dispatchers.Default) { Composer.compose(p) }
                    }
                    if (st.sleepAtSongEnd) { engine.loopSong = false; if (engine.finished) { pause(); cancelSleep() } }
                }
                delay(400)
            }
        }
    }

    // ───────────────────────── Minuteur de sommeil ─────────────────────────

    fun startSleep(minutes: Int) {
        cancelSleep()
        val end = System.currentTimeMillis() + minutes * 60_000L
        _state.update { it.copy(sleepEndAt = end, sleepAtSongEnd = false) }
        sleepJob = scope.launch {
            val fadeMs = 30_000L
            userVolumeBeforeFade = live.masterVolume
            while (isActive) {
                val left = end - System.currentTimeMillis()
                if (left <= 0) break
                if (left < fadeMs) live.masterVolume = userVolumeBeforeFade * (left / fadeMs.toFloat())
                delay(250)
            }
            pause()
            live.masterVolume = userVolumeBeforeFade
            _state.update { it.copy(sleepEndAt = 0L) }
        }
    }

    fun sleepAtSongEnd() {
        cancelSleep()
        _state.update { it.copy(sleepAtSongEnd = true) }
    }

    fun cancelSleep() {
        sleepJob?.cancel(); sleepJob = null
        if (userVolumeBeforeFade >= 0f && _state.value.sleepEndAt != 0L) live.masterVolume = userVolumeBeforeFade
        _state.update { it.copy(sleepEndAt = 0L, sleepAtSongEnd = false) }
        engine.loopSong = _state.value.radio == RadioMode.OFF
    }

    // ───────────────────────── Export ─────────────────────────

    fun export() {
        val song = engine.song ?: return
        if (_state.value.exportProgress >= 0f) return
        _state.update { it.copy(exportProgress = 0f, lastExport = null) }
        scope.launch {
            val name = withContext(Dispatchers.Default) {
                Exporter.exportWav(app, song, live) { p -> _state.update { it.copy(exportProgress = p) } }
            }
            _state.update { it.copy(exportProgress = -1f, lastExport = name) }
        }
    }

    fun clearExportMessage() = _state.update { it.copy(lastExport = null) }

    // ───────────────────────── Divers ─────────────────────────

    fun saveMixer() = store.saveMixer(live)

    fun toggleFavorite() {
        val s = _state.value.song ?: return
        store.toggleFavorite(s)
        _state.update { it.copy(revision = it.revision + 1) }
    }

    fun removeFavorite(f: Favorite) {
        store.removeFavorite(f)
        _state.update { it.copy(revision = it.revision + 1) }
    }

    val currentSectionLabel: String get() {
        val s = engine.song ?: return ""
        return s.sections[engine.currentSectionIndex].type.label
    }

    fun isHighEnergy(): Boolean {
        val s = engine.song ?: return false
        val t = s.sections[engine.currentSectionIndex].type
        return t == SectionType.DROP || t == SectionType.CHORUS
    }

    // ───────────────────────── Focus audio ─────────────────────────

    private val focusListener = AudioManager.OnAudioFocusChangeListener { change ->
        when (change) {
            AudioManager.AUDIOFOCUS_LOSS, AudioManager.AUDIOFOCUS_LOSS_TRANSIENT -> scope.launch { pause() }
            AudioManager.AUDIOFOCUS_LOSS_TRANSIENT_CAN_DUCK -> live.masterVolume = live.masterVolume * 0.4f
            AudioManager.AUDIOFOCUS_GAIN -> store.loadMasterVolume(live)
        }
    }

    private fun requestFocus(): Boolean {
        val am = ContextCompat.getSystemService(app, AudioManager::class.java) ?: return true
        val req = AudioFocusRequest.Builder(AudioManager.AUDIOFOCUS_GAIN)
            .setAudioAttributes(AudioAttributes.Builder().setUsage(AudioAttributes.USAGE_MEDIA).setContentType(AudioAttributes.CONTENT_TYPE_MUSIC).build())
            .setOnAudioFocusChangeListener(focusListener)
            .setWillPauseWhenDucked(false)
            .build()
        focusRequest = req
        return am.requestAudioFocus(req) == AudioManager.AUDIOFOCUS_REQUEST_GRANTED
    }

    private fun abandonFocus() {
        val am = ContextCompat.getSystemService(app, AudioManager::class.java) ?: return
        focusRequest?.let { am.abandonAudioFocusRequest(it) }
    }

    fun shareIntent(): Intent {
        val s = engine.song
        val text = if (s != null) "J'écoute « ${s.title} » (${s.genre.name}, ${s.bpm.toInt()} BPM) généré par PULSAR 🎧 Code : ${s.code}" else "PULSAR"
        return Intent.createChooser(Intent(Intent.ACTION_SEND).setType("text/plain").putExtra(Intent.EXTRA_TEXT, text), "Partager")
    }
}
