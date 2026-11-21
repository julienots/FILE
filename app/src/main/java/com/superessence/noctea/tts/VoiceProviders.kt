package com.superessence.noctea.tts

import android.content.Context
import android.media.MediaMetadataRetriever
import android.os.Bundle
import android.speech.tts.TextToSpeech
import android.speech.tts.UtteranceProgressListener
import android.speech.tts.Voice
import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import kotlinx.coroutines.withContext
import kotlinx.coroutines.withTimeout
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.put
import java.io.File
import java.io.RandomAccessFile
import java.net.HttpURLConnection
import java.net.URL
import java.util.Locale
import java.util.UUID

/**
 * Architecture vocale modulaire de NOCTÉA.
 *
 * - [EmbeddedVoice] : voix féminine premium préenregistrée (bibliothèque + générateur), hors ligne.
 * - [SystemTtsProvider] : moteur vocal Android de l'appareil (texte libre, hors ligne si la voix est installée).
 * - [RemoteHttpTtsProvider] : n'importe quel serveur TTS configuré par l'utilisateur
 *   (ex. serveur Piper auto-hébergé, ou passerelle vers un fournisseur premium).
 *   Aucune clé n'est intégrée au code : URL et clé éventuelle sont saisies dans les réglages.
 */
interface VoiceProvider {
    val id: String
    val displayName: String
    /** Description honnête de la qualité attendue. */
    val qualityNote: String

    /**
     * Appelé UNE fois avant une série de fragments d'un même texte : fige la voix et les réglages pour que
     * tous les fragments soient dits par la même voix (jamais de repli silencieux vers une autre voix).
     */
    suspend fun prepare() {}

    /** Synthétise [text] dans [out]. Retourne la durée en ms. */
    suspend fun synthesize(text: String, out: File): Long
}

object EmbeddedVoice {
    const val NAME = "Voix NOCTÉA (préenregistrée)"
    const val NOTE = "Voix féminine française douce, produite en studio à partir d'un modèle neuronal libre (Kokoro-82M, voix « Siwis » du corpus SIWIS, CC-BY 4.0). Une seule voix : même timbre, même hauteur et même niveau d'un bout à l'autre de chaque séance. Disponible hors connexion."
}

class TtsException(message: String) : Exception(message)

/** Moteur TTS Android, configuré pour un débit lent et une voix française. */
class SystemTtsProvider(private val context: Context) : VoiceProvider {
    override val id = "system"
    override val displayName = "Voix de l'appareil"
    override val qualityNote =
        "Utilise le moteur vocal installé sur ton téléphone. La qualité dépend de l'appareil : " +
            "les voix « réseau neuronal » de Google sont naturelles, d'autres peuvent sembler synthétiques. " +
            "NOCTÉA ajuste le niveau, la tonalité et les silences pour que tous les passages sonnent pareil."

    private var tts: TextToSpeech? = null
    private val mutex = Mutex()
    private val pending = mutableMapOf<String, CompletableDeferred<Boolean>>()
    var rate = 0.82f
    var pitch = 0.96f
    var voiceName: String = ""

    private suspend fun engine(): TextToSpeech = tts ?: withContext(Dispatchers.Main) {
        val ready = CompletableDeferred<Int>()
        val t = TextToSpeech(context.applicationContext) { status -> ready.complete(status) }
        val status = withTimeout(10_000) { ready.await() }
        if (status != TextToSpeech.SUCCESS) throw TtsException("Aucun moteur de synthèse vocale disponible sur cet appareil.")
        val res = t.setLanguage(Locale.FRANCE)
        if (res == TextToSpeech.LANG_MISSING_DATA || res == TextToSpeech.LANG_NOT_SUPPORTED) {
            throw TtsException("La voix française n'est pas installée. Installe-la dans Réglages Android › Synthèse vocale.")
        }
        t.setOnUtteranceProgressListener(object : UtteranceProgressListener() {
            override fun onStart(utteranceId: String?) {}
            override fun onDone(utteranceId: String?) { utteranceId?.let { pending.remove(it)?.complete(true) } }
            @Deprecated("Deprecated in Java")
            override fun onError(utteranceId: String?) { utteranceId?.let { pending.remove(it)?.complete(false) } }
            override fun onError(utteranceId: String?, errorCode: Int) { utteranceId?.let { pending.remove(it)?.complete(false) } }
        })
        tts = t
        t
    }

    suspend fun frenchVoices(): List<Voice> = try {
        engine().voices.orEmpty()
            .filter { it.locale.language == "fr" && !it.features.contains(TextToSpeech.Engine.KEY_FEATURE_NOT_INSTALLED) }
            .sortedWith(voiceOrder)
    } catch (e: Exception) {
        emptyList()
    }

    /** Ordre stable : hors ligne d'abord, France avant les autres pays, meilleure qualité, puis nom. */
    private val voiceOrder = compareBy<Voice>(
        { it.isNetworkConnectionRequired },
        { it.locale.country != "FR" },
        { -it.quality },
        { it.name },
    )

    private var resolved: Voice? = null

    /** Voix retenue pour tout le texte : celle choisie par l'utilisateur, sinon la meilleure voix hors ligne. */
    private suspend fun resolveVoice(t: TextToSpeech) = withContext(Dispatchers.Main) {
        val all = t.voices.orEmpty()
        val chosen = all.firstOrNull { it.name == voiceName }
            ?: all.filter {
                it.locale.language == "fr" && !it.isNetworkConnectionRequired &&
                    !it.features.contains(TextToSpeech.Engine.KEY_FEATURE_NOT_INSTALLED)
            }.sortedWith(voiceOrder).firstOrNull()
        resolved = chosen
        chosen?.let { t.voice = it }
        t.setSpeechRate(rate)
        t.setPitch(pitch)
    }

    override suspend fun prepare() {
        mutex.withLock { resolveVoice(engine()) }
    }

    override suspend fun synthesize(text: String, out: File): Long = mutex.withLock {
        val t = engine()
        if (resolved == null) resolveVoice(t)       // appel direct sans prepare() (aperçu) : même règle de choix
        withContext(Dispatchers.Main) {
            // Même voix pour tous les fragments : on la réaffirme avant chaque phrase (certains moteurs
            // reviennent à leur voix par défaut entre deux appels).
            val v = resolved
            if (v != null && t.voice?.name != v.name) t.voice = v
            t.setSpeechRate(rate)
            t.setPitch(pitch)
        }
        val uid = UUID.randomUUID().toString()
        val done = CompletableDeferred<Boolean>()
        pending[uid] = done
        val code = t.synthesizeToFile(text, Bundle(), out, uid)
        if (code != TextToSpeech.SUCCESS) {
            pending.remove(uid); throw TtsException("La synthèse a échoué.")
        }
        val ok = withTimeout(60_000) { done.await() }
        if (!ok || !out.exists() || out.length() < 100) {
            val v = resolved
            if (v != null && v.isNetworkConnectionRequired) {
                throw TtsException("La voix « ${v.name} » demande une connexion Internet. Choisis une voix hors ligne dans Profil › Voix.")
            }
            throw TtsException("La synthèse a échoué pour : « ${text.take(40)} »")
        }
        AudioFiles.durationMs(out)
    }

    fun shutdown() {
        tts?.shutdown(); tts = null
    }
}

/**
 * Fournisseur distant générique : POST JSON {"text", "voice", "length_scale", "speed"} → audio (wav/mp3/ogg).
 * Compatible avec le serveur HTTP de Piper (`python3 -m piper.http_server -m fr_FR-siwis-medium`).
 */
class RemoteHttpTtsProvider(
    private val url: String,
    private val apiKey: String,
    private val voice: String,
    private val speed: Float,
) : VoiceProvider {
    override val id = "remote"

    /** Vrai si le serveur a déjà appliqué la chaîne studio NOCTÉA (en-tête X-Noctea-Studio: 1). */
    @Volatile var studioProcessed = false
        private set
    override val displayName = "Serveur vocal personnalisé"
    override val qualityNote = "Qualité définie par le serveur configuré (ex. voix neuronale premium). Nécessite une connexion."

    override suspend fun synthesize(text: String, out: File): Long = withContext(Dispatchers.IO) {
        if (url.isBlank()) throw TtsException("Aucun serveur vocal configuré (Profil › Voix).")
        val conn = (URL(url).openConnection() as HttpURLConnection).apply {
            requestMethod = "POST"
            connectTimeout = 15_000
            readTimeout = 60_000
            doOutput = true
            setRequestProperty("Content-Type", "application/json; charset=utf-8")
            setRequestProperty("Accept", "audio/wav, audio/mpeg, audio/ogg, */*")
            if (apiKey.isNotBlank()) setRequestProperty("Authorization", "Bearer $apiKey")
        }
        try {
            val body = buildJsonObject {
                put("text", text)
                if (voice.isNotBlank()) put("voice", voice)
                put("length_scale", (1f / speed.coerceIn(0.5f, 1.5f)).toDouble())
                put("speed", speed.toDouble())
            }.toString()
            conn.outputStream.use { it.write(body.toByteArray()) }
            val code = conn.responseCode
            if (code !in 200..299) throw TtsException("Le serveur vocal a répondu $code.")
            studioProcessed = conn.getHeaderField("X-Noctea-Studio") == "1"
            conn.inputStream.use { input -> out.outputStream().use { input.copyTo(it) } }
        } finally {
            conn.disconnect()
        }
        if (out.length() < 100) throw TtsException("Réponse audio vide.")
        AudioFiles.durationMs(out)
    }
}

object AudioFiles {
    /** Durée d'un fichier audio : en-tête WAV si possible, sinon MediaMetadataRetriever. */
    fun durationMs(file: File): Long {
        wavDuration(file)?.let { return it }
        val r = MediaMetadataRetriever()
        return try {
            r.setDataSource(file.absolutePath)
            r.extractMetadata(MediaMetadataRetriever.METADATA_KEY_DURATION)?.toLongOrNull() ?: 0L
        } catch (e: Exception) {
            0L
        } finally {
            r.release()
        }
    }

    fun wavDuration(file: File): Long? = try {
        RandomAccessFile(file, "r").use { f ->
            val head = ByteArray(12)
            f.readFully(head)
            if (String(head, 0, 4) != "RIFF" || String(head, 8, 4) != "WAVE") return null
            var byteRate = 0L
            while (f.filePointer < f.length() - 8) {
                val idb = ByteArray(4); f.readFully(idb)
                val size = Integer.reverseBytes(f.readInt()).toLong() and 0xffffffffL
                val id = String(idb)
                if (id == "fmt ") {
                    val fmt = ByteArray(size.toInt()); f.readFully(fmt)
                    byteRate = (fmt[8].toLong() and 0xff) or ((fmt[9].toLong() and 0xff) shl 8) or
                        ((fmt[10].toLong() and 0xff) shl 16) or ((fmt[11].toLong() and 0xff) shl 24)
                } else if (id == "data") {
                    val realSize = if (size == 0L || size > f.length()) f.length() - f.filePointer else size
                    return if (byteRate > 0) realSize * 1000 / byteRate else null
                } else {
                    f.seek(f.filePointer + size + (size and 1))
                }
            }
            null
        }
    } catch (e: Exception) {
        null
    }
}
