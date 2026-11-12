@file:Suppress("unused", "UNUSED_PARAMETER")
package androidx.lifecycle
open class ViewModel { val stubScope = kotlinx.coroutines.CoroutineScope(kotlinx.coroutines.SupervisorJob() + kotlinx.coroutines.Dispatchers.Unconfined) }
open class AndroidViewModel(application: android.app.Application) : ViewModel()
val ViewModel.viewModelScope: kotlinx.coroutines.CoroutineScope get() = stubScope
