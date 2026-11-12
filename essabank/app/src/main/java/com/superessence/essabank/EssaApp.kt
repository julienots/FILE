package com.superessence.essabank

import android.app.Application
import android.content.Context
import com.superessence.essabank.core.data.BankDatabase
import com.superessence.essabank.core.data.BankRepository
import com.superessence.essabank.core.engine.Analytics
import com.superessence.essabank.core.engine.BankClock
import com.superessence.essabank.core.engine.BankEngine
import com.superessence.essabank.core.engine.IdGenerator
import com.superessence.essabank.core.market.MarketSimulator
import com.superessence.essabank.data.AndroidSqlDriver
import com.superessence.essabank.data.SecurityStore
import com.superessence.essabank.data.SecuritySettings

/** Conteneur de dépendances de l'application (une seule instance par processus). */
class AppContainer(
    val database: BankDatabase,
    val security: SecuritySettings,
    clock: BankClock = BankClock.SYSTEM,
) {
    val market = MarketSimulator(clock.zone)
    val engine = BankEngine(clock, IdGenerator.SECURE, market)
    val analytics = Analytics(clock, market)
    val repository = BankRepository(database, engine)

    companion object {
        fun android(context: Context) = AppContainer(BankDatabase(AndroidSqlDriver(context)), SecurityStore(context))
    }
}

class EssaApp : Application() {
    lateinit var container: AppContainer
        internal set

    override fun onCreate() {
        super.onCreate()
        container = AppContainer.android(this)
    }
}
