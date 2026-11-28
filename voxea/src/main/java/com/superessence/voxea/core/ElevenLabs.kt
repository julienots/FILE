package com.superessence.voxea.core

import java.io.ByteArrayOutputStream
import java.io.DataOutputStream
import java.net.HttpURLConnection
import java.net.URL
import java.nio.ByteBuffer
import java.nio.ByteOrder

/**
 * Conversion IA facultative (ElevenLabs « speech-to-speech ») : garde l'intonation de l'enregistrement
 * et la rend avec une autre voix. La clé de l'utilisateur ne quitte l'appareil que vers api.elevenlabs.io.
 * En cas d'erreur, repli sur la transformation hors ligne.
 */
class ElevenLabsConverter(
    private val apiKey: String,
    private val voiceIds: Map<VoiceType, String>,
    private val onFallback: (String) -> Unit = {},
) : Converter {
    override fun convert(line: Line, pcm: FloatArray, sr: Int, type: VoiceType, settings: VoiceSettings): FloatArray {
        val voice = voiceIds[type]?.trim().orEmpty()
        if (type == VoiceType.ORIGINAL || voice.isEmpty()) return LocalConverter.convert(line, pcm, sr, type, settings)
        return try {
            val outSr = 24000
            val raw = request(pcm, sr, voice, outSr)
            val bb = ByteBuffer.wrap(raw).order(ByteOrder.LITTLE_ENDIAN)
            val y = FloatArray(raw.size / 2) { bb.getShort(it * 2) / 32768f }
            Audio.resample(y, outSr, sr)
        } catch (e: Exception) {
            onFallback("IA indisponible (${e.message}) : réplique ${line.index + 1} transformée hors ligne.")
            LocalConverter.convert(line, pcm, sr, type, settings)
        }
    }

    private fun request(pcm: FloatArray, sr: Int, voiceId: String, outSr: Int): ByteArray {
        val wav = ByteArrayOutputStream().also { Wav.write(it, pcm, sr) }.toByteArray()
        val boundary = "voxea" + System.nanoTime()
        val url = URL("https://api.elevenlabs.io/v1/speech-to-speech/$voiceId?output_format=pcm_$outSr")
        val c = url.openConnection() as HttpURLConnection
        c.requestMethod = "POST"; c.doOutput = true
        c.connectTimeout = 20000; c.readTimeout = 120000
        c.setRequestProperty("xi-api-key", apiKey)
        c.setRequestProperty("Content-Type", "multipart/form-data; boundary=$boundary")
        DataOutputStream(c.outputStream).use { o ->
            fun field(name: String, value: String) {
                o.writeBytes("--$boundary\r\nContent-Disposition: form-data; name=\"$name\"\r\n\r\n")
                o.write(value.toByteArray()); o.writeBytes("\r\n")
            }
            field("model_id", "eleven_multilingual_sts_v2")
            field("remove_background_noise", "true")
            o.writeBytes("--$boundary\r\nContent-Disposition: form-data; name=\"audio\"; filename=\"voix.wav\"\r\nContent-Type: audio/wav\r\n\r\n")
            o.write(wav); o.writeBytes("\r\n--$boundary--\r\n")
        }
        val code = c.responseCode
        if (code !in 200..299) {
            val msg = c.errorStream?.bufferedReader()?.readText()?.take(160) ?: ""
            throw IllegalStateException("HTTP $code $msg")
        }
        return c.inputStream.use { it.readBytes() }
    }
}
