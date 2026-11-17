package com.superessence.noctea

import android.graphics.Bitmap
import androidx.compose.ui.test.junit4.createAndroidComposeRule
import com.superessence.noctea.ui.screens.SplashScreen
import com.superessence.noctea.ui.theme.NocteaTheme
import com.superessence.noctea.ui.theme.Palettes
import org.junit.Assume.assumeTrue
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config
import org.robolectric.annotation.GraphicsMode
import java.io.File

/** Images successives de l'écran de lancement (avec -Pscreenshots=true) : build/screenshots/splash/ */
@RunWith(RobolectricTestRunner::class)
@GraphicsMode(GraphicsMode.Mode.NATIVE)
@Config(sdk = [35], qualifiers = "w393dp-h852dp-mdpi")
class SplashFramesTest {
    @get:Rule val rule = createAndroidComposeRule<androidx.activity.ComponentActivity>()

    @Test
    fun frames() {
        assumeTrue(System.getProperty("noctea.screenshots") == "true")
        rule.mainClock.autoAdvance = false
        rule.setContent { NocteaTheme(Palettes.Nuit) { SplashScreen {} } }
        val dir = File("build/screenshots/splash").apply { mkdirs() }
        var t = 0L
        var n = 0
        while (t <= 9200L) {
            rule.mainClock.advanceTimeBy(if (t == 0L) 0L else STEP)
            rule.activityRule.scenario.onActivity { act ->
                val v = act.window.decorView.rootView
                val b = Bitmap.createBitmap(v.width, v.height, Bitmap.Config.ARGB_8888)
                v.draw(android.graphics.Canvas(b))
                File(dir, "f%03d.png".format(n)).outputStream().use { b.compress(Bitmap.CompressFormat.PNG, 100, it) }
            }
            t += STEP; n++
        }
    }

    private companion object { const val STEP = 40L }
}
