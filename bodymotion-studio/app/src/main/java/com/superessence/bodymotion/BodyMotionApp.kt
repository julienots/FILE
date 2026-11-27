package com.superessence.bodymotion

import android.app.Application
import com.google.android.filament.utils.Utils
import com.superessence.bodymotion.data.Store

class BodyMotionApp : Application() {
    override fun onCreate() {
        super.onCreate()
        com.superessence.bodymotion.data.CrashGuard.install(this)
        // Charge Filament, gltfio et filament-utils (bibliothèques natives).
        renderAvailable = runCatching { Utils.init() }.isSuccess
        store = Store(this)
        // Plantage pendant l'analyse GPU lors de la session précédente : retour automatique au CPU.
        if (com.superessence.bodymotion.data.CrashGuard.lastCrashWasGpu(com.superessence.bodymotion.data.CrashGuard.pendingReport()))
            store.updateSettings { it.copy(gpu = false) }
    }

    companion object {
        lateinit var store: Store
            private set
        /** Faux uniquement hors appareil réel (tests JVM) : le viewport affiche alors un rendu 2D de secours. */
        var renderAvailable = false
            private set
    }
}
