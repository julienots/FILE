package com.superessence.sleepcut

import android.app.AlarmManager
import android.content.Context
import android.content.Intent
import android.graphics.Bitmap
import android.media.AudioManager
import android.os.Looper
import androidx.compose.ui.test.junit4.createAndroidComposeRule
import androidx.test.core.app.ApplicationProvider
import com.superessence.sleepcut.ui.MainScreen
import com.superessence.sleepcut.ui.SleepTheme
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Assume.assumeTrue
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.Robolectric
import org.robolectric.RobolectricTestRunner
import org.robolectric.Shadows.shadowOf
import org.robolectric.annotation.Config
import org.robolectric.annotation.GraphicsMode
import org.robolectric.annotation.LooperMode
import java.io.File

@RunWith(RobolectricTestRunner::class)
@LooperMode(LooperMode.Mode.PAUSED)
@GraphicsMode(GraphicsMode.Mode.NATIVE)
@Config(sdk = [35], qualifiers = "w393dp-h852dp-xxhdpi")
class SleepCutTest {
    @get:Rule val rule = createAndroidComposeRule<androidx.activity.ComponentActivity>()
    private val ctx: Context get() = ApplicationProvider.getApplicationContext()

    @Test
    fun startExtendCancelSchedulesWakeAlarm() {
        TimerController.load(ctx)
        TimerController.updateSettings(ctx) { it.copy(fadeSeconds = 30) }
        val before = System.currentTimeMillis()
        TimerController.start(ctx, 20)
        val end = TimerController.state.value.endAt
        assertTrue(end >= before + 20 * 60_000L && end <= System.currentTimeMillis() + 20 * 60_000L)
        val am = shadowOf(ctx.getSystemService(AlarmManager::class.java))
        val alarm = am.nextScheduledAlarm!!
        assertEquals((end - 35_000L).toDouble(), alarm.triggerAtTime.toDouble(), 50.0)
        // le service de premier plan a été démarré
        assertEquals(TimerService::class.java.name, shadowOf(ctx as android.app.Application).nextStartedService.component!!.className)

        TimerController.extend(ctx, 5)
        assertEquals(end + 5 * 60_000L, TimerController.state.value.endAt)
        TimerController.extend(ctx, -60)
        assertTrue(TimerController.state.value.endAt >= System.currentTimeMillis() + 59_000L)

        TimerController.cancel(ctx)
        assertEquals(0L, TimerController.state.value.endAt)
        assertEquals(null, am.nextScheduledAlarm)
        assertEquals(20, TimerController.settings.value.lastMinutes)
    }

    @Test
    fun serviceFadesVolumeThenStopsAndRestores() {
        val audio = ctx.getSystemService(AudioManager::class.java)
        audio.setStreamVolume(AudioManager.STREAM_MUSIC, 12, 0)
        TimerController.load(ctx)
        TimerController.updateSettings(ctx) { it.copy(fadeSeconds = 30, restoreVolume = true, shakeToExtend = false) }
        TimerController.startAt(ctx, System.currentTimeMillis() + 10_000L)
        val ctrl = Robolectric.buildService(TimerService::class.java, Intent(ctx, TimerService::class.java).setAction(TimerService.ACTION_START)).create().startCommand(0, 1)
        shadowOf(Looper.getMainLooper()).idle()
        // 10 s restantes sur 30 s de fondu → environ un tiers du volume
        val v = audio.getStreamVolume(AudioManager.STREAM_MUSIC)
        assertTrue("volume pendant le fondu = $v", v in 3..5)
        assertTrue(TimerController.state.value.fading)

        // fin : on force l'échéance
        TimerController.startAt(ctx, System.currentTimeMillis() - 1)
        ctrl.get().onStartCommand(Intent(ctx, TimerService::class.java).setAction(TimerService.ACTION_UPDATE), 0, 2)
        shadowOf(Looper.getMainLooper()).idle()
        Thread.sleep(4000) // MusicStopper attend la mise en pause des lecteurs
        shadowOf(Looper.getMainLooper()).idle()
        assertEquals(0L, TimerController.state.value.endAt)
        assertTrue(TimerController.state.value.lastResult!!.isNotBlank())
        assertEquals("volume restauré", 12, audio.getStreamVolume(AudioManager.STREAM_MUSIC))
    }

    @Test
    fun screenshots() {
        assumeTrue(System.getProperty("sleepcut.screenshots") == "true")
        TimerController.load(ctx)
        val actions = object : Actions {
            override fun start(minutes: Int) {}
            override fun startAt(endAt: Long) {}
            override fun extend(minutes: Int) {}
            override fun cancel() {}
            override fun stopNow() {}
            override fun settings(f: (Settings) -> Settings) {}
            override fun openNotificationAccess() {}
            override fun openDndAccess() {}
            override fun openBattery() {}
            override fun openExactAlarm() {}
            override fun batteryOk() = false
            override fun exactOk() = true
            override fun sessionOk() = true
            override fun dndOk() = false
            override fun nowPlaying() = listOf("Spotify" to "Nuit étoilée — Lo-fi Girl")
            override fun trackRemaining(): Long? = 154_000L
        }
        val state = androidx.compose.runtime.mutableStateOf(TimerState())
        rule.setContent { SleepTheme { MainScreen(state.value, Settings(lastMinutes = 45, stopCount = 12), 0, actions) } }
        for ((name, running) in listOf("s01_setup" to false, "s02_running" to true)) {
            val now = System.currentTimeMillis()
            state.value = if (running) TimerState(endAt = now + 23 * 60_000L + 41_000L, startedAt = now - 6 * 60_000L) else TimerState()
            rule.mainClock.advanceTimeBy(1500)
            var bmp: Bitmap? = null
            rule.activityRule.scenario.onActivity { act ->
                val v = act.window.decorView.rootView
                bmp = Bitmap.createBitmap(v.width, v.height, Bitmap.Config.ARGB_8888).also { v.draw(android.graphics.Canvas(it)) }
            }
            val dir = File("build/screenshots").apply { mkdirs() }
            File(dir, "$name.png").outputStream().use { bmp!!.compress(Bitmap.CompressFormat.PNG, 100, it) }
        }
    }
}
