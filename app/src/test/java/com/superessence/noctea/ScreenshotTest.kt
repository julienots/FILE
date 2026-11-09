package com.superessence.noctea

import android.graphics.Bitmap
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.mutableStateOf
import androidx.compose.ui.test.junit4.createAndroidComposeRule
import androidx.test.core.app.ApplicationProvider
import com.superessence.noctea.audio.MediaIds
import com.superessence.noctea.ui.LocalApp
import com.superessence.noctea.ui.MainScaffold
import com.superessence.noctea.ui.screens.SplashScreen
import com.superessence.noctea.ui.theme.NocteaTheme
import com.superessence.noctea.ui.theme.Palettes
import org.junit.Assume.assumeTrue
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.ParameterizedRobolectricTestRunner
import org.robolectric.annotation.Config
import org.robolectric.annotation.GraphicsMode
import java.io.File

/** Captures d'écran (lancées uniquement avec -Pscreenshots=true) : dossier build/screenshots */
@RunWith(ParameterizedRobolectricTestRunner::class)
@GraphicsMode(GraphicsMode.Mode.NATIVE)
@Config(sdk = [35], qualifiers = "w393dp-h852dp-xxhdpi")
class ScreenshotTest(private val name: String, private val route: String, private val atMs: Long) {
    companion object {
        @JvmStatic
        @ParameterizedRobolectricTestRunner.Parameters(name = "{0}")
        fun shots() = listOf(
            arrayOf("01_splash_studio_a", "splash", 1600L),
            arrayOf("02_splash_studio_b", "splash", 4700L),
            arrayOf("03_splash_noctea", "splash", 9500L),
            arrayOf("04_home", "home", 1200L),
            arrayOf("05_explore", "explore", 1200L),
            arrayOf("06_create", "create", 1200L),
            arrayOf("07_player_breath", "player", 1200L),
            arrayOf("08_mixer", "mixer", 1200L),
            arrayOf("09_ritual", "ritual", 1200L),
            arrayOf("10_profile", "profile", 1200L),
            arrayOf("11_about", "about", 1200L),
            arrayOf("12_favorites", "favorites", 1200L),
            arrayOf("13_stats", "stats", 1200L),
        )
    }

    @get:Rule val rule = createAndroidComposeRule<androidx.activity.ComponentActivity>()

    @Test
    fun capture() {
        assumeTrue(System.getProperty("noctea.screenshots") == "true")
        val app = ApplicationProvider.getApplicationContext<NocteaApp>()
        app.store.settings { it.copy(onboardingDone = true, userName = "Léa", autoNight = false) }
        if (route == "player") { app.engine.play(MediaIds.track("breath_coherence"), resume = false); app.engine.seekTo(62_000) }
        if (route == "favorites") listOf("sleep_deep", "story_cabin", "breath_478").forEach { app.store.toggleFavorite(MediaIds.track(it)) }
        if (route == "stats") app.store.edit { s -> s.copy(practiceDays = (1..9).associate { "2026-11-%02d".format(it) to it * 150_000L }) }
        rule.mainClock.autoAdvance = false
        rule.setContent {
            NocteaTheme(Palettes.Nuit) {
                CompositionLocalProvider(LocalApp provides app) {
                    if (route == "splash") SplashScreen {} else MainScaffold(mutableStateOf(null), start = route)
                }
            }
        }
        rule.mainClock.advanceTimeBy(atMs)
        var bmp: Bitmap? = null
        rule.activityRule.scenario.onActivity { act ->
            val v = act.window.decorView.rootView
            val b = Bitmap.createBitmap(v.width, v.height, Bitmap.Config.ARGB_8888)
            v.draw(android.graphics.Canvas(b))
            bmp = b
        }
        val dir = File("build/screenshots").apply { mkdirs() }
        File(dir, "$name.png").outputStream().use { bmp!!.compress(Bitmap.CompressFormat.PNG, 100, it) }
    }
}
