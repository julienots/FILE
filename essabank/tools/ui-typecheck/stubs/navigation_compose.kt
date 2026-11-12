@file:Suppress("unused", "UNUSED_PARAMETER")
package androidx.navigation.compose
import androidx.compose.animation.AnimatedContentTransitionScope
import androidx.compose.animation.AnimatedVisibilityScope
import androidx.compose.animation.EnterTransition
import androidx.compose.animation.ExitTransition
import androidx.compose.runtime.Composable
import androidx.compose.runtime.State
import androidx.compose.ui.Modifier
import androidx.navigation.*
@Composable fun rememberNavController(): NavHostController = androidx.compose.runtime.remember { NavHostController() }
@Composable fun NavController.currentBackStackEntryAsState(): State<NavBackStackEntry?> = androidx.compose.runtime.remember { androidx.compose.runtime.mutableStateOf(null) }
@Composable fun NavHost(
    navController: NavHostController, startDestination: String, modifier: Modifier = Modifier,
    enterTransition: (AnimatedContentTransitionScope<NavBackStackEntry>.() -> EnterTransition) = { TODO() },
    exitTransition: (AnimatedContentTransitionScope<NavBackStackEntry>.() -> ExitTransition) = { TODO() },
    popEnterTransition: (AnimatedContentTransitionScope<NavBackStackEntry>.() -> EnterTransition) = enterTransition,
    popExitTransition: (AnimatedContentTransitionScope<NavBackStackEntry>.() -> ExitTransition) = exitTransition,
    builder: NavGraphBuilder.() -> Unit,
) {}
fun NavGraphBuilder.composable(route: String, arguments: List<NamedNavArgument> = emptyList(), content: @Composable AnimatedVisibilityScope.(NavBackStackEntry) -> Unit) {}
