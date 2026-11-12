@file:Suppress("unused", "UNUSED_PARAMETER")
package androidx.core.view
class WindowInsetsControllerCompat { var isAppearanceLightStatusBars: Boolean = false; var isAppearanceLightNavigationBars: Boolean = false }
object WindowCompat { fun getInsetsController(window: android.view.Window, view: android.view.View): WindowInsetsControllerCompat = TODO() }
