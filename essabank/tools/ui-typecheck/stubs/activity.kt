@file:Suppress("unused", "UNUSED_PARAMETER")
package androidx.activity
open class ComponentActivity : android.app.Activity()
class SystemBarStyle private constructor() {
    companion object { fun dark(scrim: Int): SystemBarStyle = SystemBarStyle() }
}
fun ComponentActivity.enableEdgeToEdge(statusBarStyle: SystemBarStyle = SystemBarStyle.dark(0), navigationBarStyle: SystemBarStyle = SystemBarStyle.dark(0)) {}
inline fun <reified VM : androidx.lifecycle.ViewModel> ComponentActivity.viewModels(): Lazy<VM> = lazy { TODO() }
