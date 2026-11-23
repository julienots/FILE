package com.superessence.squad

import android.app.Application
import com.superessence.squad.engine.Squad

class SquadApp : Application() {
    override fun onCreate() {
        super.onCreate()
        Squad.init(this)
    }
}
