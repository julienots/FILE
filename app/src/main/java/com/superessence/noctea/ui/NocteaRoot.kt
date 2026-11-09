package com.superessence.noctea.ui

import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.Crossfade
import androidx.compose.animation.core.tween
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.slideInVertically
import androidx.compose.animation.slideOutVertically
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
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
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.AutoAwesome
import androidx.compose.material.icons.filled.Close
import androidx.compose.material.icons.filled.Explore
import androidx.compose.material.icons.filled.Favorite
import androidx.compose.material.icons.filled.Home
import androidx.compose.material.icons.filled.Pause
import androidx.compose.material.icons.filled.Person
import androidx.compose.material.icons.filled.PlayArrow
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.runtime.staticCompositionLocalOf
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.navigation.NavHostController
import androidx.navigation.NavType
import androidx.navigation.compose.NavHost
import androidx.navigation.compose.composable
import androidx.navigation.compose.currentBackStackEntryAsState
import androidx.navigation.compose.rememberNavController
import androidx.navigation.navArgument
import com.superessence.noctea.MainActivity
import com.superessence.noctea.NocteaApp
import com.superessence.noctea.ui.components.ArtIllustration
import com.superessence.noctea.ui.screens.AboutScreen
import com.superessence.noctea.ui.screens.CategoryScreen
import com.superessence.noctea.ui.screens.CreateScreen
import com.superessence.noctea.ui.screens.CustomTextScreen
import com.superessence.noctea.ui.screens.ExploreScreen
import com.superessence.noctea.ui.screens.FavoritesScreen
import com.superessence.noctea.ui.screens.HistoryScreen
import com.superessence.noctea.ui.screens.HomeScreen
import com.superessence.noctea.ui.screens.JournalScreen
import com.superessence.noctea.ui.screens.MixerScreen
import com.superessence.noctea.ui.screens.OnboardingScreen
import com.superessence.noctea.ui.screens.PlayerScreen
import com.superessence.noctea.ui.screens.PremiumScreen
import com.superessence.noctea.ui.screens.ProfileScreen
import com.superessence.noctea.ui.screens.RitualScreen
import com.superessence.noctea.ui.screens.SearchScreen
import com.superessence.noctea.ui.screens.SettingsScreen
import com.superessence.noctea.ui.screens.SplashScreen
import com.superessence.noctea.ui.screens.StatsScreen
import com.superessence.noctea.ui.screens.VoiceSettingsScreen
import com.superessence.noctea.ui.theme.LocalPalette
import com.superessence.noctea.ui.theme.NocteaTheme
import com.superessence.noctea.ui.theme.Palettes
import kotlinx.coroutines.delay
import java.time.LocalTime

val LocalApp = staticCompositionLocalOf<NocteaApp> { error("app") }
val LocalNav = staticCompositionLocalOf<NavHostController> { error("nav") }

object Routes {
    const val HOME = "home"
    const val EXPLORE = "explore"
    const val CREATE = "create"
    const val FAVORITES = "favorites"
    const val PROFILE = "profile"
    const val PLAYER = "player"
    const val MIXER = "mixer"
    const val RITUAL = "ritual"
    const val JOURNAL = "journal"
    const val SEARCH = "search"
    const val SETTINGS = "settings"
    const val VOICE = "voice"
    const val ABOUT = "about"
    const val PREMIUM = "premium"
    const val STATS = "stats"
    const val HISTORY = "history"
    const val CUSTOM_TEXT = "customtext"
    fun category(id: String) = "category/$id"
}

private data class Tab(val route: String, val label: String, val icon: ImageVector)

private val tabs = listOf(
    Tab(Routes.HOME, "Accueil", Icons.Filled.Home),
    Tab(Routes.EXPLORE, "Explorer", Icons.Filled.Explore),
    Tab(Routes.CREATE, "Créer", Icons.Filled.AutoAwesome),
    Tab(Routes.FAVORITES, "Favoris", Icons.Filled.Favorite),
    Tab(Routes.PROFILE, "Profil", Icons.Filled.Person),
)

/** Heure courante (mise à jour chaque minute) pour le mode nuit automatique. */
@Composable
private fun currentHour(): Int {
    var h by remember { mutableIntStateOf(LocalTime.now().hour) }
    LaunchedEffect(Unit) { while (true) { delay(60_000); h = LocalTime.now().hour } }
    return h
}

@Composable
fun NocteaRoot(app: NocteaApp, activity: MainActivity, skipSplash: Boolean) {
    val snap by app.store.state.collectAsState()
    val hour = currentHour()
    val bedtimeHour = snap.ritual.bedtimeMinutes / 60
    val nightNow = snap.settings.autoNight && (hour >= ((bedtimeHour + 23) % 24).coerceAtLeast(20) || hour < 6)
    val base = Palettes.byName(snap.settings.theme)
    val palette = if (nightNow) Palettes.deepNight(base) else base
    var showSplash by rememberSaveable { mutableStateOf(!skipSplash) }

    NocteaTheme(palette) {
        CompositionLocalProvider(LocalApp provides app) {
            Crossfade(targetState = showSplash, animationSpec = tween(700), label = "splash") { splash ->
                if (splash) {
                    SplashScreen { showSplash = false }
                } else if (!snap.settings.onboardingDone) {
                    OnboardingScreen()
                } else {
                    MainScaffold(activity.pendingRoute)
                }
            }
        }
    }
}

@Composable
internal fun MainScaffold(pendingRoute: androidx.compose.runtime.MutableState<String?>, start: String = Routes.HOME) {
    val nav = rememberNavController()
    val app = LocalApp.current
    val entry by nav.currentBackStackEntryAsState()
    val route = entry?.destination?.route
    val pending by pendingRoute
    LaunchedEffect(pending) {
        pending?.let {
            when (it) {
                "player" -> if (app.engine.ui.value.playable != null) nav.navigate(Routes.PLAYER) { launchSingleTop = true }
                "ritual" -> nav.navigate(Routes.RITUAL) { launchSingleTop = true }
            }
            pendingRoute.value = null
        }
    }
    val showBars = route in tabs.map { it.route }
    CompositionLocalProvider(LocalNav provides nav) {
        Box(Modifier.fillMaxSize().background(LocalPalette.current.sky2)) {
            NavHost(
                nav, startDestination = start, modifier = Modifier.fillMaxSize(),
                enterTransition = { fadeIn(tween(450)) + slideInVertically(tween(450)) { it / 24 } },
                exitTransition = { fadeOut(tween(300)) },
                popEnterTransition = { fadeIn(tween(400)) },
                popExitTransition = { fadeOut(tween(300)) + slideOutVertically(tween(350)) { it / 24 } },
            ) {
                composable(Routes.HOME) { HomeScreen() }
                composable(Routes.EXPLORE) { ExploreScreen() }
                composable(Routes.CREATE) { CreateScreen() }
                composable(Routes.FAVORITES) { FavoritesScreen() }
                composable(Routes.PROFILE) { ProfileScreen() }
                composable(Routes.PLAYER) { PlayerScreen() }
                composable(Routes.MIXER) { MixerScreen() }
                composable(Routes.RITUAL) { RitualScreen() }
                composable(Routes.JOURNAL) { JournalScreen() }
                composable(Routes.SEARCH) { SearchScreen() }
                composable(Routes.SETTINGS) { SettingsScreen() }
                composable(Routes.VOICE) { VoiceSettingsScreen() }
                composable(Routes.ABOUT) { AboutScreen() }
                composable(Routes.PREMIUM) { PremiumScreen() }
                composable(Routes.STATS) { StatsScreen() }
                composable(Routes.HISTORY) { HistoryScreen() }
                composable(Routes.CUSTOM_TEXT) { CustomTextScreen() }
                composable("category/{id}", arguments = listOf(navArgument("id") { type = NavType.StringType })) {
                    CategoryScreen(it.arguments?.getString("id") ?: "sommeil")
                }
            }
            if (showBars) {
                Column(Modifier.align(Alignment.BottomCenter)) {
                    MiniPlayer()
                    Box(Modifier.fillMaxWidth().height(14.dp).background(Brush.verticalGradient(listOf(Color.Transparent, LocalPalette.current.sky1.copy(alpha = 0.97f)))))
                    BottomBar(route, nav)
                }
            }
        }
    }
}

@Composable
private fun BottomBar(route: String?, nav: NavHostController) {
    val p = LocalPalette.current
    Row(
        Modifier
            .fillMaxWidth()
            .background(Brush.verticalGradient(listOf(p.sky1.copy(alpha = 0.97f), p.sky2)))
            .navigationBarsPadding()
            .padding(top = 6.dp, bottom = 6.dp),
    ) {
        tabs.forEach { tab ->
            val selected = route == tab.route
            Column(
                Modifier
                    .weight(1f)
                    .clip(RoundedCornerShape(18.dp))
                    .clickable {
                        if (!selected) nav.navigate(tab.route) {
                            popUpTo(Routes.HOME) { saveState = true }
                            launchSingleTop = true
                            restoreState = true
                        }
                    }
                    .padding(vertical = 6.dp),
                horizontalAlignment = Alignment.CenterHorizontally,
            ) {
                Box(
                    Modifier
                        .clip(RoundedCornerShape(50))
                        .background(if (selected) p.accent.copy(alpha = 0.18f) else Color.Transparent)
                        .padding(horizontal = 16.dp, vertical = 4.dp),
                ) {
                    Icon(tab.icon, contentDescription = tab.label, tint = if (selected) p.moon else p.textDim, modifier = Modifier.size(22.dp))
                }
                Spacer(Modifier.height(2.dp))
                Text(tab.label, style = MaterialTheme.typography.labelSmall, color = if (selected) p.text else p.textDim)
            }
        }
    }
}

@Composable
private fun MiniPlayer() {
    val app = LocalApp.current
    val nav = LocalNav.current
    val ui by app.engine.ui.collectAsState()
    val p = LocalPalette.current
    val playable = ui.playable
    AnimatedVisibility(visible = playable != null, enter = fadeIn() + slideInVertically { it }, exit = fadeOut()) {
        if (playable == null) return@AnimatedVisibility
        Column(
            Modifier
                .padding(horizontal = 12.dp)
                .clip(RoundedCornerShape(22.dp))
                .background(Brush.horizontalGradient(listOf(p.sky0.copy(alpha = 0.97f), p.glow.copy(alpha = 0.55f))))
                .clickable { nav.navigate(Routes.PLAYER) { launchSingleTop = true } },
        ) {
            Row(Modifier.padding(8.dp), verticalAlignment = Alignment.CenterVertically) {
                Box(Modifier.size(46.dp).clip(RoundedCornerShape(14.dp))) { ArtIllustration(playable.art, Modifier.fillMaxSize()) }
                Spacer(Modifier.width(12.dp))
                Column(Modifier.weight(1f)) {
                    Text(playable.title, style = MaterialTheme.typography.titleSmall, maxLines = 1, overflow = TextOverflow.Ellipsis)
                    Text(playable.subtitle, style = MaterialTheme.typography.bodySmall, maxLines = 1, overflow = TextOverflow.Ellipsis)
                }
                IconButton(onClick = { app.engine.toggle() }) {
                    Icon(if (ui.isPlaying) Icons.Filled.Pause else Icons.Filled.PlayArrow, contentDescription = if (ui.isPlaying) "Pause" else "Lecture", tint = p.moon)
                }
                IconButton(onClick = { app.engine.stop() }) {
                    Icon(Icons.Filled.Close, contentDescription = "Arrêter", tint = p.textDim)
                }
            }
            LinearProgressIndicator(
                progress = { if (ui.durationMs > 0) ui.positionMs / ui.durationMs.toFloat() else 0f },
                modifier = Modifier.fillMaxWidth().height(2.dp),
                color = p.moon, trackColor = Color.Transparent,
            )
        }
    }
}
