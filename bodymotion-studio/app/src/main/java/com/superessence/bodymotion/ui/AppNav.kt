package com.superessence.bodymotion.ui

import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.core.tween
import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.navigation.NavHostController
import androidx.navigation.NavType
import androidx.navigation.compose.NavHost
import androidx.navigation.compose.composable
import androidx.navigation.compose.rememberNavController
import androidx.navigation.navArgument
import com.superessence.bodymotion.BodyMotionApp
import com.superessence.bodymotion.ui.screens.*

object Routes {
    const val INTRO = "intro"; const val HOME = "home"; const val CHARACTERS = "characters"; const val LIBRARY = "library"
    const val PROJECTS = "projects"; const val SETTINGS = "settings"
    fun character(id: String) = "character/$id"
    fun capture(id: String, video: Boolean = false) = "capture/$id?video=$video"
    fun editor(id: String) = "editor/$id"
    fun export(ids: List<String>) = "export/${ids.joinToString(",")}"
    fun project(id: String) = "project/$id"
}

@Composable
fun AppNav(start: String = Routes.INTRO) {
    val nav = rememberNavController()
    val store = BodyMotionApp.store
    val firstLaunch = remember { !store.settings.value.introSeen }
    NavHost(nav, startDestination = start, enterTransition = { fadeIn(tween(220)) }, exitTransition = { fadeOut(tween(180)) }) {
        composable(Routes.INTRO) {
            IntroScreen(full = firstLaunch) {
                store.updateSettings { it.copy(introSeen = true) }
                nav.navigate(Routes.HOME) { popUpTo(Routes.INTRO) { inclusive = true } }
            }
        }
        composable(Routes.HOME) { HomeScreen(nav) }
        composable("characters?import={import}", listOf(navArgument("import") { type = NavType.BoolType; defaultValue = false })) { CharactersScreen(nav, it.arguments!!.getBoolean("import")) }
        composable("character/{id}", listOf(navArgument("id") { type = NavType.StringType })) { CharacterDetailScreen(nav, it.arguments!!.getString("id")!!) }
        composable("capture/{id}?video={video}", listOf(navArgument("id") { type = NavType.StringType }, navArgument("video") { type = NavType.BoolType; defaultValue = false })) {
            CaptureScreen(nav, it.arguments!!.getString("id")!!, it.arguments!!.getBoolean("video"))
        }
        composable(Routes.LIBRARY) { LibraryScreen(nav) }
        composable("editor/{id}", listOf(navArgument("id") { type = NavType.StringType })) { EditorScreen(nav, it.arguments!!.getString("id")!!) }
        composable("export/{ids}", listOf(navArgument("ids") { type = NavType.StringType })) { ExportScreen(nav, it.arguments!!.getString("ids")!!.split(",").filter { s -> s.isNotBlank() }) }
        composable(Routes.PROJECTS) { ProjectsScreen(nav) }
        composable("project/{id}", listOf(navArgument("id") { type = NavType.StringType })) { ProjectDetailScreen(nav, it.arguments!!.getString("id")!!) }
        composable(Routes.SETTINGS) { SettingsScreen(nav) }
    }
}

fun NavHostController.go(route: String) = navigate(route) { launchSingleTop = true }

/** Onglets principaux : on revient à l'accueil plutôt que d'empiler. */
fun NavHostController.tab(route: String) = navigate(route) { popUpTo(Routes.HOME) { saveState = true }; launchSingleTop = true; restoreState = true }
