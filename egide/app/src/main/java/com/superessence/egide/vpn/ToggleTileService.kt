package com.superessence.egide.vpn

import android.content.Intent
import android.net.VpnService
import android.service.quicksettings.Tile
import android.service.quicksettings.TileService
import com.superessence.egide.ui.MainActivity

/** Tuile des réglages rapides : activer / désactiver Égide d'un geste. */
class ToggleTileService : TileService() {

    override fun onStartListening() = refresh()

    override fun onClick() {
        if (EgideVpnService.state.value != EgideVpnService.State.OFF) {
            EgideVpnService.stop(this)
        } else if (VpnService.prepare(this) == null) {
            EgideVpnService.start(this)
        } else {
            val i = Intent(this, MainActivity::class.java).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
            if (android.os.Build.VERSION.SDK_INT >= 34) {
                startActivityAndCollapse(
                    android.app.PendingIntent.getActivity(this, 0, i, android.app.PendingIntent.FLAG_IMMUTABLE)
                )
            } else {
                @Suppress("DEPRECATION") startActivityAndCollapse(i)
            }
            return
        }
        qsTile?.let {
            it.state = if (EgideVpnService.state.value == EgideVpnService.State.OFF) Tile.STATE_ACTIVE else Tile.STATE_INACTIVE
            it.updateTile()
        }
    }

    private fun refresh() {
        val t = qsTile ?: return
        val on = EgideVpnService.state.value != EgideVpnService.State.OFF
        t.state = if (on) Tile.STATE_ACTIVE else Tile.STATE_INACTIVE
        t.label = "Égide"
        if (android.os.Build.VERSION.SDK_INT >= 29) t.subtitle = if (on) "Protégé" else "Désactivé"
        t.updateTile()
    }
}
