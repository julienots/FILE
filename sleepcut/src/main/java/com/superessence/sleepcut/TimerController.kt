package com.superessence.sleepcut

import android.app.AlarmManager
import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import android.os.Build
import androidx.core.content.ContextCompat
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow

data class TimerState(
    /** Heure de coupure (ms depuis epoch), 0 = aucun minuteur. */
    val endAt: Long = 0L,
    val startedAt: Long = 0L,
    val fading: Boolean = false,
    val lastResult: String? = null,
)

data class Settings(
    val fadeSeconds: Int = 30,
    val lastMinutes: Int = 30,
    val restoreVolume: Boolean = true,
    val shakeToExtend: Boolean = true,
    val vibrateAtEnd: Boolean = false,
    val dndAtEnd: Boolean = false,
    val stopCount: Int = 0,
)

/** Point d'entrée unique pour démarrer, prolonger et annuler le minuteur. */
object TimerController {
    private val _state = MutableStateFlow(TimerState())
    val state: StateFlow<TimerState> = _state.asStateFlow()
    private val _settings = MutableStateFlow(Settings())
    val settings: StateFlow<Settings> = _settings.asStateFlow()
    private var loaded = false

    private fun prefs(ctx: Context) = ctx.getSharedPreferences("sleepcut", Context.MODE_PRIVATE)

    fun load(ctx: Context) {
        if (loaded) return
        loaded = true
        val p = prefs(ctx)
        _settings.value = Settings(
            fadeSeconds = p.getInt("fade", 30), lastMinutes = p.getInt("last", 30),
            restoreVolume = p.getBoolean("restore", true), shakeToExtend = p.getBoolean("shake", true),
            vibrateAtEnd = p.getBoolean("vibrate", false), dndAtEnd = p.getBoolean("dnd", false), stopCount = p.getInt("count", 0),
        )
        val end = p.getLong("endAt", 0L)
        _state.value = if (end > System.currentTimeMillis()) TimerState(end, p.getLong("startedAt", 0L)) else TimerState()
    }

    fun updateSettings(ctx: Context, f: (Settings) -> Settings) {
        val s = f(_settings.value)
        _settings.value = s
        prefs(ctx).edit()
            .putInt("fade", s.fadeSeconds).putInt("last", s.lastMinutes).putBoolean("restore", s.restoreVolume)
            .putBoolean("shake", s.shakeToExtend).putBoolean("vibrate", s.vibrateAtEnd).putBoolean("dnd", s.dndAtEnd)
            .putInt("count", s.stopCount).apply()
    }

    val isRunning get() = _state.value.endAt > System.currentTimeMillis()

    fun start(ctx: Context, minutes: Int) {
        load(ctx)
        updateSettings(ctx) { it.copy(lastMinutes = minutes) }
        startAt(ctx, System.currentTimeMillis() + minutes * 60_000L)
    }

    fun startAt(ctx: Context, endAt: Long) {
        load(ctx)
        val now = System.currentTimeMillis()
        setState(ctx, TimerState(endAt = endAt, startedAt = now))
        ContextCompat.startForegroundService(ctx, Intent(ctx, TimerService::class.java).setAction(TimerService.ACTION_START))
        scheduleAlarm(ctx)
    }

    fun extend(ctx: Context, minutes: Int) {
        load(ctx)
        val st = _state.value
        if (st.endAt == 0L) { start(ctx, minutes); return }
        val newEnd = (st.endAt + minutes * 60_000L).coerceAtLeast(System.currentTimeMillis() + 60_000L)
        setState(ctx, st.copy(endAt = newEnd, fading = false))
        runCatching { ctx.startService(Intent(ctx, TimerService::class.java).setAction(TimerService.ACTION_UPDATE)) }
        scheduleAlarm(ctx)
    }

    fun cancel(ctx: Context) {
        clear(ctx)
        runCatching { ctx.startService(Intent(ctx, TimerService::class.java).setAction(TimerService.ACTION_CANCEL)) }
    }

    /** Remet l'état à zéro sans toucher au service. */
    fun clear(ctx: Context) {
        load(ctx)
        cancelAlarm(ctx)
        setState(ctx, TimerState(lastResult = null))
    }

    /** Coupe tout de suite (avec un fondu court). */
    fun stopNow(ctx: Context) {
        load(ctx)
        setState(ctx, _state.value.copy(endAt = System.currentTimeMillis() + 3_000L, fading = false))
        ContextCompat.startForegroundService(ctx, Intent(ctx, TimerService::class.java).setAction(TimerService.ACTION_START))
        scheduleAlarm(ctx)
    }

    internal fun setFading(ctx: Context, fading: Boolean) = setState(ctx, _state.value.copy(fading = fading))

    internal fun finished(ctx: Context, result: String) {
        cancelAlarm(ctx)
        setState(ctx, TimerState(lastResult = result))
        updateSettings(ctx) { it.copy(stopCount = it.stopCount + 1) }
    }

    private fun setState(ctx: Context, s: TimerState) {
        _state.value = s
        prefs(ctx).edit().putLong("endAt", s.endAt).putLong("startedAt", s.startedAt).apply()
        SleepTile.requestUpdate(ctx)
    }

    // ───── Alarme de secours : réveille le téléphone même en veille profonde (Doze) ─────

    fun canExact(ctx: Context): Boolean {
        if (Build.VERSION.SDK_INT < 31) return true
        return ctx.getSystemService(AlarmManager::class.java).canScheduleExactAlarms()
    }

    private fun alarmIntent(ctx: Context) = PendingIntent.getBroadcast(
        ctx, 7, Intent(ctx, AlarmReceiver::class.java), PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT,
    )

    /** Programme le réveil au début du fondu (le service fait le reste, verrou de réveil tenu). */
    fun scheduleAlarm(ctx: Context) {
        val st = _state.value
        if (st.endAt == 0L) return
        val am = ctx.getSystemService(AlarmManager::class.java)
        val fadeMs = _settings.value.fadeSeconds * 1000L
        val at = (st.endAt - fadeMs - 5_000L).coerceAtLeast(System.currentTimeMillis() + 500L)
        val pi = alarmIntent(ctx)
        if (canExact(ctx)) am.setExactAndAllowWhileIdle(AlarmManager.RTC_WAKEUP, at, pi)
        else am.setAndAllowWhileIdle(AlarmManager.RTC_WAKEUP, at, pi)
    }

    private fun cancelAlarm(ctx: Context) {
        ctx.getSystemService(AlarmManager::class.java).cancel(alarmIntent(ctx))
    }
}
