package com.superessence.sleepcut

import android.content.ComponentName
import android.content.Context
import android.os.Build
import android.service.quicksettings.Tile
import android.service.quicksettings.TileService

/** Tuile des réglages rapides : un appui lance le dernier minuteur, un autre l'annule. */
class SleepTile : TileService() {
    override fun onStartListening() { refresh() }

    override fun onClick() {
        TimerController.load(this)
        if (TimerController.isRunning) TimerController.cancel(this)
        else TimerController.start(this, TimerController.settings.value.lastMinutes)
        refresh()
    }

    private fun refresh() {
        TimerController.load(this)
        val t = qsTile ?: return
        val st = TimerController.state.value
        val running = st.endAt > System.currentTimeMillis()
        t.state = if (running) Tile.STATE_ACTIVE else Tile.STATE_INACTIVE
        t.label = "SleepCut"
        if (Build.VERSION.SDK_INT >= 29) {
            t.subtitle = if (running) "${((st.endAt - System.currentTimeMillis()) / 60_000L) + 1} min restantes" else "${TimerController.settings.value.lastMinutes} min"
        }
        t.updateTile()
    }

    companion object {
        fun requestUpdate(ctx: Context) {
            runCatching { requestListeningState(ctx, ComponentName(ctx, SleepTile::class.java)) }
        }
    }
}
