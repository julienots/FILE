package com.superessence.essabank

import android.content.Context
import androidx.test.core.app.ApplicationProvider
import androidx.test.ext.junit.runners.AndroidJUnit4
import com.superessence.essabank.core.data.BankDatabase
import com.superessence.essabank.core.data.BankRepository
import com.superessence.essabank.core.demo.DemoData
import com.superessence.essabank.core.engine.BankEngine
import com.superessence.essabank.core.model.AccountType
import com.superessence.essabank.core.money.Money
import com.superessence.essabank.data.AndroidSqlDriver
import com.superessence.essabank.data.PinCheck
import com.superessence.essabank.data.SecurityStore
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.runBlocking
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.annotation.Config

/** Persistance réelle sur SQLite Android (Robolectric) et réglages de sécurité. */
@RunWith(AndroidJUnit4::class)
@Config(sdk = [35], application = android.app.Application::class)
class AndroidPersistenceTest {
    private val context: Context = ApplicationProvider.getApplicationContext()

    @Test
    fun les_donnees_survivent_a_la_fermeture() {
        val engine = BankEngine()
        val demo = DemoData.create(engine.clock, engine.ids, engine.market)
        val current = demo.accounts.first { it.type == AccountType.CURRENT }
        val savings = demo.accounts.first { it.type == AccountType.SAVINGS }

        val db1 = BankDatabase(AndroidSqlDriver(context, "test-persist.db"))
        val repo1 = BankRepository(db1, engine, Dispatchers.Unconfined)
        runBlocking {
            repo1.replaceAll(demo)
            assertTrue(repo1.execute { transferInternal(it, current.id, savings.id, Money.of("250.50"), "Test fermeture") }.isOk)
        }
        db1.close()

        // Réouverture du fichier, comme après un redémarrage de l'application.
        val db2 = BankDatabase(AndroidSqlDriver(context, "test-persist.db"))
        val reloaded = db2.load()
        assertNotNull(reloaded)
        assertEquals(current.balance - Money.of("250.50"), reloaded!!.account(current.id)!!.balance)
        assertEquals(savings.balance + Money.of("250.50"), reloaded.account(savings.id)!!.balance)
        assertTrue(reloaded.transactions.any { it.label == "Test fermeture" })
        for (a in reloaded.accounts) assertEquals(a.balance.cents, db2.sqlBalance(a.id))
        db2.close()
    }

    @Test
    fun pin_hache_et_delai_apres_echecs() {
        val store = SecurityStore(context)
        store.clear()
        assertFalse(store.hasPin)
        store.setPin("2580")
        assertTrue(store.hasPin)
        assertEquals(PinCheck.Ok, store.check("2580"))
        repeat(4) { assertTrue(store.check("1111") is PinCheck.Wrong) }
        assertTrue(store.check("1111") is PinCheck.Locked)
        // Pendant l'attente, même le bon code est refusé.
        assertTrue(store.check("2580") is PinCheck.Locked)
        // Le PIN n'est jamais stocké en clair.
        val raw = context.getSharedPreferences("essabank_security", Context.MODE_PRIVATE).all.values.joinToString()
        assertFalse(raw.contains("2580"))
    }
}
