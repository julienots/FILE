@file:Suppress("unused", "UNUSED_PARAMETER")
package androidx.activity.result.contract
abstract class ActivityResultContract<I, O>
class ActivityResultContracts {
    open class CreateDocument(mimeType: String) : ActivityResultContract<String, android.net.Uri?>()
    open class OpenDocument : ActivityResultContract<Array<String>, android.net.Uri?>()
    open class PickVisualMedia : ActivityResultContract<androidx.activity.result.PickVisualMediaRequest, android.net.Uri?>() {
        sealed interface VisualMediaType
        object ImageOnly : VisualMediaType
    }
}
