package com.superessence.essabank

import androidx.compose.ui.test.hasText
import androidx.compose.ui.test.junit4.createAndroidComposeRule
import androidx.compose.ui.test.onAllNodesWithText
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.performScrollToNode
import androidx.compose.ui.test.onFirst
import androidx.compose.ui.test.hasScrollAction
import androidx.test.ext.junit.runners.AndroidJUnit4
import com.superessence.essabank.core.model.AccountType
import com.superessence.essabank.core.money.Money
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.annotation.Config

/**
 * Parcours complet dans la vraie activité : introduction, profil de démonstration, création du PIN,
 * tableau de bord, virement simulé, puis recréation de l'activité (les données doivent subsister).
 */
@RunWith(AndroidJUnit4::class)
@Config(sdk = [35], qualifiers = "w411dp-h891dp-xxhdpi")
class AppFlowTest {
    @get:Rule val rule = createAndroidComposeRule<MainActivity>()

    private fun waitText(text: String, timeout: Long = 10_000) =
        rule.waitUntil(timeout) { rule.onAllNodesWithText(text, substring = true).fetchSemanticsNodes().isNotEmpty() }

    private fun pin(code: String) = code.forEach { rule.onAllNodesWithText(it.toString()).onFirst().performClick() }

    @Test
    fun parcours_demo_virement_et_persistance() {
        // Introduction (toucher pour passer)
        rule.mainClock.advanceTimeBy(500)
        rule.onNodeWithText("Application de simulation", substring = true).performClick()
        waitText("profil de démonstration")
        rule.onNodeWithText("Découvrir avec le profil de démonstration").performClick()

        waitText("Choisissez votre code PIN")
        pin("2580"); rule.onNodeWithText("Valider").performClick()
        waitText("Confirmez votre code PIN")
        pin("2580"); rule.onNodeWithText("Valider").performClick()
        rule.waitUntil(10_000) {
            rule.onAllNodesWithText("Plus tard").fetchSemanticsNodes().isNotEmpty() ||
                rule.onAllNodesWithText("Solde principal").fetchSemanticsNodes().isNotEmpty()
        }
        if (rule.onAllNodesWithText("Plus tard").fetchSemanticsNodes().isNotEmpty()) rule.onNodeWithText("Plus tard").performClick()

        waitText("Solde principal")
        rule.onNodeWithText("Revenus du mois").assertExists()

        // Virement interne via le moteur de l'application (même chemin que l'écran de confirmation).
        val vm = rule.activity.let { androidx.lifecycle.ViewModelProvider(it)[com.superessence.essabank.ui.AppViewModel::class.java] }
        val s = vm.state.value
        assertNotNull(s)
        val current = s!!.accounts.first { it.type == AccountType.CURRENT }
        val savings = s.accounts.first { it.type == AccountType.SAVINGS }
        kotlinx.coroutines.runBlocking { vm.exec { transferInternal(it, current.id, savings.id, Money.units(100), "Test UI") } }
        rule.waitForIdle()

        // Recréation de l'activité : le profil et le virement sont toujours là.
        rule.activityRule.scenario.recreate()
        rule.waitForIdle()
        val app = rule.activity.application as EssaApp
        val reloaded = app.container.database.load()!!
        assertEquals(current.balance - Money.units(100), reloaded.account(current.id)!!.balance)
        assertEquals(savings.balance + Money.units(100), reloaded.account(savings.id)!!.balance)
    }
}
