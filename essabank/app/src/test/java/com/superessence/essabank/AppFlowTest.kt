package com.superessence.essabank

import androidx.compose.ui.test.hasText
import androidx.compose.ui.test.junit4.createAndroidComposeRule
import androidx.compose.ui.test.onAllNodesWithText
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.performScrollToNode
import androidx.compose.ui.test.onFirst
import androidx.compose.ui.test.printToString
import androidx.compose.ui.test.performSemanticsAction
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

    /** Avance l'horloge manuellement (les animations infinies empêchent l'attente automatique). */
    private fun until(timeoutMs: Long = 15_000, cond: () -> Boolean) {
        var t = 0L
        while (!cond()) {
            check(t < timeoutMs) {
                val texts = Regex("Text = \\[([^\\]]*)\\]").findAll(rule.onAllNodes(androidx.compose.ui.test.isRoot()).onFirst().printToString(Int.MAX_VALUE)).map { it.groupValues[1] }.toList()
                "Condition non atteinte. Écran : " + texts.take(40).joinToString(" | ") + "\n" + rule.onAllNodes(androidx.compose.ui.test.isRoot()).fetchSemanticsNodes().size + " racine(s)\n" + rule.onAllNodes(androidx.compose.ui.test.isRoot()).onFirst().printToString(3).take(1500)
            }
            rule.mainClock.advanceTimeBy(100); t += 100
            org.robolectric.Shadows.shadowOf(android.os.Looper.getMainLooper()).idle()
            Thread.sleep(5)
        }
    }

    private fun has(text: String) = rule.onAllNodesWithText(text, substring = true).fetchSemanticsNodes().isNotEmpty()
    private fun waitText(text: String) = until { has(text) }

    private fun pin(code: String) = code.forEach { rule.onAllNodesWithText(it.toString()).onFirst().performClick() }

    @Test
    fun parcours_demo_virement_et_persistance() {
        rule.mainClock.autoAdvance = false
        // Introduction (toucher pour passer)
        until { has("Application de simulation") }
        rule.onNodeWithText("Application de simulation", substring = true).performClick()
        waitText("profil de démonstration")
        rule.onNodeWithText("Découvrir avec le profil de démonstration").performClick()

        waitText("Choisissez votre code PIN")
        pin("2580"); rule.onNodeWithText("Valider").performClick()
        waitText("Confirmez votre code PIN")
        pin("2580"); rule.onNodeWithText("Valider").performClick()
        until { has("Plus tard") || has("Solde principal") }
        if (has("Plus tard")) rule.onNodeWithText("Plus tard").performClick()

        waitText("Solde principal")
        rule.onNodeWithText("Revenus du mois").assertExists()

        // Virement interne via le moteur de l'application (même chemin que l'écran de confirmation).
        val vm = rule.activity.let { androidx.lifecycle.ViewModelProvider(it)[com.superessence.essabank.ui.AppViewModel::class.java] }
        val s = vm.state.value
        assertNotNull(s)
        val current = s!!.accounts.first { it.type == AccountType.CURRENT }
        val savings = s.accounts.first { it.type == AccountType.SAVINGS }
        var done = false
        vm.run(onOk = { done = true }) { transferInternal(it, current.id, savings.id, Money.units(100), "Test UI") }
        until { done }

        // Recréation de l'activité : le profil et le virement sont toujours là.
        rule.activityRule.scenario.recreate()
        until { true }
        val app = rule.activity.application as EssaApp
        val reloaded = app.container.database.load()!!
        assertEquals(current.balance - Money.units(100), reloaded.account(current.id)!!.balance)
        assertEquals(savings.balance + Money.units(100), reloaded.account(savings.id)!!.balance)
    }
}
