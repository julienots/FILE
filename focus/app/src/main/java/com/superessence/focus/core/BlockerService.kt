package com.superessence.focus.core

import android.accessibilityservice.AccessibilityService
import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.provider.Settings
import android.view.accessibility.AccessibilityEvent
import com.superessence.focus.BlockedActivity

/**
 * Service d'accessibilité : à chaque changement de fenêtre, si une session bloquante est en cours
 * et que l'application ouverte n'est pas autorisée, on affiche l'écran « Concentration en cours ».
 * Les Réglages sont eux aussi bloqués : la session ne peut être arrêtée que depuis Focus.
 */
class BlockerService : AccessibilityService() {
    private var lastPkg = ""
    private var lastAt = 0L

    override fun onServiceConnected() { Store.init(this); running = true }
    override fun onDestroy() { running = false; super.onDestroy() }
    override fun onInterrupt() {}

    override fun onAccessibilityEvent(e: AccessibilityEvent?) {
        if (e?.eventType != AccessibilityEvent.TYPE_WINDOW_STATE_CHANGED) return
        val pkg = e.packageName?.toString() ?: return
        Session.check(this)
        if (!Session.blocking) return
        if (Apps.allowed(this, pkg)) return
        val now = System.currentTimeMillis()
        if (pkg == lastPkg && now - lastAt < 700) return
        lastPkg = pkg; lastAt = now
        performGlobalAction(GLOBAL_ACTION_HOME)
        startActivity(
            Intent(this, BlockedActivity::class.java)
                .putExtra("pkg", pkg)
                .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TOP),
        )
    }

    companion object {
        @Volatile var running = false

        fun isEnabled(c: Context): Boolean {
            val me = ComponentName(c, BlockerService::class.java).flattenToString()
            val list = Settings.Secure.getString(c.contentResolver, Settings.Secure.ENABLED_ACCESSIBILITY_SERVICES) ?: ""
            return list.split(':').any { it.equals(me, ignoreCase = true) }
        }
    }
}
