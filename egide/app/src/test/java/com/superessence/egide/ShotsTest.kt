package com.superessence.egide

import android.graphics.Bitmap
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.asAndroidBitmap
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.test.captureToImage
import androidx.compose.ui.test.junit4.createAndroidComposeRule
import androidx.compose.ui.test.onNodeWithTag
import com.superessence.egide.ui.EgideTheme
import com.superessence.egide.ui.HomeScreen
import com.superessence.egide.ui.ListsScreen
import com.superessence.egide.ui.Splash
import org.junit.Assume.assumeTrue
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config
import org.robolectric.annotation.GraphicsMode
import java.io.File

/** Captures d'écran (lancer avec -Pshots=dossier). */
@RunWith(RobolectricTestRunner::class)
@GraphicsMode(GraphicsMode.Mode.NATIVE)
@Config(sdk = [34], qualifiers = "w411dp-h891dp-xxhdpi", application = android.app.Application::class)
class ShotsTest {
    @get:Rule val rule = createAndroidComposeRule<androidx.activity.ComponentActivity>()
    private val out = System.getProperty("egide.shots").orEmpty()

    private fun save(name: String) {
        var b: Bitmap? = null
        rule.activityRule.scenario.onActivity { act ->
            val v = act.window.decorView.rootView
            b = Bitmap.createBitmap(v.width, v.height, Bitmap.Config.ARGB_8888).also { v.draw(android.graphics.Canvas(it)) }
        }
        File(out).mkdirs()
        File(out, "$name.png").outputStream().use { b!!.compress(Bitmap.CompressFormat.PNG, 100, it) }
    }

    @Test fun splash() {
        assumeTrue(out.isNotEmpty())
        rule.mainClock.autoAdvance = false
        rule.setContent { EgideTheme { Box(Modifier.fillMaxSize().background(com.superessence.egide.ui.C.Ink).testTag("root")) { Splash {} } } }
        var t = 0L
        for ((ms, name) in listOf(700L to "splash-1", 1500L to "splash-2", 2350L to "splash-3")) {
            rule.mainClock.advanceTimeBy(ms - t); t = ms
            save(name)
        }
    }

    private fun initCore() {
        val ctx = androidx.test.core.app.ApplicationProvider.getApplicationContext<android.content.Context>()
        com.superessence.egide.core.Prefs.init(ctx)
        com.superessence.egide.core.Filter.init(ctx)
        com.superessence.egide.core.Filter.ensureLoaded()
    }

    @Test fun home() {
        assumeTrue(out.isNotEmpty())
        initCore()
        rule.mainClock.autoAdvance = false
        rule.setContent { EgideTheme { Box(Modifier.fillMaxSize().background(com.superessence.egide.ui.C.Ink).testTag("root")) { HomeScreen({}, {}) } } }
        rule.mainClock.advanceTimeBy(1000)
        save("home")
    }

    @Test fun lists() {
        assumeTrue(out.isNotEmpty())
        initCore()
        rule.mainClock.autoAdvance = false
        rule.setContent { EgideTheme { Box(Modifier.fillMaxSize().background(com.superessence.egide.ui.C.Ink).testTag("root")) { ListsScreen() } } }
        rule.mainClock.advanceTimeBy(1000)
        save("lists")
    }
}
