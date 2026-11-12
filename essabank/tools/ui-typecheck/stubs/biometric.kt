@file:Suppress("unused", "UNUSED_PARAMETER")
package androidx.biometric
class BiometricManager private constructor() {
    fun canAuthenticate(authenticators: Int): Int = 0
    interface Authenticators { companion object { const val BIOMETRIC_STRONG = 15; const val BIOMETRIC_WEAK = 255 } }
    companion object { const val BIOMETRIC_SUCCESS = 0; fun from(context: android.content.Context): BiometricManager = BiometricManager() }
}
class BiometricPrompt(activity: androidx.fragment.app.FragmentActivity, executor: java.util.concurrent.Executor, callback: AuthenticationCallback) {
    abstract class AuthenticationCallback {
        open fun onAuthenticationError(errorCode: Int, errString: CharSequence) {}
        open fun onAuthenticationSucceeded(result: AuthenticationResult) {}
        open fun onAuthenticationFailed() {}
    }
    class AuthenticationResult
    class PromptInfo { class Builder {
        fun setTitle(t: CharSequence) = this; fun setSubtitle(t: CharSequence?) = this; fun setNegativeButtonText(t: CharSequence) = this
        fun setAllowedAuthenticators(a: Int) = this; fun build(): PromptInfo = PromptInfo()
    } }
    fun authenticate(info: PromptInfo) {}
    companion object { const val ERROR_NEGATIVE_BUTTON = 13; const val ERROR_USER_CANCELED = 10 }
}
