package com.superessence.essabank

import android.os.Bundle
import android.view.WindowManager
import androidx.activity.SystemBarStyle
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.viewModels
import androidx.fragment.app.FragmentActivity
import com.superessence.essabank.ui.AppViewModel
import com.superessence.essabank.ui.EssaRoot

/**
 * Activité unique. Hérite de [FragmentActivity] pour l'API BiometricPrompt.
 * Gère le verrouillage automatique en arrière-plan et la protection d'écran (FLAG_SECURE).
 */
class MainActivity : FragmentActivity() {
    private val vm: AppViewModel by viewModels()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge(
            statusBarStyle = SystemBarStyle.dark(android.graphics.Color.TRANSPARENT),
            navigationBarStyle = SystemBarStyle.dark(android.graphics.Color.TRANSPARENT),
        )
        applySecureFlag(vm.security.secureScreen)
        setContent { EssaRoot(vm, onSecureChanged = ::applySecureFlag) }
    }

    fun applySecureFlag(secure: Boolean) {
        if (secure) window.setFlags(WindowManager.LayoutParams.FLAG_SECURE, WindowManager.LayoutParams.FLAG_SECURE)
        else window.clearFlags(WindowManager.LayoutParams.FLAG_SECURE)
    }

    override fun onStop() {
        super.onStop()
        if (!isChangingConfigurations) vm.onBackground()
    }

    override fun onStart() {
        super.onStart()
        vm.onForeground()
    }
}
