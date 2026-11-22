package com.superessence.promzy

import android.graphics.Bitmap
import androidx.compose.ui.test.junit4.createAndroidComposeRule
import androidx.test.core.app.ApplicationProvider
import com.superessence.promzy.data.Feed
import com.superessence.promzy.ui.MainScreen
import com.superessence.promzy.ui.PromzyTheme
import com.superessence.promzy.ui.SplashScreen
import com.superessence.promzy.ui.Tab
import org.junit.Assume.assumeTrue
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.ParameterizedRobolectricTestRunner
import org.robolectric.annotation.Config
import org.robolectric.annotation.GraphicsMode
import java.io.File

@RunWith(ParameterizedRobolectricTestRunner::class)
@GraphicsMode(GraphicsMode.Mode.NATIVE)
@Config(sdk = [35], qualifiers = "w393dp-h852dp-xxhdpi")
class ScreenshotTest(private val name: String, private val route: String, private val atMs: Long) {
    companion object {
        @JvmStatic
        @ParameterizedRobolectricTestRunner.Parameters(name = "{0}")
        fun shots() = listOf(
            arrayOf("1_splash_studio", "splash", 2300L),
            arrayOf("2_splash_promzy", "splash", 5600L),
            arrayOf("3_top", "TOP", 1500L),
            arrayOf("4_live", "LIVE", 1500L),
            arrayOf("5_search", "SEARCH", 1500L),
            arrayOf("6_favs", "FAVS", 1500L),
        )
    }

    @get:Rule val rule = createAndroidComposeRule<androidx.activity.ComponentActivity>()

    @Test
    fun capture() {
        assumeTrue(System.getProperty("shots") == "true")
        val vm = PromzyViewModel(ApplicationProvider.getApplicationContext())
        val t = System.currentTimeMillis()
        vm.feeds.getValue(Feed.HOT).value = vm.feeds.getValue(Feed.HOT).value.copy(deals = SampleDeals, updatedAt = t - 8000)
        vm.feeds.getValue(Feed.NEW).value = vm.feeds.getValue(Feed.NEW).value.copy(deals = SampleDeals.reversed(), updatedAt = t - 8000, freshIds = setOf("7", "6"))
        if (route == "FAVS") { vm.favorites.toggle(SampleDeals[1]); vm.favorites.toggle(SampleDeals[4]) }
        if (route == "SEARCH") vm.recent.value = listOf("ps5", "airpods pro")
        rule.mainClock.autoAdvance = false
        rule.setContent { PromzyTheme { if (route == "splash") SplashScreen {} else MainScreen(vm, Tab.valueOf(route)) } }
        rule.mainClock.advanceTimeBy(atMs)
        var bmp: Bitmap? = null
        rule.activityRule.scenario.onActivity { act ->
            val v = act.window.decorView.rootView
            bmp = Bitmap.createBitmap(v.width, v.height, Bitmap.Config.ARGB_8888).also { v.draw(android.graphics.Canvas(it)) }
        }
        val dir = File("build/screenshots").apply { mkdirs() }
        File(dir, "$name.png").outputStream().use { bmp!!.compress(Bitmap.CompressFormat.PNG, 100, it) }
    }
}
