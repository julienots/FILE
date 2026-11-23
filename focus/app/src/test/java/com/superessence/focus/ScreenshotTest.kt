package com.superessence.focus

import android.Manifest
import android.app.Application
import android.graphics.Bitmap
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.ui.Modifier
import androidx.compose.ui.test.junit4.createAndroidComposeRule
import androidx.test.core.app.ApplicationProvider
import com.superessence.focus.core.Session
import com.superessence.focus.core.Store
import com.superessence.focus.ui.BlockScreen
import com.superessence.focus.ui.C
import com.superessence.focus.ui.CardsScreen
import com.superessence.focus.ui.FocusTheme
import com.superessence.focus.ui.HomeScreen
import com.superessence.focus.ui.RomaneScreen
import com.superessence.focus.ui.Splash
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.ParameterizedRobolectricTestRunner
import org.robolectric.Shadows.shadowOf
import org.robolectric.annotation.Config
import org.robolectric.annotation.GraphicsMode
import java.io.File

/** Rend chaque écran (et plusieurs instants du lancement) dans build/screenshots. */
@RunWith(ParameterizedRobolectricTestRunner::class)
@GraphicsMode(GraphicsMode.Mode.NATIVE)
@Config(sdk = [35], qualifiers = "w393dp-h852dp-xxhdpi")
class ScreenshotTest(private val name: String, private val route: String, private val atMs: Long) {
    companion object {
        @JvmStatic
        @ParameterizedRobolectricTestRunner.Parameters(name = "{0}")
        fun shots() = listOf(
            arrayOf("01_splash_a", "splash", 900L),
            arrayOf("02_splash_b", "splash", 2700L),
            arrayOf("03_splash_c", "splash", 3600L),
            arrayOf("04_splash_logo", "splash", 6800L),
            arrayOf("05_home", "home", 800L),
            arrayOf("06_home_session", "home_run", 800L),
            arrayOf("07_cards", "cards", 800L),
            arrayOf("08_romane_setup", "romane", 800L),
            arrayOf("09_block", "block", 800L),
        )
    }

    @get:Rule val rule = createAndroidComposeRule<androidx.activity.ComponentActivity>()

    @Test
    fun capture() {
        val app = ApplicationProvider.getApplicationContext<Application>()
        Store.init(app)
        if (route == "romane") shadowOf(app).grantPermissions(Manifest.permission.READ_SMS, Manifest.permission.SEND_SMS, Manifest.permission.RECEIVE_SMS)
        if (route == "home_run") {
            Store.addMinutes(35)
            Session.start(app, 45, block = true, strict = false)
            Store.sessionStart = System.currentTimeMillis() - 12 * 60_000L
            Store.sessionEnd = System.currentTimeMillis() + 33 * 60_000L
        }
        rule.mainClock.autoAdvance = false
        rule.setContent {
            FocusTheme {
                Box(Modifier.fillMaxSize().background(C.Bg)) {
                    when (route) {
                        "splash" -> Splash {}
                        "home", "home_run" -> HomeScreen {}
                        "cards" -> CardsScreen()
                        "romane" -> RomaneScreen()
                        else -> BlockScreen()
                    }
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
