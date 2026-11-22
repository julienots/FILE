package com.superessence.pulsar

import android.graphics.Bitmap
import androidx.compose.ui.test.junit4.createAndroidComposeRule
import com.superessence.pulsar.engine.GenParams
import com.superessence.pulsar.player.Player
import com.superessence.pulsar.ui.HomeScreen
import com.superessence.pulsar.ui.PulsarTheme
import com.superessence.pulsar.ui.StudioScreen
import org.junit.Assume.assumeTrue
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.ParameterizedRobolectricTestRunner
import org.robolectric.Shadows
import org.robolectric.annotation.Config
import org.robolectric.annotation.GraphicsMode
import org.robolectric.annotation.LooperMode
import java.io.File

@RunWith(ParameterizedRobolectricTestRunner::class)
@GraphicsMode(GraphicsMode.Mode.NATIVE)
@LooperMode(LooperMode.Mode.PAUSED)
@Config(sdk = [35], qualifiers = "w393dp-h852dp-xxhdpi")
class ScreenshotTest(private val name: String, private val screen: String, private val genre: String, private val tab: Int) {
    companion object {
        @JvmStatic
        @ParameterizedRobolectricTestRunner.Parameters(name = "{0}")
        fun shots() = listOf(
            arrayOf("p01_home", "home", "lofi", 0),
            arrayOf("p02_studio_lofi", "studio", "lofi", 0),
            arrayOf("p03_studio_trap", "studio", "trap", 0),
            arrayOf("p04_studio_hardstyle", "studio", "hardstyle", 0),
            arrayOf("p05_fx", "studio", "synthwave", 1),
            arrayOf("p06_compo", "studio", "dubstep", 2),
            arrayOf("p07_sleep", "studio", "ambient", 3),
            arrayOf("p08_home_mad", "home_mad", "trap", 0),
        )
    }

    @get:Rule val rule = createAndroidComposeRule<androidx.activity.ComponentActivity>()

    @Test
    fun capture() {
        assumeTrue(System.getProperty("pulsar.screenshots") == "true")
        Player.init(rule.activity)
        Player.generate(GenParams(genre, 0x1A2B3C), autoplay = false)
        Shadows.shadowOf(android.os.Looper.getMainLooper()).idle()
        Thread.sleep(500)
        Shadows.shadowOf(android.os.Looper.getMainLooper()).idle()
        // simule une position en cours de morceau
        val buf = FloatArray(2048 * 2)
        repeat(if (screen == "studio") 900 else 1) { Player.engine.render(buf, 2048) }
        if (tab > 0) org.robolectric.RuntimeEnvironment.setQualifiers("+w393dp-h1750dp")
        rule.mainClock.autoAdvance = false
        rule.setContent {
            PulsarTheme {
                val st = Player.state.value.copy(playing = true)
                if (screen.startsWith("home")) HomeScreen(st, if (screen == "home_mad") com.superessence.pulsar.engine.Family.MALADE else com.superessence.pulsar.engine.Family.CHILL) {} else StudioScreen(st, tab) {}
            }
        }
        rule.mainClock.advanceTimeBy(600)
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
