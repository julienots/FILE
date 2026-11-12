package com.superessence.essabank.security

import android.content.Context
import android.content.ContextWrapper
import androidx.biometric.BiometricManager
import androidx.biometric.BiometricPrompt
import androidx.core.content.ContextCompat
import androidx.fragment.app.FragmentActivity

/** Biométrie Android (empreinte, visage) : utilisée uniquement pour déverrouiller l'application localement. */
object Biometric {
    private const val AUTH = BiometricManager.Authenticators.BIOMETRIC_STRONG or BiometricManager.Authenticators.BIOMETRIC_WEAK

    fun isAvailable(context: Context): Boolean =
        BiometricManager.from(context).canAuthenticate(AUTH) == BiometricManager.BIOMETRIC_SUCCESS

    fun prompt(
        context: Context,
        title: String,
        subtitle: String,
        negative: String,
        onSuccess: () -> Unit,
        onFailure: (String?) -> Unit = {},
    ) {
        val activity = context.findActivity() ?: return onFailure(null)
        val prompt = BiometricPrompt(activity, ContextCompat.getMainExecutor(activity), object : BiometricPrompt.AuthenticationCallback() {
            override fun onAuthenticationSucceeded(result: BiometricPrompt.AuthenticationResult) = onSuccess()
            override fun onAuthenticationError(errorCode: Int, errString: CharSequence) {
                if (errorCode != BiometricPrompt.ERROR_NEGATIVE_BUTTON && errorCode != BiometricPrompt.ERROR_USER_CANCELED) onFailure(errString.toString())
                else onFailure(null)
            }
        })
        val info = BiometricPrompt.PromptInfo.Builder()
            .setTitle(title)
            .setSubtitle(subtitle)
            .setNegativeButtonText(negative)
            .setAllowedAuthenticators(AUTH)
            .build()
        prompt.authenticate(info)
    }

    private fun Context.findActivity(): FragmentActivity? {
        var c: Context? = this
        while (c != null) {
            if (c is FragmentActivity) return c
            c = (c as? ContextWrapper)?.baseContext
        }
        return null
    }
}
