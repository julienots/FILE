@file:Suppress("unused", "UNUSED_PARAMETER")
package androidx.lifecycle.compose
import androidx.compose.runtime.Composable
import androidx.compose.runtime.State
import androidx.compose.runtime.collectAsState
@Composable fun <T> kotlinx.coroutines.flow.StateFlow<T>.collectAsStateWithLifecycle(): State<T> = collectAsState()
