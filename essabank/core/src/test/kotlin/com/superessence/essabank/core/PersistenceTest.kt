package com.superessence.essabank.core

import com.superessence.essabank.core.data.BankDatabase
import com.superessence.essabank.core.data.BankRepository
import com.superessence.essabank.core.engine.EngineResult
import com.superessence.essabank.core.model.AccountType
import com.superessence.essabank.core.money.Money
import com.superessence.essabank.core.security.BackupCodec
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.async
import kotlinx.coroutines.awaitAll
import kotlinx.coroutines.runBlocking
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertFalse
import kotlin.test.assertNotNull
import kotlin.test.assertNull
import kotlin.test.assertTrue

class PersistenceTest {
    @get:Rule val tmp = TemporaryFolder()

    private val clock = T.clock()
    private val engine = T.engine(clock)

    private fun open(name: String = "essabank.db") = BankDatabase(JdbcDriver(tmp.root.resolve(name).absolutePath))

    @Test fun `aller-retour complet en base`() {
        val demo = T.demo(engine)
        val db = open()
        assertFalse(db.hasData())
        assertNull(db.load())
        db.replaceAll(demo)
        val loaded = db.load()
        assertEquals(demo.canonical(), loaded)
        assertEquals(demo.transactions.size.toLong(), db.transactionCount())
        // Le solde recalculé en SQL correspond au solde stocké.
        for (a in demo.accounts) assertEquals(a.balance.cents, db.sqlBalance(a.id))
        db.close()
    }

    @Test fun `les donnees survivent a la fermeture de l application`() {
        val demo = T.demo(engine)
        val current = demo.accounts.first { it.type == AccountType.CURRENT }
        val savings = demo.accounts.first { it.type == AccountType.SAVINGS }
        run {
            val repo = BankRepository(open(), engine, Dispatchers.Unconfined)
            runBlocking {
                repo.replaceAll(demo)
                val r = repo.execute { transferInternal(it, current.id, savings.id, Money.of("100.01"), "Avant fermeture") }
                assertTrue(r.isOk)
            }
        }
        // « Redémarrage » : nouvelle connexion, nouveau dépôt.
        val repo2 = BankRepository(open(), engine, Dispatchers.Unconfined)
        val s = runBlocking { repo2.load() }
        assertNotNull(s)
        assertEquals(current.balance - Money.of("100.01"), s.account(current.id)!!.balance)
        assertEquals(savings.balance + Money.of("100.01"), s.account(savings.id)!!.balance)
        assertTrue(s.transactions.any { it.label == "Avant fermeture" })
        assertConsistent(s)
    }

    @Test fun `une panne d ecriture n enregistre rien et ne change pas l etat`() {
        val demo = T.demo(engine)
        val driver = JdbcDriver(tmp.root.resolve("panne.db").absolutePath)
        val db = BankDatabase(driver)
        val repo = BankRepository(db, engine, Dispatchers.Unconfined)
        runBlocking { repo.replaceAll(demo) }
        val current = demo.accounts.first { it.type == AccountType.CURRENT }
        val savings = demo.accounts.first { it.type == AccountType.SAVINGS }
        // Le crédit (2e écriture) échoue : la transaction SQL doit être annulée en entier.
        driver.failOnSqlContaining = "INSERT OR REPLACE INTO transactions"
        assertFailsWith<IllegalStateException> {
            runBlocking { repo.execute { transferInternal(it, current.id, savings.id, Money.units(50)) } }
        }
        driver.failOnSqlContaining = null
        assertEquals(demo, repo.current, "l'état publié ne doit pas changer")
        assertEquals(demo.canonical(), db.load(), "la base ne doit contenir aucune écriture partielle")
    }

    @Test fun `operations concurrentes serialisees`() {
        val demo = T.demo(engine)
        val repo = BankRepository(open("concurrent.db"), engine, Dispatchers.IO)
        runBlocking { repo.replaceAll(demo) }
        val current = demo.accounts.first { it.type == AccountType.CURRENT }
        val savings = demo.accounts.first { it.type == AccountType.SAVINGS }
        val results = runBlocking(Dispatchers.Default) {
            (1..60).map { i ->
                async {
                    if (i % 2 == 0) repo.execute { transferInternal(it, current.id, savings.id, Money.units(10), requestId = "c$i") }
                    else repo.execute { transferInternal(it, savings.id, current.id, Money.units(5), requestId = "c$i") }
                }
            }.awaitAll()
        }
        assertTrue(results.all { it is EngineResult.Ok })
        val s = repo.current!!
        assertEquals(current.balance - Money.units(300) + Money.units(150), s.account(current.id)!!.balance)
        assertConsistent(s)
        assertEquals(s.canonical(), open("concurrent.db").load())
    }

    @Test fun `sauvegarde chiffree et restauration`() {
        val demo = T.demo(engine)
        val repo = BankRepository(open("a.db"), engine, Dispatchers.Unconfined)
        runBlocking { repo.replaceAll(demo) }
        val backup = runBlocking { repo.exportBackup("motdepasse-solide") }
        assertFalse(String(backup, Charsets.ISO_8859_1).contains("Camille"), "la sauvegarde ne doit pas être lisible en clair")
        val other = BankRepository(open("b.db"), engine, Dispatchers.Unconfined)
        assertFailsWith<BackupCodec.BadPasswordException> { runBlocking { other.importBackup(backup, "mauvais-mdp") } }
        assertFailsWith<BackupCodec.BadFormatException> { runBlocking { other.importBackup("pas une sauvegarde".toByteArray(), "x") } }
        val restored = runBlocking { other.importBackup(backup, "motdepasse-solide") }
        assertEquals(demo, restored)
        assertEquals(demo.canonical(), open("b.db").load())
    }

    @Test fun `effacement complet`() {
        val db = open()
        db.replaceAll(T.demo(engine))
        db.wipe()
        assertNull(db.load())
        assertEquals(0L, db.transactionCount())
    }
}
