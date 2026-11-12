@file:Suppress("unused", "UNUSED_PARAMETER")
package androidx.activity.compose
import androidx.compose.runtime.Composable
import androidx.activity.result.contract.ActivityResultContract
fun androidx.activity.ComponentActivity.setContent(content: @Composable () -> Unit) {}
@Composable fun BackHandler(enabled: Boolean = true, onBack: () -> Unit) {}
class ManagedActivityResultLauncher<I, O> { fun launch(input: I) {} }
@Composable fun <I, O> rememberLauncherForActivityResult(contract: ActivityResultContract<I, O>, onResult: (O) -> Unit): ManagedActivityResultLauncher<I, O> = androidx.compose.runtime.remember { ManagedActivityResultLauncher() }
