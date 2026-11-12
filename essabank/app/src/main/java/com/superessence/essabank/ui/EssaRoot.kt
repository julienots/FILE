package com.superessence.essabank.ui

import android.app.Activity
import androidx.compose.animation.AnimatedContent
import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.core.tween
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.scaleIn
import androidx.compose.animation.slideInHorizontally
import androidx.compose.animation.slideInVertically
import androidx.compose.animation.slideOutHorizontally
import androidx.compose.animation.slideOutVertically
import androidx.compose.animation.togetherWith
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.navigationBarsPadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.BarChart
import androidx.compose.material.icons.rounded.CreditCard
import androidx.compose.material.icons.rounded.Home
import androidx.compose.material.icons.rounded.Person
import androidx.compose.material.icons.rounded.SwapHoriz
import androidx.compose.material3.Icon
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.SideEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.platform.LocalView
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.core.view.WindowCompat
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import androidx.navigation.NavHostController
import androidx.navigation.NavType
import androidx.navigation.compose.NavHost
import androidx.navigation.compose.composable
import androidx.navigation.compose.currentBackStackEntryAsState
import androidx.navigation.compose.rememberNavController
import androidx.navigation.navArgument
import com.superessence.essabank.core.model.Preferences
import com.superessence.essabank.core.model.ThemeMode
import com.superessence.essabank.ui.components.LocalPrefs
import com.superessence.essabank.ui.components.MessageHost
import com.superessence.essabank.ui.components.SkeletonHome
import com.superessence.essabank.ui.i18n.LocalLang
import com.superessence.essabank.ui.i18n.tr
import com.superessence.essabank.ui.screens.AboutScreen
import com.superessence.essabank.ui.screens.AccountDetailScreen
import com.superessence.essabank.ui.screens.AccountEditorScreen
import com.superessence.essabank.ui.screens.AccountsScreen
import com.superessence.essabank.ui.screens.AssetScreen
import com.superessence.essabank.ui.screens.BeneficiariesScreen
import com.superessence.essabank.ui.screens.BudgetsScreen
import com.superessence.essabank.ui.screens.CardDetailScreen
import com.superessence.essabank.ui.screens.CardsScreen
import com.superessence.essabank.ui.screens.DashboardCustomScreen
import com.superessence.essabank.ui.screens.DataScreen
import com.superessence.essabank.ui.screens.EditProfileScreen
import com.superessence.essabank.ui.screens.GoalDetailScreen
import com.superessence.essabank.ui.screens.GoalEditorScreen
import com.superessence.essabank.ui.screens.HistoryScreen
import com.superessence.essabank.ui.screens.HomeScreen
import com.superessence.essabank.ui.screens.IntroScreen
import com.superessence.essabank.ui.screens.InvestScreen
import com.superessence.essabank.ui.screens.LoansScreen
import com.superessence.essabank.ui.screens.LockScreen
import com.superessence.essabank.ui.screens.NewCardScreen
import com.superessence.essabank.ui.screens.NotificationsScreen
import com.superessence.essabank.ui.screens.OnboardingScreen
import com.superessence.essabank.ui.screens.PaymentsScreen
import com.superessence.essabank.ui.screens.PinSetupScreen
import com.superessence.essabank.ui.screens.PlansScreen
import com.superessence.essabank.ui.screens.PreferencesScreen
import com.superessence.essabank.ui.screens.ProfileScreen
import com.superessence.essabank.ui.screens.SavingsScreen
import com.superessence.essabank.ui.screens.ScheduledScreen
import com.superessence.essabank.ui.screens.SecurityScreen
import com.superessence.essabank.ui.screens.StatsScreen
import com.superessence.essabank.ui.screens.TransactionScreen
import com.superessence.essabank.ui.screens.TransferScreen
import com.superessence.essabank.ui.theme.Essa
import com.superessence.essabank.ui.theme.EssaTheme
import com.superessence.essabank.ui.theme.EssaType

object Routes {
    const val HOME = "home"
    const val CARDS = "cards"
    const val PAYMENTS = "payments"
    const val STATS = "stats"
    const val PROFILE = "profile"
    const val ACCOUNTS = "accounts"
    const val NEW_ACCOUNT = "account_new"
    const val NEW_CARD = "card_new"
    const val BENEFICIARIES = "beneficiaries"
    const val SCHEDULED = "scheduled"
    const val SAVINGS = "savings"
    const val NEW_GOAL = "goal_new"
    const val INVEST = "invest"
    const val LOANS = "loans"
    const val NOTIFICATIONS = "notifications"
    const val BUDGETS = "budgets"
    const val EDIT_PROFILE = "profile_edit"
    const val PREFERENCES = "preferences"
    const val SECURITY = "security"
    const val DATA = "data"
    const val PLANS = "plans"
    const val ABOUT = "about"
    const val DASHBOARD = "dashboard_custom"

    fun account(id: String) = "account/$id"
    fun editAccount(id: String) = "account_edit/$id"
    fun card(id: String) = "card/$id"
    fun tx(id: String) = "tx/$id"
    fun goal(id: String) = "goal/$id"
    fun editGoal(id: String) = "goal_edit/$id"
    fun asset(symbol: String) = "asset/$symbol"
    fun history(account: String? = null, category: String? = null) =
        "history?account=${account ?: ""}&category=${category ?: ""}"
    fun transfer(mode: String = "internal", to: String? = null, from: String? = null) =
        "transfer?mode=$mode&to=${to ?: ""}&from=${from ?: ""}"

    val tabs = listOf(HOME, CARDS, PAYMENTS, STATS, PROFILE)
}

/** Navigation partagée par tous les écrans. */
class Nav(private val controller: NavHostController) {
    fun go(route: String) = controller.navigate(route) { launchSingleTop = true }
    fun back() { controller.popBackStack() }
    fun tab(route: String) = controller.navigate(route) {
        popUpTo(Routes.HOME) { saveState = true }
        launchSingleTop = true
        restoreState = true
    }
    /** Remplace l'écran courant (ex. : formulaire → reçu). */
    fun replace(route: String) {
        controller.popBackStack()
        controller.navigate(route) { launchSingleTop = true }
    }
}

@Composable
fun EssaRoot(vm: AppViewModel, onSecureChanged: (Boolean) -> Unit) {
    val state by vm.state.collectAsStateWithLifecycle()
    val loaded by vm.loaded.collectAsStateWithLifecycle()
    val locked by vm.locked.collectAsStateWithLifecycle()
    val message by vm.message.collectAsStateWithLifecycle()
    val pinReady by vm.pinReady.collectAsStateWithLifecycle()
    val prefs = state?.profile?.preferences ?: Preferences(theme = ThemeMode.DARK)
    var introDone by rememberSaveable { mutableStateOf(false) }

    CompositionLocalProvider(LocalLang provides prefs.language, LocalPrefs provides prefs) {
        EssaTheme(prefs.theme, prefs.accent) {
            val colors = Essa.colors
            val view = LocalView.current
            SideEffect {
                val window = (view.context as? Activity)?.window ?: return@SideEffect
                WindowCompat.getInsetsController(window, view).apply {
                    isAppearanceLightStatusBars = !colors.isDark
                    isAppearanceLightNavigationBars = !colors.isDark
                }
            }
            Box(Modifier.fillMaxSize().background(colors.background)) {
                val screen = when {
                    !introDone -> "intro"
                    !loaded -> "loading"
                    state == null -> "onboarding"
                    !pinReady -> "pin"
                    locked -> "lock"
                    else -> "app"
                }
                AnimatedContent(
                    targetState = screen,
                    transitionSpec = { (fadeIn(tween(420)) + scaleIn(initialScale = 0.98f, animationSpec = tween(420))) togetherWith fadeOut(tween(250)) },
                    label = "root",
                ) { s ->
                    when (s) {
                        "intro" -> IntroScreen(onDone = { introDone = true })
                        "loading" -> SkeletonHome()
                        "onboarding" -> OnboardingScreen(vm, onCreated = { })
                        "pin" -> PinSetupScreen(vm, onDone = { vm.finishPinSetup() })
                        "lock" -> LockScreen(vm)
                        else -> MainNavigation(vm, onSecureChanged)
                    }
                }
                MessageHost(message) { vm.consumeMessage(it) }
            }
        }
    }
}

@Composable
private fun MainNavigation(vm: AppViewModel, onSecureChanged: (Boolean) -> Unit) {
    val controller = rememberNavController()
    val nav = remember(controller) { Nav(controller) }
    val entry by controller.currentBackStackEntryAsState()
    val route = entry?.destination?.route
    val showBar = route in Routes.tabs
    val c = Essa.colors

    Box(Modifier.fillMaxSize()) {
        NavHost(
            controller, startDestination = Routes.HOME, modifier = Modifier.fillMaxSize(),
            enterTransition = { if (targetState.destination.route in Routes.tabs && initialState.destination.route in Routes.tabs) fadeIn(tween(220)) else slideInHorizontally(tween(320)) { it / 3 } + fadeIn(tween(320)) },
            exitTransition = { if (targetState.destination.route in Routes.tabs && initialState.destination.route in Routes.tabs) fadeOut(tween(180)) else fadeOut(tween(200)) },
            popEnterTransition = { fadeIn(tween(260)) },
            popExitTransition = { slideOutHorizontally(tween(280)) { it / 3 } + fadeOut(tween(240)) },
        ) {
            composable(Routes.HOME) { HomeScreen(vm, nav) }
            composable(Routes.CARDS) { CardsScreen(vm, nav) }
            composable(Routes.PAYMENTS) { PaymentsScreen(vm, nav) }
            composable(Routes.STATS) { StatsScreen(vm, nav) }
            composable(Routes.PROFILE) { ProfileScreen(vm, nav) }

            composable(Routes.ACCOUNTS) { AccountsScreen(vm, nav) }
            composable("account/{id}") { AccountDetailScreen(vm, nav, it.arguments?.getString("id").orEmpty()) }
            composable(Routes.NEW_ACCOUNT) { AccountEditorScreen(vm, nav, null) }
            composable("account_edit/{id}") { AccountEditorScreen(vm, nav, it.arguments?.getString("id")) }
            composable("card/{id}") { CardDetailScreen(vm, nav, it.arguments?.getString("id").orEmpty()) }
            composable(Routes.NEW_CARD) { NewCardScreen(vm, nav) }
            composable(
                "transfer?mode={mode}&to={to}&from={from}",
                arguments = listOf(
                    navArgument("mode") { type = NavType.StringType; defaultValue = "internal" },
                    navArgument("to") { type = NavType.StringType; defaultValue = "" },
                    navArgument("from") { type = NavType.StringType; defaultValue = "" },
                ),
            ) {
                TransferScreen(vm, nav, it.arguments?.getString("mode") ?: "internal",
                    it.arguments?.getString("to")?.ifEmpty { null }, it.arguments?.getString("from")?.ifEmpty { null })
            }
            composable(Routes.BENEFICIARIES) { BeneficiariesScreen(vm, nav) }
            composable(Routes.SCHEDULED) { ScheduledScreen(vm, nav) }
            composable(
                "history?account={account}&category={category}",
                arguments = listOf(
                    navArgument("account") { type = NavType.StringType; defaultValue = "" },
                    navArgument("category") { type = NavType.StringType; defaultValue = "" },
                ),
            ) {
                HistoryScreen(vm, nav, it.arguments?.getString("account")?.ifEmpty { null }, it.arguments?.getString("category")?.ifEmpty { null })
            }
            composable("tx/{id}") { TransactionScreen(vm, nav, it.arguments?.getString("id").orEmpty()) }
            composable(Routes.SAVINGS) { SavingsScreen(vm, nav) }
            composable("goal/{id}") { GoalDetailScreen(vm, nav, it.arguments?.getString("id").orEmpty()) }
            composable(Routes.NEW_GOAL) { GoalEditorScreen(vm, nav, null) }
            composable("goal_edit/{id}") { GoalEditorScreen(vm, nav, it.arguments?.getString("id")) }
            composable(Routes.INVEST) { InvestScreen(vm, nav) }
            composable("asset/{symbol}") { AssetScreen(vm, nav, it.arguments?.getString("symbol").orEmpty()) }
            composable(Routes.LOANS) { LoansScreen(vm, nav) }
            composable(Routes.NOTIFICATIONS) { NotificationsScreen(vm, nav) }
            composable(Routes.BUDGETS) { BudgetsScreen(vm, nav) }
            composable(Routes.EDIT_PROFILE) { EditProfileScreen(vm, nav) }
            composable(Routes.PREFERENCES) { PreferencesScreen(vm, nav) }
            composable(Routes.SECURITY) { SecurityScreen(vm, nav, onSecureChanged) }
            composable(Routes.DATA) { DataScreen(vm, nav) }
            composable(Routes.PLANS) { PlansScreen(vm, nav) }
            composable(Routes.ABOUT) { AboutScreen(nav) }
            composable(Routes.DASHBOARD) { DashboardCustomScreen(vm, nav) }
        }

        AnimatedVisibility(
            visible = showBar, enter = slideInVertically { it } + fadeIn(), exit = slideOutVertically { it } + fadeOut(),
            modifier = Modifier.align(Alignment.BottomCenter),
        ) {
            BottomBar(route) { nav.tab(it) }
        }
    }
}

private data class TabItem(val route: String, val icon: ImageVector, val fr: String, val en: String)

@Composable
private fun BottomBar(current: String?, onSelect: (String) -> Unit) {
    val c = Essa.colors
    val items = listOf(
        TabItem(Routes.HOME, Icons.Rounded.Home, "Accueil", "Home"),
        TabItem(Routes.CARDS, Icons.Rounded.CreditCard, "Cartes", "Cards"),
        TabItem(Routes.PAYMENTS, Icons.Rounded.SwapHoriz, "Paiements", "Payments"),
        TabItem(Routes.STATS, Icons.Rounded.BarChart, "Statistiques", "Insights"),
        TabItem(Routes.PROFILE, Icons.Rounded.Person, "Profil", "Profile"),
    )
    Box(
        Modifier.fillMaxWidth().background(androidx.compose.ui.graphics.Brush.verticalGradient(listOf(Color.Transparent, c.background.copy(alpha = 0.92f), c.background)))
            .navigationBarsPadding().padding(start = 14.dp, end = 14.dp, top = 10.dp, bottom = 10.dp),
    ) {
        Row(
            Modifier.fillMaxWidth().height(66.dp).clip(RoundedCornerShape(24.dp)).background(c.elevated)
                .border(1.dp, c.border, RoundedCornerShape(24.dp)).padding(horizontal = 6.dp),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            items.forEach { item ->
                val selected = current == item.route
                Column(
                    Modifier.weight(1f).clip(RoundedCornerShape(18.dp))
                        .clickable(interactionSource = remember { MutableInteractionSource() }, indication = null) { onSelect(item.route) }
                        .padding(vertical = 6.dp),
                    horizontalAlignment = Alignment.CenterHorizontally,
                ) {
                    Box(
                        Modifier.size(width = 46.dp, height = 30.dp).clip(RoundedCornerShape(12.dp))
                            .background(if (selected) c.accentSoft else Color.Transparent),
                        contentAlignment = Alignment.Center,
                    ) {
                        Icon(item.icon, null, tint = if (selected) c.accent else c.textMuted, modifier = Modifier.size(23.dp))
                    }
                    Spacer(Modifier.height(2.dp))
                    Text(tr(item.fr, item.en), style = EssaType.caption.copy(letterSpacing = 0.sp), color = if (selected) c.text else c.textMuted, maxLines = 1)
                }
            }
        }
    }
}

