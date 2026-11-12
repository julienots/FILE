@file:Suppress("unused", "UNUSED_PARAMETER")
package androidx.navigation
class NavDestination { val route: String? = null }
class NavBackStackEntry { val destination: NavDestination = NavDestination(); val arguments: android.os.Bundle? = null }
open class NavController { fun navigate(route: String, builder: NavOptionsBuilder.() -> Unit = {}) {}; fun popBackStack(): Boolean = true }
class NavHostController : NavController()
class PopUpToBuilder { var saveState: Boolean = false; var inclusive: Boolean = false }
class NavOptionsBuilder { var launchSingleTop = false; var restoreState = false; fun popUpTo(route: String, b: PopUpToBuilder.() -> Unit = {}) {} }
abstract class NavType<T> { companion object { val StringType: NavType<String?> = TODO() } }
class NavArgumentBuilder { var type: NavType<*>? = null; var defaultValue: Any? = null; var nullable = false }
class NamedNavArgument
fun navArgument(name: String, builder: NavArgumentBuilder.() -> Unit): NamedNavArgument = NamedNavArgument()
class NavGraphBuilder
