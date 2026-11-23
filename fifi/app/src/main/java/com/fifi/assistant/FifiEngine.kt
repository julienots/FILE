package com.fifi.assistant

import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import android.hardware.camera2.CameraCharacteristics
import android.hardware.camera2.CameraManager
import android.media.AudioManager
import android.net.Uri
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.provider.MediaStore
import android.speech.RecognitionListener
import android.speech.RecognizerIntent
import android.speech.SpeechRecognizer
import androidx.core.app.NotificationCompat

/**
 * Le moteur de Fifi (thread principal) :
 * écoute du mot « Dis Fifi » en boucle -> écoute de la question -> réponse vocale -> action.
 */
object FifiEngine {
    enum class State { IDLE, WAITING_WAKE, LISTENING, SPEAKING }

    interface Listener {
        fun onState(state: State) {}
        fun onHeard(text: String) {}
        fun onReply(text: String) {}
    }

    private lateinit var app: Context
    private lateinit var brain: Brain
    private lateinit var player: VoicePlayer
    private val main = Handler(Looper.getMainLooper())
    private val listeners = LinkedHashSet<Listener>()
    private var recognizer: SpeechRecognizer? = null
    private var initialized = false

    var state = State.IDLE
        private set(v) { field = v; listeners.forEach { it.onState(v) } }

    /** Écoute permanente activée (service au premier plan). */
    var wakeMode = false
        private set
    /** Coupe le « bip » de la reconnaissance vocale pendant l'écoute en boucle. */
    var muteBeep = true
    /** Vrai quand l'écran de l'appli est visible (sinon les actions passent par une notification). */
    var activityVisible = false

    private var muted = false
    private var listeningForQuestion = false
    private var gotResult = false

    fun init(ctx: Context) {
        if (initialized) return
        app = ctx.applicationContext
        brain = Brain(loadVoiceBank(app))
        player = VoicePlayer(app)
        val p = app.getSharedPreferences("fifi", Context.MODE_PRIVATE)
        muteBeep = p.getBoolean("muteBeep", true)
        brain.flemmeRate = p.getFloat("flemme", 0.15f).toDouble()
        initialized = true
    }

    fun setFlemme(level: Float) {
        brain.flemmeRate = level.toDouble()
        brain.flemmeOkRate = 0.10 + level
    }

    fun addListener(l: Listener) { listeners += l; l.onState(state) }
    fun removeListener(l: Listener) { listeners -= l }

    // ---------------------------------------------------------------- écoute

    fun startWakeMode() {
        wakeMode = true
        if (state == State.IDLE) listenWake()
    }

    fun stopWakeMode() {
        wakeMode = false
        if (state == State.WAITING_WAKE) {
            recognizer?.cancel()
            state = State.IDLE
        }
        unmute()
    }

    /** Bouton « Parler à Fifi » ou appui long Accueil : on écoute directement la question. */
    fun askNow() {
        player.stop()
        recognizer?.cancel()
        listenQuestion(sayYes = false)
    }

    private fun listenWake() {
        listeningForQuestion = false
        state = State.WAITING_WAKE
        if (muteBeep) mute()
        startRecognizer(wake = true)
    }

    private fun listenQuestion(sayYes: Boolean) {
        if (sayYes) {
            speak(brain.simple("oui")) { listenQuestion(false) }
            return
        }
        listeningForQuestion = true
        state = State.LISTENING
        unmute()
        startRecognizer(wake = false)
    }

    private fun startRecognizer(wake: Boolean) {
        if (!SpeechRecognizer.isRecognitionAvailable(app)) {
            listeners.forEach { it.onReply("Pas de reconnaissance vocale sur ce téléphone (installe l'appli Google).") }
            state = State.IDLE
            return
        }
        val rec = recognizer ?: SpeechRecognizer.createSpeechRecognizer(app).also {
            it.setRecognitionListener(recListener); recognizer = it
        }
        gotResult = false
        val i = Intent(RecognizerIntent.ACTION_RECOGNIZE_SPEECH).apply {
            putExtra(RecognizerIntent.EXTRA_LANGUAGE_MODEL, RecognizerIntent.LANGUAGE_MODEL_FREE_FORM)
            putExtra(RecognizerIntent.EXTRA_LANGUAGE, "fr-FR")
            putExtra(RecognizerIntent.EXTRA_LANGUAGE_PREFERENCE, "fr-FR")
            putExtra(RecognizerIntent.EXTRA_PARTIAL_RESULTS, true)
            putExtra(RecognizerIntent.EXTRA_MAX_RESULTS, 5)
            putExtra(RecognizerIntent.EXTRA_CALLING_PACKAGE, app.packageName)
            if (wake) putExtra(RecognizerIntent.EXTRA_PREFER_OFFLINE, true)
            putExtra(RecognizerIntent.EXTRA_SPEECH_INPUT_COMPLETE_SILENCE_LENGTH_MILLIS, 1200L)
        }
        try {
            rec.startListening(i)
        } catch (e: Exception) {
            restartLater(1000)
        }
    }

    private fun restartLater(ms: Long) {
        main.removeCallbacks(restartRunnable)
        main.postDelayed(restartRunnable, ms)
    }

    private val restartRunnable = Runnable {
        if (state == State.SPEAKING) return@Runnable
        if (wakeMode) listenWake() else state = State.IDLE
    }

    private val recListener = object : RecognitionListener {
        override fun onReadyForSpeech(params: Bundle?) {}
        override fun onBeginningOfSpeech() {}
        override fun onRmsChanged(rmsdB: Float) {}
        override fun onBufferReceived(buffer: ByteArray?) {}
        override fun onEndOfSpeech() {}
        override fun onEvent(eventType: Int, params: Bundle?) {}

        override fun onPartialResults(partialResults: Bundle?) {
            if (!listeningForQuestion) return
            val t = partialResults?.getStringArrayList(SpeechRecognizer.RESULTS_RECOGNITION)?.firstOrNull() ?: return
            if (t.isNotBlank()) listeners.forEach { it.onHeard(t) }
        }

        override fun onResults(results: Bundle?) {
            if (gotResult) return
            gotResult = true
            val all = results?.getStringArrayList(SpeechRecognizer.RESULTS_RECOGNITION).orEmpty()
            if (listeningForQuestion) {
                val t = all.firstOrNull().orEmpty()
                listeners.forEach { it.onHeard(t) }
                // On enlève un éventuel « Dis Fifi » répété au début.
                val n = brain.normalize(t)
                val q = Brain.afterWakeWord(n)?.takeIf { it.isNotBlank() } ?: t
                reply(brain.answer(q))
            } else {
                val hit = all.firstNotNullOfOrNull { Brain.afterWakeWord(brain.normalize(it))?.let { rest -> it to rest } }
                if (hit == null) { listenWake(); return }
                listeners.forEach { it.onHeard(hit.first) }
                if (hit.second.split(" ").count { it.length > 1 } >= 2) reply(brain.answer(hit.second))
                else listenQuestion(sayYes = true)
            }
        }

        override fun onError(error: Int) {
            if (gotResult) return
            gotResult = true
            when {
                listeningForQuestion && (error == SpeechRecognizer.ERROR_NO_MATCH || error == SpeechRecognizer.ERROR_SPEECH_TIMEOUT) ->
                    reply(brain.simple("silence"))
                error == SpeechRecognizer.ERROR_RECOGNIZER_BUSY || error == SpeechRecognizer.ERROR_CLIENT -> {
                    recognizer?.destroy(); recognizer = null
                    restartLater(800)
                }
                error == SpeechRecognizer.ERROR_INSUFFICIENT_PERMISSIONS -> {
                    listeners.forEach { it.onReply("J'ai besoin du micro ! Autorise-le dans les réglages.") }
                    state = State.IDLE
                }
                else -> restartLater(if (error == SpeechRecognizer.ERROR_NETWORK || error == SpeechRecognizer.ERROR_SERVER) 3000 else 250)
            }
        }
    }

    // ---------------------------------------------------------------- réponse

    private fun reply(r: Reply) {
        // La lampe s'allume tout de suite, le reste après la phrase.
        if (r.action == Action.TORCH_ON || r.action == Action.TORCH_OFF) doAction(r)
        speak(r) {
            if (r.action != Action.TORCH_ON && r.action != Action.TORCH_OFF) doAction(r)
            if (wakeMode) listenWake() else state = State.IDLE
        }
    }

    private fun speak(r: Reply, done: () -> Unit) {
        recognizer?.cancel()
        unmute()
        state = State.SPEAKING
        listeners.forEach { it.onReply(r.text) }
        player.play(r.clips) { main.post(done) }
    }

    fun stopTalking() {
        player.stop()
        recognizer?.cancel()
        if (wakeMode) listenWake() else state = State.IDLE
    }

    private fun doAction(r: Reply) {
        when (r.action) {
            Action.NONE, Action.STOP -> {}
            Action.TORCH_ON -> torch(true)
            Action.TORCH_OFF -> torch(false)
            Action.SEARCH -> open(searchIntent(r.query), "Recherche : ${r.query}")
            Action.WEATHER -> open(searchIntent("météo"), "La météo")
            Action.YOUTUBE -> open(
                app.packageManager.getLaunchIntentForPackage("com.google.android.youtube")
                    ?: Intent(Intent.ACTION_VIEW, Uri.parse("https://www.youtube.com")), "YouTube"
            )
            Action.MUSIC -> open(Intent.makeMainSelectorActivity(Intent.ACTION_MAIN, Intent.CATEGORY_APP_MUSIC), "Ta musique")
            Action.DIAL -> open(Intent(Intent.ACTION_DIAL), "Le téléphone")
            Action.CAMERA -> open(Intent(MediaStore.INTENT_ACTION_STILL_IMAGE_CAMERA), "L'appareil photo")
        }
    }

    private fun searchIntent(q: String) =
        Intent(Intent.ACTION_VIEW, Uri.parse("https://www.google.com/search?q=" + Uri.encode(q)))

    private fun open(i: Intent, label: String) {
        i.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
        if (activityVisible) {
            try { app.startActivity(i); return } catch (_: Exception) {}
        }
        // Android interdit d'ouvrir une appli depuis l'arrière-plan : on passe par une notification.
        val nm = app.getSystemService(NotificationManager::class.java)
        nm.createNotificationChannel(NotificationChannel("fifi_actions", "Actions de Fifi", NotificationManager.IMPORTANCE_HIGH))
        val pi = PendingIntent.getActivity(app, label.hashCode(), i, PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT)
        val n = NotificationCompat.Builder(app, "fifi_actions")
            .setSmallIcon(R.drawable.ic_stat_fifi)
            .setContentTitle("Fifi a la flemme d'ouvrir ça elle-même")
            .setContentText("Touche ici : $label")
            .setContentIntent(pi).setAutoCancel(true)
            .setPriority(NotificationCompat.PRIORITY_HIGH).build()
        runCatching { nm.notify(42, n) }
    }

    private fun torch(on: Boolean) {
        runCatching {
            val cm = app.getSystemService(CameraManager::class.java)
            val id = cm.cameraIdList.firstOrNull {
                cm.getCameraCharacteristics(it).get(CameraCharacteristics.FLASH_INFO_AVAILABLE) == true
            } ?: return
            cm.setTorchMode(id, on)
        }
    }

    // ---------------------------------------------------------------- bip

    private val beepStreams = intArrayOf(AudioManager.STREAM_MUSIC, AudioManager.STREAM_NOTIFICATION, AudioManager.STREAM_SYSTEM)

    private fun mute() {
        if (muted) return
        val am = app.getSystemService(AudioManager::class.java)
        beepStreams.forEach { runCatching { am.adjustStreamVolume(it, AudioManager.ADJUST_MUTE, 0) } }
        muted = true
    }

    private fun unmute() {
        if (!muted) return
        val am = app.getSystemService(AudioManager::class.java)
        beepStreams.forEach { runCatching { am.adjustStreamVolume(it, AudioManager.ADJUST_UNMUTE, 0) } }
        muted = false
    }

    fun shutdown() {
        stopWakeMode()
        player.stop()
        recognizer?.destroy(); recognizer = null
        state = State.IDLE
    }
}
