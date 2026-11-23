package com.superessence.egide.vpn

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.net.VpnService
import com.superessence.egide.core.Prefs

/** Relance la protection après un redémarrage du téléphone ou une mise à jour de l'appli. */
class BootReceiver : BroadcastReceiver() {
    override fun onReceive(context: Context, intent: Intent) {
        Prefs.init(context)
        if (Prefs.protectionWanted && Prefs.startOnBoot && VpnService.prepare(context) == null) {
            runCatching { EgideVpnService.start(context) }
        }
    }
}
