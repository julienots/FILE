package com.superessence.tagforge.tasks

import android.annotation.SuppressLint
import android.app.Activity
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.SearchManager
import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.content.Intent
import android.hardware.camera2.CameraCharacteristics
import android.hardware.camera2.CameraManager
import android.media.AudioManager
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.os.VibrationEffect
import android.os.Vibrator
import android.provider.AlarmClock
import android.provider.MediaStore
import android.provider.Settings
import android.speech.tts.TextToSpeech
import android.speech.tts.UtteranceProgressListener
import android.view.KeyEvent
import android.widget.Toast
import androidx.core.app.NotificationCompat
import com.superessence.tagforge.R
import com.superessence.tagforge.nfc.Encoders
import kotlinx.coroutines.delay
import kotlinx.coroutines.suspendCancellableCoroutine
import kotlinx.coroutines.withTimeoutOrNull
import java.util.Locale
import kotlin.coroutines.resume

/** Exécute une liste de tâches. Retourne un compte rendu ligne par ligne. */
class TaskRunner(private val activity: Activity) {

    private var tts: TextToSpeech? = null
    private val audio = activity.getSystemService(Context.AUDIO_SERVICE) as AudioManager
    private val log = mutableListOf<String>()

    suspend fun run(tasks: List<TaskItem>, onStep: (String) -> Unit = {}): List<String> {
        for (t in tasks) {
            onStep(t.summary())
            val r = runCatching { exec(t) }.fold({ it }, { "⚠ ${t.type.title} : ${it.message ?: it.javaClass.simpleName}" })
            log += r
        }
        tts?.shutdown()
        return log
    }

    private fun start(i: Intent) {
        i.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
        activity.startActivity(i)
    }

    @SuppressLint("MissingPermission")
    private suspend fun exec(t: TaskItem): String {
        when (t.type) {
            TaskType.SPEAK -> speak(t.arg(0), t.arg(1), t.arg(2).toFloatOrNull() ?: 1f)
            TaskType.ASSISTANT -> {
                val ok = runCatching { start(Intent(Intent.ACTION_VOICE_COMMAND)) }.isSuccess ||
                    runCatching { start(Intent(Intent.ACTION_ASSIST)) }.isSuccess
                if (!ok) error("aucun assistant vocal installé")
            }
            TaskType.SEARCH -> runCatching { start(Intent(Intent.ACTION_WEB_SEARCH).putExtra(SearchManager.QUERY, t.arg(0))) }
                .getOrElse { start(Intent(Intent.ACTION_VIEW, Uri.parse("https://www.google.com/search?q=" + Encoders.enc(t.arg(0))))) }
            TaskType.FLASH -> flash(t.arg(0))
            TaskType.RINGER -> {
                val mode = when (t.arg(0)) {
                    "normal" -> AudioManager.RINGER_MODE_NORMAL
                    "silent" -> AudioManager.RINGER_MODE_SILENT
                    "toggle" -> if (audio.ringerMode == AudioManager.RINGER_MODE_NORMAL) AudioManager.RINGER_MODE_VIBRATE else AudioManager.RINGER_MODE_NORMAL
                    else -> AudioManager.RINGER_MODE_VIBRATE
                }
                val nm = activity.getSystemService(NotificationManager::class.java)
                if (!nm.isNotificationPolicyAccessGranted && mode == AudioManager.RINGER_MODE_SILENT) {
                    start(Intent(Settings.ACTION_NOTIFICATION_POLICY_ACCESS_SETTINGS))
                    error("autorisez « Ne pas déranger » pour TagForge puis réessayez")
                }
                try { audio.ringerMode = mode } catch (e: SecurityException) {
                    start(Intent(Settings.ACTION_NOTIFICATION_POLICY_ACCESS_SETTINGS)); error("autorisation « Ne pas déranger » requise")
                }
            }
            TaskType.DND -> {
                val nm = activity.getSystemService(NotificationManager::class.java)
                if (!nm.isNotificationPolicyAccessGranted) {
                    start(Intent(Settings.ACTION_NOTIFICATION_POLICY_ACCESS_SETTINGS)); error("autorisez « Ne pas déranger » pour TagForge")
                }
                val on = when (t.arg(0)) { "on" -> true; "off" -> false; else -> nm.currentInterruptionFilter == NotificationManager.INTERRUPTION_FILTER_ALL }
                nm.setInterruptionFilter(if (on) NotificationManager.INTERRUPTION_FILTER_PRIORITY else NotificationManager.INTERRUPTION_FILTER_ALL)
            }
            TaskType.VOLUME -> {
                val stream = when (t.arg(1)) { "ring" -> AudioManager.STREAM_RING; "alarm" -> AudioManager.STREAM_ALARM; "notif" -> AudioManager.STREAM_NOTIFICATION; else -> AudioManager.STREAM_MUSIC }
                val max = audio.getStreamMaxVolume(stream)
                val pct = (t.arg(0).toIntOrNull() ?: 50).coerceIn(0, 100)
                audio.setStreamVolume(stream, max * pct / 100, AudioManager.FLAG_SHOW_UI)
            }
            TaskType.MEDIA -> {
                val code = when (t.arg(0)) { "next" -> KeyEvent.KEYCODE_MEDIA_NEXT; "previous" -> KeyEvent.KEYCODE_MEDIA_PREVIOUS; "stop" -> KeyEvent.KEYCODE_MEDIA_STOP; else -> KeyEvent.KEYCODE_MEDIA_PLAY_PAUSE }
                audio.dispatchMediaKeyEvent(KeyEvent(KeyEvent.ACTION_DOWN, code))
                audio.dispatchMediaKeyEvent(KeyEvent(KeyEvent.ACTION_UP, code))
            }
            TaskType.OPEN_APP -> {
                val i = activity.packageManager.getLaunchIntentForPackage(t.arg(0).trim())
                if (i != null) start(i) else start(Intent(Intent.ACTION_VIEW, Uri.parse("market://details?id=" + t.arg(0).trim())))
            }
            TaskType.OPEN_URL -> start(Intent(Intent.ACTION_VIEW, Uri.parse(t.arg(0).trim().let { if (it.contains(":")) it else "https://$it" })))
            TaskType.NAVIGATE -> start(Intent(Intent.ACTION_VIEW, Uri.parse("google.navigation:q=" + Encoders.enc(t.arg(0)) + "&mode=" + t.arg(1))))
            TaskType.DIAL -> start(Intent(Intent.ACTION_DIAL, Uri.parse("tel:" + Encoders.digits(t.arg(0)))))
            TaskType.SMS -> start(Intent(Intent.ACTION_SENDTO, Uri.parse("smsto:" + Encoders.digits(t.arg(0)))).putExtra("sms_body", t.arg(1)))
            TaskType.ALARM -> {
                val (h, m) = t.arg(0).split(':', 'h', 'H').map { it.trim().toIntOrNull() ?: 0 }.let { (it.getOrElse(0) { 7 }) to (it.getOrElse(1) { 0 }) }
                start(Intent(AlarmClock.ACTION_SET_ALARM).putExtra(AlarmClock.EXTRA_HOUR, h).putExtra(AlarmClock.EXTRA_MINUTES, m)
                    .putExtra(AlarmClock.EXTRA_MESSAGE, t.arg(1)).putExtra(AlarmClock.EXTRA_SKIP_UI, true))
                return "Alarme %02d:%02d".format(h, m)
            }
            TaskType.TIMER -> {
                val min = t.arg(0).replace(',', '.').toDoubleOrNull() ?: 5.0
                start(Intent(AlarmClock.ACTION_SET_TIMER).putExtra(AlarmClock.EXTRA_LENGTH, (min * 60).toInt())
                    .putExtra(AlarmClock.EXTRA_MESSAGE, t.arg(1)).putExtra(AlarmClock.EXTRA_SKIP_UI, true))
            }
            TaskType.SETTINGS -> start(settingsIntent(t.arg(0)))
            TaskType.BRIGHTNESS -> {
                if (!Settings.System.canWrite(activity)) {
                    start(Intent(Settings.ACTION_MANAGE_WRITE_SETTINGS, Uri.parse("package:" + activity.packageName)))
                    error("autorisez « Modifier les paramètres système » pour TagForge")
                }
                val v = ((t.arg(0).toIntOrNull() ?: 80).coerceIn(0, 100) * 255 / 100).coerceAtLeast(1)
                Settings.System.putInt(activity.contentResolver, Settings.System.SCREEN_BRIGHTNESS_MODE, Settings.System.SCREEN_BRIGHTNESS_MODE_MANUAL)
                Settings.System.putInt(activity.contentResolver, Settings.System.SCREEN_BRIGHTNESS, v)
            }
            TaskType.CAMERA -> start(Intent(if (t.arg(0) == "video") MediaStore.INTENT_ACTION_VIDEO_CAMERA else MediaStore.INTENT_ACTION_STILL_IMAGE_CAMERA))
            TaskType.VIBRATE -> {
                val v = activity.getSystemService(Vibrator::class.java)
                v.vibrate(VibrationEffect.createOneShot((t.arg(0).toLongOrNull() ?: 400L).coerceIn(10, 5000), VibrationEffect.DEFAULT_AMPLITUDE))
            }
            TaskType.CLIPBOARD -> activity.getSystemService(ClipboardManager::class.java).setPrimaryClip(ClipData.newPlainText("TagForge", t.arg(0)))
            TaskType.NOTIFY -> notify(activity, t.arg(0), t.arg(1))
            TaskType.TOAST -> Handler(Looper.getMainLooper()).post { Toast.makeText(activity, t.arg(0), Toast.LENGTH_LONG).show() }
            TaskType.WAIT -> delay(((t.arg(0).replace(',', '.').toDoubleOrNull() ?: 1.0) * 1000).toLong().coerceIn(0, 60_000))
        }
        return "✓ " + t.summary()
    }

    private fun settingsIntent(k: String): Intent = when (k) {
        "wifi" -> if (Build.VERSION.SDK_INT >= 29) Intent(Settings.Panel.ACTION_WIFI) else Intent(Settings.ACTION_WIFI_SETTINGS)
        "internet" -> if (Build.VERSION.SDK_INT >= 29) Intent(Settings.Panel.ACTION_INTERNET_CONNECTIVITY) else Intent(Settings.ACTION_WIRELESS_SETTINGS)
        "volume" -> if (Build.VERSION.SDK_INT >= 29) Intent(Settings.Panel.ACTION_VOLUME) else Intent(Settings.ACTION_SOUND_SETTINGS)
        "nfc" -> if (Build.VERSION.SDK_INT >= 29) Intent(Settings.Panel.ACTION_NFC) else Intent(Settings.ACTION_NFC_SETTINGS)
        "bluetooth" -> Intent(Settings.ACTION_BLUETOOTH_SETTINGS)
        "location" -> Intent(Settings.ACTION_LOCATION_SOURCE_SETTINGS)
        "display" -> Intent(Settings.ACTION_DISPLAY_SETTINGS)
        "battery" -> Intent(Settings.ACTION_BATTERY_SAVER_SETTINGS)
        "hotspot" -> Intent(Settings.ACTION_WIRELESS_SETTINGS)
        "airplane" -> Intent(Settings.ACTION_AIRPLANE_MODE_SETTINGS)
        "dnd" -> Intent(Settings.ACTION_ZEN_MODE_PRIORITY_SETTINGS)
        else -> Intent(Settings.ACTION_SETTINGS)
    }

    private suspend fun flash(action: String) {
        val cm = activity.getSystemService(CameraManager::class.java)
        val id = cm.cameraIdList.firstOrNull { cm.getCameraCharacteristics(it).get(CameraCharacteristics.FLASH_INFO_AVAILABLE) == true }
            ?: error("pas de flash sur ce téléphone")
        val on = when (action) {
            "on" -> true
            "off" -> false
            else -> {
                // l'état actuel est envoyé immédiatement par le callback
                val current = withTimeoutOrNull(800) {
                    suspendCancellableCoroutine<Boolean> { c ->
                        val cb = object : CameraManager.TorchCallback() {
                            override fun onTorchModeChanged(cameraId: String, enabled: Boolean) {
                                if (cameraId == id) { cm.unregisterTorchCallback(this); if (c.isActive) c.resume(enabled) }
                            }
                        }
                        cm.registerTorchCallback(cb, Handler(Looper.getMainLooper()))
                        c.invokeOnCancellation { cm.unregisterTorchCallback(cb) }
                    }
                } ?: false
                !current
            }
        }
        cm.setTorchMode(id, on)
    }

    private suspend fun speak(text: String, lang: String, rate: Float) {
        val engine = tts ?: withTimeoutOrNull(8000) {
            suspendCancellableCoroutine<TextToSpeech?> { c ->
                var e: TextToSpeech? = null
                e = TextToSpeech(activity.applicationContext) { status ->
                    if (c.isActive) c.resume(if (status == TextToSpeech.SUCCESS) e else null)
                }
            }
        }?.also { tts = it } ?: error("synthèse vocale indisponible (installez « Services vocaux Google »)")

        val locale = Locale.forLanguageTag(lang.ifBlank { "fr-FR" })
        val res = engine.setLanguage(locale)
        if (res == TextToSpeech.LANG_MISSING_DATA || res == TextToSpeech.LANG_NOT_SUPPORTED) engine.language = Locale.getDefault()
        // privilégie une voix Google de qualité (réseau neuronal / hors ligne) pour la langue choisie
        runCatching {
            engine.voices?.filter { it.locale.language == locale.language && !it.isNetworkConnectionRequired }
                ?.maxByOrNull { it.quality }?.let { engine.voice = it }
        }
        engine.setSpeechRate(rate)
        withTimeoutOrNull(60_000) {
            suspendCancellableCoroutine<Unit> { c ->
                engine.setOnUtteranceProgressListener(object : UtteranceProgressListener() {
                    override fun onStart(id: String?) {}
                    override fun onDone(id: String?) { if (c.isActive) c.resume(Unit) }
                    @Deprecated("Deprecated in Java")
                    override fun onError(id: String?) { if (c.isActive) c.resume(Unit) }
                })
                val params = Bundle().apply { putInt(TextToSpeech.Engine.KEY_PARAM_STREAM, AudioManager.STREAM_MUSIC) }
                engine.speak(text, TextToSpeech.QUEUE_FLUSH, params, "tf-" + System.nanoTime())
            }
        }
    }

    companion object {
        fun notify(ctx: Context, title: String, text: String) {
            val nm = ctx.getSystemService(NotificationManager::class.java)
            nm.createNotificationChannel(NotificationChannel("tasks", "Tâches NFC", NotificationManager.IMPORTANCE_DEFAULT))
            val n = NotificationCompat.Builder(ctx, "tasks")
                .setSmallIcon(R.drawable.ic_launcher_foreground)
                .setContentTitle(title.ifBlank { "TagForge" })
                .setContentText(text)
                .setStyle(NotificationCompat.BigTextStyle().bigText(text))
                .setAutoCancel(true)
                .build()
            try { nm.notify((System.currentTimeMillis() % 100000).toInt(), n) } catch (_: SecurityException) { error("autorisez les notifications pour TagForge") }
        }
    }
}
