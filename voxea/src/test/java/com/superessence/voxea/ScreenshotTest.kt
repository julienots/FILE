package com.superessence.voxea

import android.app.Application
import android.graphics.Bitmap
import androidx.compose.ui.test.junit4.createAndroidComposeRule
import androidx.test.core.app.ApplicationProvider
import com.superessence.voxea.core.Wav
import com.superessence.voxea.core.VoiceType
import com.superessence.voxea.ui.AppViewModel
import com.superessence.voxea.ui.EditScreen
import com.superessence.voxea.ui.HomeScreen
import com.superessence.voxea.ui.IntroScreen
import com.superessence.voxea.ui.PrompterScreen
import com.superessence.voxea.ui.SettingsScreen
import com.superessence.voxea.ui.VoxeaTheme
import org.junit.Assume.assumeTrue
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.ParameterizedRobolectricTestRunner
import org.robolectric.annotation.Config
import org.robolectric.annotation.GraphicsMode
import java.io.File
import kotlin.math.sin

/** Captures (avec -Pscreenshots=true) dans voxea/build/screenshots. Sinon : rendu des écrans sans crash. */
@RunWith(ParameterizedRobolectricTestRunner::class)
@GraphicsMode(GraphicsMode.Mode.NATIVE)
@Config(sdk = [35], qualifiers = "w393dp-h852dp-xxhdpi")
class ScreenshotTest(private val name: String, private val route: String, private val atMs: Long) {
    companion object {
        @JvmStatic
        @ParameterizedRobolectricTestRunner.Parameters(name = "{0}")
        fun shots() = listOf(
            arrayOf("01_intro_studio", "intro", 1350L),
            arrayOf("02_intro_studio_fin", "intro", 2200L),
            arrayOf("03_chargement", "intro", 6500L),
            arrayOf("04_accueil", "home", 800L),
            arrayOf("05_voix", "cast", 800L),
            arrayOf("06_enregistrer", "record", 800L),
            arrayOf("07_prompteur", "prompter", 800L),
            arrayOf("08_prompteur_rec", "prompter_rec", 800L),
            arrayOf("09_mp3", "export", 800L),
            arrayOf("10_reglages", "settings", 800L),
        )
    }

    @get:Rule val rule = createAndroidComposeRule<androidx.activity.ComponentActivity>()

    @Test
    fun capture() {
        val app = ApplicationProvider.getApplicationContext<Application>()
        val vm = AppViewModel(app)
        vm.store.list().forEach { vm.store.delete(it) }
        val p0 = vm.importText(AppViewModel.EXAMPLE, "Exemple — La gare")!!
        var p = vm.setCharacterVoice(p0, "NARRATEUR", VoiceType.ORIGINAL).let { vm.project(p0.id)!! }
        val tone = FloatArray(22050) { (0.3 * sin(it * 0.05)).toFloat() }
        listOf(0, 1, 2).forEach { Wav.write(vm.store.recFile(p, it), tone, 44100) }
        if (route == "export") {
            com.superessence.voxea.core.Renderer(vm.store).render(p, com.superessence.voxea.core.LocalConverter)
        }
        vm.openPrompter(p)
        if (route == "prompter_rec") vm.prompterJump(3)
        rule.mainClock.autoAdvance = false
        rule.setContent {
            VoxeaTheme {
                when (route) {
                    "intro" -> IntroScreen(false, {}, {})
                    "home" -> HomeScreen(vm)
                    "cast" -> EditScreen(vm, p.id, 0)
                    "record" -> EditScreen(vm, p.id, 1)
                    "export" -> EditScreen(vm, p.id, 2)
                    "prompter", "prompter_rec" -> PrompterScreen(vm, p.id)
                    "settings" -> SettingsScreen(vm)
                }
            }
        }
        rule.mainClock.advanceTimeBy(atMs)
        if (System.getProperty("voxea.screenshots") != "true") return
        var bmp: Bitmap? = null
        rule.activityRule.scenario.onActivity { act ->
            val v = act.window.decorView.rootView
            val b = Bitmap.createBitmap(v.width, v.height, Bitmap.Config.ARGB_8888)
            v.draw(android.graphics.Canvas(b)); bmp = b
        }
        val dir = File("build/screenshots").apply { mkdirs() }
        File(dir, "$name.png").outputStream().use { bmp!!.compress(Bitmap.CompressFormat.PNG, 100, it) }
    }
}
