package com.superessence.noctea

import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.mutableStateOf
import androidx.compose.ui.test.junit4.createComposeRule
import androidx.compose.ui.test.onAllNodesWithText
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.performScrollTo
import androidx.test.core.app.ApplicationProvider
import com.superessence.noctea.audio.MediaIds
import com.superessence.noctea.ui.LocalApp
import com.superessence.noctea.ui.MainScaffold
import com.superessence.noctea.ui.Routes
import com.superessence.noctea.ui.screens.SplashScreen
import com.superessence.noctea.ui.theme.NocteaTheme
import com.superessence.noctea.ui.theme.Palettes
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.ParameterizedRobolectricTestRunner
import org.robolectric.annotation.Config
import org.robolectric.annotation.GraphicsMode

/** Rend chaque écran avec les vraies données de l'application : aucun plantage, aucun écran vide. */
@RunWith(ParameterizedRobolectricTestRunner::class)
@GraphicsMode(GraphicsMode.Mode.NATIVE)
@Config(sdk = [35], qualifiers = "w411dp-h891dp-xxhdpi")
class ScreensTest(private val route: String, private val expected: String) {

    companion object {
        @JvmStatic
        @ParameterizedRobolectricTestRunner.Parameters(name = "{0}")
        fun routes() = listOf(
            arrayOf(Routes.HOME, "Commencer ma méditation"),
            arrayOf(Routes.EXPLORE, "Explorer"),
            arrayOf(Routes.CREATE, "Générer ma méditation"),
            arrayOf(Routes.FAVORITES, "Favoris"),
            arrayOf(Routes.PROFILE, "Mon rituel du soir"),
            arrayOf(Routes.MIXER, "Mixeur nocturne"),
            arrayOf(Routes.RITUAL, "Commencer mon rituel"),
            arrayOf(Routes.JOURNAL, "Journal du soir"),
            arrayOf(Routes.SEARCH, "Suggestions"),
            arrayOf(Routes.SETTINGS, "Mode nuit automatique"),
            arrayOf(Routes.VOICE, "Voix NOCTÉA (préenregistrée)"),
            arrayOf(Routes.ABOUT, "Superessence Studio — éditeur de NOCTÉA"),
            arrayOf(Routes.PREMIUM, "NOCTÉA Premium"),
            arrayOf(Routes.STATS, "Mes statistiques"),
            arrayOf(Routes.HISTORY, "Historique d'écoute"),
            arrayOf(Routes.CUSTOM_TEXT, "Créer l'audio"),
            arrayOf("category/histoires", "Une cabane au milieu de la forêt"),
            arrayOf(Routes.PLAYER, "Aucune séance en cours"),
        )
    }

    @get:Rule val rule = createComposeRule()

    @Test
    fun rendersWithoutCrash() {
        val app = ApplicationProvider.getApplicationContext<NocteaApp>()
        app.store.settings { it.copy(onboardingDone = true, userName = "Test") }
        rule.mainClock.autoAdvance = false
        rule.setContent {
            NocteaTheme(Palettes.Nuit) {
                CompositionLocalProvider(LocalApp provides app) {
                    MainScaffold(mutableStateOf(null), start = route)
                }
            }
        }
        rule.mainClock.advanceTimeBy(1500)
        assertTrue("« $expected » absent sur $route", rule.onAllNodesWithText(expected, substring = true, useUnmergedTree = true).fetchSemanticsNodes().isNotEmpty())
    }
}

@RunWith(org.robolectric.RobolectricTestRunner::class)
@GraphicsMode(GraphicsMode.Mode.NATIVE)
@Config(sdk = [35], qualifiers = "w411dp-h891dp-xxhdpi")
class FlowsTest {
    @get:Rule val rule = createComposeRule()

    private fun app() = ApplicationProvider.getApplicationContext<NocteaApp>().also {
        it.store.settings { s -> s.copy(onboardingDone = true) }
    }

    @Test
    fun splashRevealsStudioThenFinishes() {
        var done = false
        rule.mainClock.autoAdvance = false
        rule.setContent { NocteaTheme(Palettes.Nuit) { SplashScreen { done = true } } }
        rule.mainClock.advanceTimeBy(3500)
        rule.onNodeWithText("S T U D I O", useUnmergedTree = true).assertExists()
        rule.mainClock.advanceTimeBy(9000)
        assertTrue(done)
    }

    @Test
    fun generatorCreatesAndPlaysAMeditation() {
        val app = app()
        rule.mainClock.autoAdvance = false
        rule.setContent {
            NocteaTheme(Palettes.Nuit) { CompositionLocalProvider(LocalApp provides app) { MainScaffold(mutableStateOf(null), start = Routes.CREATE) } }
        }
        rule.mainClock.advanceTimeBy(500)
        rule.onNodeWithText("Générer ma méditation").performScrollTo().performClick()
        rule.mainClock.advanceTimeBy(3000)
        assertTrue(app.store.value.customs.isNotEmpty())
        rule.onNodeWithText("Écouter").performScrollTo().performClick()
        rule.mainClock.advanceTimeBy(1500)
        val ui = app.engine.ui.value
        assertTrue(ui.playable?.mediaId == MediaIds.custom(app.store.value.customs.first().id))
        rule.onAllNodesWithText("EN ÉCOUTE", useUnmergedTree = true).fetchSemanticsNodes().isNotEmpty().let(::assertTrue)
        app.engine.stop()
    }

    @Test
    fun playerShowsBreathingGuideAndControls() {
        val app = app()
        app.engine.play(MediaIds.track("breath_coherence"), resume = false)
        app.engine.seekTo(65_000)
        rule.mainClock.autoAdvance = false
        rule.setContent {
            NocteaTheme(Palettes.Nuit) { CompositionLocalProvider(LocalApp provides app) { MainScaffold(mutableStateOf(null), start = Routes.PLAYER) } }
        }
        rule.mainClock.advanceTimeBy(1500)
        rule.onNodeWithText("Cohérence cardiaque").assertExists()
        rule.onNodeWithText("Minuteur").performClick()
        rule.mainClock.advanceTimeBy(1000)
        rule.onNodeWithText("15 min").performClick()
        rule.mainClock.advanceTimeBy(500)
        assertTrue(app.engine.ui.value.sleepTimerEndAt > System.currentTimeMillis())
        app.engine.stop()
    }
}
