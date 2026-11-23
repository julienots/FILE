package com.superessence.squad

import android.graphics.Bitmap
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.background
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.asAndroidBitmap
import androidx.compose.ui.test.captureToImage
import androidx.compose.ui.test.junit4.createAndroidComposeRule
import androidx.compose.ui.test.onRoot
import androidx.test.core.app.ApplicationProvider
import com.superessence.squad.engine.Squad
import com.superessence.squad.model.Archetypes
import com.superessence.squad.ui.C
import com.superessence.squad.ui.DetailScreen
import com.superessence.squad.ui.HomeScreen
import com.superessence.squad.ui.RecruitScreen
import com.superessence.squad.ui.RhythmScreen
import com.superessence.squad.ui.StudioSplash
import com.superessence.squad.ui.StyleScreen
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config
import org.robolectric.annotation.GraphicsMode
import java.io.File

@RunWith(RobolectricTestRunner::class)
@GraphicsMode(GraphicsMode.Mode.NATIVE)
@Config(qualifiers = "w393dp-h851dp-xxhdpi")
class ScreensTest {
    @get:Rule val rule = createAndroidComposeRule<androidx.activity.ComponentActivity>()
    private val out = File("build/renders").apply { mkdirs() }

    private fun shot(name: String) {
        var bmp: Bitmap? = null
        rule.activityRule.scenario.onActivity { act ->
            val v = act.window.decorView.rootView
            val b = Bitmap.createBitmap(v.width, v.height, Bitmap.Config.ARGB_8888)
            v.draw(android.graphics.Canvas(b))
            bmp = b
        }
        File(out, "$name.png").outputStream().use { bmp!!.compress(Bitmap.CompressFormat.PNG, 100, it) }
    }

    private fun seed() {
        Squad.init(ApplicationProvider.getApplicationContext())
        if (Squad.state.value.mascots.isEmpty()) {
            listOf("tank", "ragnar", "boss", "kenji").forEach { Squad.add(Archetypes.byId(it)) }
            val r = Squad.state.value.mascots[1].id
            Squad.recordPr(r, "Squat", 160f, true); Squad.recordPr(r, "Développé couché", 122.5f, true)
        }
    }

    @Test fun splash() {
        rule.mainClock.autoAdvance = false
        rule.setContent { StudioSplash {} }
        rule.mainClock.advanceTimeBy(4200)
        shot("splash")
    }

    @Test fun home() {
        seed()
        rule.mainClock.autoAdvance = false
        rule.setContent { Box(Modifier.fillMaxSize().background(C.bg)) { HomeScreen(Squad.state.value, false, {}, {}, {}, {}) } }
        rule.mainClock.advanceTimeBy(1500)
        shot("home")
    }

    @Test fun detail() {
        seed()
        val id = Squad.state.value.mascots[1].id
        rule.mainClock.autoAdvance = false
        rule.setContent { Box(Modifier.fillMaxSize().background(C.bg)) { DetailScreen(Squad.state.value, id, {}, {}) } }
        rule.mainClock.advanceTimeBy(1500)
        shot("detail")
    }

    @Test fun recruit() {
        seed()
        rule.mainClock.autoAdvance = false
        rule.setContent { Box(Modifier.fillMaxSize().background(C.bg)) { RecruitScreen(Squad.state.value, {}, {}) } }
        rule.mainClock.advanceTimeBy(1500)
        shot("recruit")
    }

    @Test fun style() {
        seed()
        val id = Squad.state.value.mascots[0].id
        rule.mainClock.autoAdvance = false
        rule.setContent { Box(Modifier.fillMaxSize().background(C.bg)) { StyleScreen(Squad.state.value, id, {}) } }
        rule.mainClock.advanceTimeBy(1500)
        shot("style")
    }

    @Test fun rhythm() {
        seed()
        rule.mainClock.autoAdvance = false
        rule.setContent { Box(Modifier.fillMaxSize().background(C.bg)) { RhythmScreen(Squad.state.value, {}, {}) } }
        rule.mainClock.advanceTimeBy(1500)
        shot("rhythm")
    }
}
