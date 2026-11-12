@file:Suppress("unused", "UNUSED_PARAMETER")
package androidx.core.content
import android.content.SharedPreferences
inline fun SharedPreferences.edit(commit: Boolean = false, action: SharedPreferences.Editor.() -> Unit) { val e = edit(); e.action(); e.apply() }
object ContextCompat { fun getMainExecutor(context: android.content.Context): java.util.concurrent.Executor = TODO() }
