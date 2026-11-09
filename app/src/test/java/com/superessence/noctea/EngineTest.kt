package com.superessence.noctea

import android.os.Looper
import androidx.media3.common.C
import androidx.test.core.app.ApplicationProvider
import com.superessence.noctea.audio.MediaIds
import com.superessence.noctea.data.CustomMeditation
import com.superessence.noctea.generator.GeneratorParams
import com.superessence.noctea.generator.Goal
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.Shadows.shadowOf
import org.robolectric.annotation.Config
import java.time.Duration

/** Moteur audio réel : timeline concaténée, durée exacte, ambiance, minuteur, file du rituel. */
@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35])
class EngineTest {
    private val app get() = ApplicationProvider.getApplicationContext<NocteaApp>()

    private fun waitDuration(): Long {
        val p = app.engine.player
        repeat(300) {
            shadowOf(Looper.getMainLooper()).idleFor(Duration.ofMillis(20))
            Thread.sleep(5)
            if (p.duration != C.TIME_UNSET && p.duration > 0) return p.duration
        }
        return p.duration
    }

    @Test
    fun trackTimelineMatchesCatalogDuration() {
        val t = app.catalog.track("sleep_deep")!!
        app.engine.play(MediaIds.track(t.id), resume = false)
        val d = waitDuration()
        assertEquals(MediaIds.track(t.id), app.engine.player.currentMediaItem?.mediaId)
        assertTrue("durée $d vs ${t.durationMs}", kotlin.math.abs(d - t.durationMs) < t.durationMs / 100)
        assertEquals(t.ambience.map { it.sound }.toSet(), app.engine.mixer.levels.value.keys)
        app.engine.stop()
    }

    @Test
    fun generatedMeditationPlaysWithExactLength() {
        val params = GeneratorParams(Goal.RELAX, 5)
        val meta = CustomMeditation("t1", 0, "Test", params, 300_000)
        app.store.edit { it.copy(customs = listOf(meta)) }
        val resolved = app.resolver.resolve(MediaIds.custom("t1"))
        assertNotNull(resolved)
        assertEquals(300_000L, resolved!!.durationMs)
        app.engine.play(MediaIds.custom("t1"), resume = false)
        val d = waitDuration()
        assertTrue("durée $d", kotlin.math.abs(d - 300_000) < 3_000)
        app.engine.stop()
    }

    @Test
    fun mixerAndRitualQueue() {
        app.store.edit { it.copy(lastMix = mapOf("rain_soft" to 0.5f, "piano" to 0.3f)) }
        app.engine.startMixer(30)
        val d = waitDuration()
        assertEquals(30 * 60_000L, d)
        assertEquals(setOf("rain_soft", "piano"), app.engine.mixer.levels.value.keys)
        app.engine.play(listOf(MediaIds.track("breath_478"), MediaIds.track("relax_calm"), MediaIds.mixer(20, "waves")), 0, resume = false)
        waitDuration()
        assertEquals(3, app.engine.player.mediaItemCount)
        app.engine.setSleepTimer(10)
        assertTrue(app.engine.ui.value.sleepTimerEndAt > 0)
        app.engine.cancelSleepTimer()
        app.engine.stop()
    }

    @Test
    fun allTracksResolve() {
        app.catalog.tracks.forEach { assertNotNull(app.resolver.resolve(MediaIds.track(it.id))) }
        assertTrue(app.resolver.resolve("unknown:x") == null)
    }
}
