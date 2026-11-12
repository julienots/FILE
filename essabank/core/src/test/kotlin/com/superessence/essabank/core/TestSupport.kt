package com.superessence.essabank.core

import com.superessence.essabank.core.data.SqlDriver
import com.superessence.essabank.core.data.SqlRow
import com.superessence.essabank.core.demo.DemoData
import com.superessence.essabank.core.engine.BankEngine
import com.superessence.essabank.core.engine.EngineResult
import com.superessence.essabank.core.engine.FixedClock
import com.superessence.essabank.core.engine.SeededIds
import com.superessence.essabank.core.market.MarketSimulator
import com.superessence.essabank.core.model.BankState
import com.superessence.essabank.core.model.Direction
import java.sql.Connection
import java.sql.DriverManager
import kotlin.test.assertEquals
import kotlin.test.fail

/** Pilote SQLite réel (sqlite-jdbc) pour tester la persistance sur JVM. */
class JdbcDriver(path: String) : SqlDriver {
    private val conn: Connection = DriverManager.getConnection("jdbc:sqlite:$path").apply { autoCommit = true }
    private var depth = 0
    /** Permet de simuler une panne d'écriture. */
    var failOnSqlContaining: String? = null

    override fun execute(sql: String, args: List<Any?>) {
        failOnSqlContaining?.let { if (sql.contains(it)) throw IllegalStateException("Panne simulée") }
        conn.prepareStatement(sql).use { st ->
            args.forEachIndexed { i, a -> st.setObject(i + 1, a) }
            st.execute()
        }
    }

    override fun <T> query(sql: String, args: List<Any?>, map: (SqlRow) -> T): List<T> =
        conn.prepareStatement(sql).use { st ->
            args.forEachIndexed { i, a -> st.setObject(i + 1, a) }
            st.executeQuery().use { rs ->
                val out = ArrayList<T>()
                val row = object : SqlRow {
                    override fun string(index: Int): String? = rs.getString(index + 1)
                    override fun long(index: Int): Long? = rs.getLong(index + 1).takeUnless { rs.wasNull() }
                }
                while (rs.next()) out += map(row)
                out
            }
        }

    override fun <T> transaction(block: () -> T): T {
        if (depth == 0) conn.autoCommit = false
        depth++
        var ok = false
        try {
            val r = block(); ok = true; return r
        } finally {
            depth--
            if (depth == 0) {
                if (ok) conn.commit() else conn.rollback()
                conn.autoCommit = true
            } else if (!ok) {
                // l'exception remonte jusqu'à la transaction externe, qui annulera tout
            }
        }
    }

    override fun close() = conn.close()
}

object T {
    fun clock() = FixedClock.at(2026, 11, 12, 14, 30)
    fun engine(clock: FixedClock = clock(), seed: Long = 42) = BankEngine(clock, SeededIds(seed), MarketSimulator(clock.zone))
    fun demo(engine: BankEngine): BankState = DemoData.create(engine.clock, engine.ids, engine.market)
}

fun <V> EngineResult<V>.ok(): EngineResult.Ok<V> = when (this) {
    is EngineResult.Ok -> this
    is EngineResult.Err -> fail("Erreur inattendue : $error")
}

/** Vérifie les invariants comptables de l'état. */
fun assertConsistent(s: BankState) {
    for (a in s.accounts) {
        val txs = s.transactions.filter { it.accountId == a.id }.sortedWith(BankState.TX_ORDER)
        val sum = txs.sumOf { if (it.direction == Direction.CREDIT) it.amount.cents else -it.amount.cents }
        assertEquals(a.balance.cents, sum, "Somme des écritures ≠ solde pour ${a.name}")
        var bal = 0L
        for (t in txs) {
            assert(t.amount.cents > 0) { "Montant non positif" }
            bal += if (t.direction == Direction.CREDIT) t.amount.cents else -t.amount.cents
            assertEquals(bal, t.balanceAfter.cents, "Solde après opération incohérent (${t.label})")
        }
    }
    // Les deux écritures d'un virement interne se compensent.
    val ids = s.accounts.map { it.id }.toSet()
    s.transactions.filter { it.transferId != null }.groupBy { it.transferId }.forEach { (_, legs) ->
        // Si un des deux comptes a été supprimé, seule l'écriture restante subsiste.
        if (legs.size == 1) return@forEach
        assertEquals(2, legs.size, "Un virement interne doit avoir deux écritures")
        assertEquals(0L, legs.sumOf { it.signed.cents }, "Les deux écritures doivent se compenser")
    }
    for (g in s.goals) {
        val moves = s.goalMovements.filter { it.goalId == g.id }
        val sum = moves.sumOf { if (it.type == com.superessence.essabank.core.model.GoalMoveType.DEPOSIT) it.amount.cents else -it.amount.cents }
        assertEquals(g.saved.cents, sum, "Épargne du coffre ${g.name} incohérente")
    }
}
