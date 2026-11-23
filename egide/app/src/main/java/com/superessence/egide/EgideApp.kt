package com.superessence.egide

import android.app.Application
import com.superessence.egide.core.Filter
import com.superessence.egide.core.Prefs
import kotlin.concurrent.thread

class EgideApp : Application() {
    override fun onCreate() {
        super.onCreate()
        Prefs.init(this)
        Filter.init(this)
        thread(name = "egide-load") { Filter.ensureLoaded() }
    }
}
