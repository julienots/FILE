package com.superessence.essabank

import android.graphics.Bitmap
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.ui.Modifier
import androidx.compose.ui.test.junit4.createAndroidComposeRule
import androidx.navigation.compose.rememberNavController
import androidx.test.core.app.ApplicationProvider
import androidx.test.ext.junit.runners.AndroidJUnit4
import com.superessence.essabank.core.data.BankDatabase
import com.superessence.essabank.core.demo.DemoData
import com.superessence.essabank.core.model.TxKind
import com.superessence.essabank.data.AndroidSqlDriver
import com.superessence.essabank.data.InMemorySecuritySettings
import com.superessence.essabank.ui.AppViewModel
import com.superessence.essabank.ui.Nav
import com.superessence.essabank.ui.components.LocalPrefs
import com.superessence.essabank.ui.i18n.LocalLang
import com.superessence.essabank.ui.screens.*
import com.superessence.essabank.ui.theme.Essa
import com.superessence.essabank.ui.theme.EssaTheme
import kotlinx.coroutines.runBlocking
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.annotation.Config
import org.robolectric.annotation.GraphicsMode
import java.io.File

/**
 * Rend chaque écran avec le profil de démonstration (vrai moteur, vraie base SQLite Android)
 * et vérifie qu'aucun ne plante. Avec -Pscreenshots=true, enregistre une capture PNG
 * de chaque écran dans app/build/screenshots.
 */
@RunWith(AndroidJUnit4::class)
@GraphicsMode(GraphicsMode.Mode.NATIVE)
@Config(sdk = [35], qualifiers = "w411dp-h891dp-xxhdpi", application = android.app.Application::class)
class ScreensTest {
    @get:Rule val rule = createAndroidComposeRule<androidx.activity.ComponentActivity>()

    private val app: android.app.Application = ApplicationProvider.getApplicationContext()
    private val container = AppContainer(BankDatabase(AndroidSqlDriver(app, "screens.db")), InMemorySecuritySettings().apply { setPin("2580") })
    private val vm: AppViewModel = run {
        // Base remplie AVANT la création du ViewModel (pas de blocage du fil principal).
        container.database.replaceAll(DemoData.create(container.engine.clock, container.engine.ids, container.market))
        AppViewModel(app, container).also { awaitMain { it.state.value != null } }
    }
    private val shots = System.getProperty("essabank.screenshots") == "true"

    private fun screen(name: String, content: @Composable (Nav) -> Unit) {
        // Horloge manuelle : certaines animations sont infinies (effet de chargement, cartes flottantes).
        rule.mainClock.autoAdvance = false
        rule.setContent {
            val prefs = vm.state.value!!.profile.preferences
            val nav = Nav(rememberNavController())
            CompositionLocalProvider(LocalLang provides prefs.language, LocalPrefs provides prefs) {
                EssaTheme(prefs.theme, prefs.accent) {
                    Box(Modifier.fillMaxSize().background(Essa.colors.background)) { content(nav) }
                }
            }
        }
        repeat(60) { rule.mainClock.advanceTimeBy(50) }
        if (shots) {
            var bmp: Bitmap? = null
            rule.activityRule.scenario.onActivity { act ->
                val v = act.window.decorView.rootView
                bmp = Bitmap.createBitmap(v.width, v.height, Bitmap.Config.ARGB_8888).also { v.draw(android.graphics.Canvas(it)) }
            }
            val dir = File("build/screenshots").apply { mkdirs() }
            File(dir, "$name.png").outputStream().use { bmp!!.compress(Bitmap.CompressFormat.PNG, 100, it) }
        }
    }

    private val s get() = vm.state.value!!

    @Test fun accueil() = screen("04-home") { HomeScreen(vm, it) }
    @Test fun cartes() = screen("05-cards") { CardsScreen(vm, it) }
    @Test fun carte() = screen("06-card-detail") { CardDetailScreen(vm, it, s.cards.first().id) }
    @Test fun nouvelle_carte() = screen("07-new-card") { NewCardScreen(vm, it) }
    @Test fun paiements() = screen("08-payments") { PaymentsScreen(vm, it) }
    @Test fun virement() = screen("09-transfer") { TransferScreen(vm, it, "beneficiary", s.beneficiaries.first().id, null) }
    @Test fun statistiques() = screen("10-stats") { StatsScreen(vm, it) }
    @Test fun budgets() = screen("11-budgets") { BudgetsScreen(vm, it) }
    @Test fun comptes() = screen("12-accounts") { AccountsScreen(vm, it) }
    @Test fun compte() = screen("13-account") { AccountDetailScreen(vm, it, s.accounts.first().id) }
    @Test fun historique() = screen("14-history") { HistoryScreen(vm, it, null, null) }
    @Test fun transaction() = screen("15-transaction") { TransactionScreen(vm, it, s.transactions.last { t -> t.kind == TxKind.CARD_PAYMENT }.id) }
    @Test fun epargne() = screen("16-savings") { SavingsScreen(vm, it) }
    @Test fun objectif() = screen("17-goal") { GoalDetailScreen(vm, it, s.goals.first().id) }
    @Test fun investissements() = screen("18-invest") { InvestScreen(vm, it) }
    @Test fun actif() = screen("19-asset") { AssetScreen(vm, it, "ESC.SIM") }
    @Test fun credits() = screen("20-loans") { LoansScreen(vm, it) }
    @Test fun notifications() = screen("21-notifications") { NotificationsScreen(vm, it) }
    @Test fun profil() = screen("22-profile") { ProfileScreen(vm, it) }
    @Test fun preferences() = screen("23-preferences") { PreferencesScreen(vm, it) }
    @Test fun securite() = screen("24-security") { SecurityScreen(vm, it) {} }
    @Test fun donnees() = screen("25-data") { DataScreen(vm, it) }
    @Test fun formules() = screen("26-plans") { PlansScreen(vm, it) }
    @Test fun a_propos() = screen("27-about") { AboutScreen(it) }
    @Test fun beneficiaires() = screen("28-beneficiaries") { BeneficiariesScreen(vm, it) }
    @Test fun programmes() = screen("29-scheduled") { ScheduledScreen(vm, it) }
    @Test fun editeur_objectif() = screen("30-goal-editor") { GoalEditorScreen(vm, it, null) }
    @Test fun editeur_compte() = screen("31-account-editor") { AccountEditorScreen(vm, it, null) }
    @Test fun edition_profil() = screen("32-edit-profile") { EditProfileScreen(vm, it) }
    @Test fun tableau_de_bord() = screen("33-dashboard") { DashboardCustomScreen(vm, it) }
    @Test fun verrouillage() = screen("03-lock") { LockScreen(vm) }
    @Test fun accueil_onboarding() = screen("02-onboarding") { OnboardingScreen(vm) {} }
}
