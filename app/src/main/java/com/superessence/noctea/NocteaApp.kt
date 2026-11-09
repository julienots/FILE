package com.superessence.noctea

import android.app.Application
import com.superessence.noctea.audio.AudioEngine
import com.superessence.noctea.audio.AudioExporter
import com.superessence.noctea.audio.PlayableResolver
import com.superessence.noctea.billing.Entitlements
import com.superessence.noctea.billing.LaunchOfferGateway
import com.superessence.noctea.data.Catalog
import com.superessence.noctea.data.UserStore
import com.superessence.noctea.generator.MeditationGenerator
import com.superessence.noctea.generator.PhraseBank
import com.superessence.noctea.notify.Reminders
import com.superessence.noctea.tts.SystemTtsProvider
import kotlinx.coroutines.MainScope

class NocteaApp : Application() {
    val scope = MainScope()
    lateinit var catalog: Catalog
        private set
    lateinit var store: UserStore
        private set
    lateinit var generator: MeditationGenerator
        private set
    lateinit var resolver: PlayableResolver
        private set
    lateinit var engine: AudioEngine
        private set
    lateinit var entitlements: Entitlements
        private set
    val exporter by lazy { AudioExporter(this) }
    val systemTts by lazy { SystemTtsProvider(this) }

    override fun onCreate() {
        super.onCreate()
        catalog = Catalog.load(this)
        store = UserStore(this)
        generator = MeditationGenerator(PhraseBank { catalog.bank[it].orEmpty() }, catalog.fx)
        resolver = PlayableResolver(catalog, store, generator)
        engine = AudioEngine(this, store, resolver, scope)
        entitlements = Entitlements(LaunchOfferGateway())
        Reminders.createChannels(this)
        runCatching { Reminders.reschedule(this) }
    }
}
