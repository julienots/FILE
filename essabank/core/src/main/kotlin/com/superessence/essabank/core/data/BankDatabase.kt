package com.superessence.essabank.core.data

import com.superessence.essabank.core.model.Account
import com.superessence.essabank.core.model.AppNotification
import com.superessence.essabank.core.model.BankState
import com.superessence.essabank.core.model.Beneficiary
import com.superessence.essabank.core.model.Budgets
import com.superessence.essabank.core.model.Card
import com.superessence.essabank.core.model.GoalMovement
import com.superessence.essabank.core.model.Holding
import com.superessence.essabank.core.model.LoanSimulation
import com.superessence.essabank.core.model.Meta
import com.superessence.essabank.core.model.Profile
import com.superessence.essabank.core.model.SavingsGoal
import com.superessence.essabank.core.model.ScheduledTransfer
import com.superessence.essabank.core.model.Trade
import com.superessence.essabank.core.model.Transaction
import kotlinx.serialization.KSerializer
import kotlinx.serialization.json.Json

/** Abstraction minimale d'un moteur SQLite (Android SQLiteDatabase ou JDBC pour les tests). */
interface SqlDriver {
    fun execute(sql: String, args: List<Any?> = emptyList())
    fun <T> query(sql: String, args: List<Any?> = emptyList(), map: (SqlRow) -> T): List<T>
    fun <T> transaction(block: () -> T): T
    fun close()
}

interface SqlRow {
    fun string(index: Int): String?
    fun long(index: Int): Long?
}

val BankJson = Json {
    ignoreUnknownKeys = true
    encodeDefaults = true
    explicitNulls = false
}

/**
 * Base locale persistante d'ESSABANK.
 *
 * Chaque entité est stockée dans sa table, avec des colonnes indexées pour les requêtes
 * fréquentes et une colonne `data` (JSON) pour le détail. [save] n'écrit que les lignes
 * modifiées, dans UNE transaction SQLite : en cas d'erreur, rien n'est enregistré.
 */
class BankDatabase(private val driver: SqlDriver) {

    private class Table<T>(
        val name: String,
        val serializer: KSerializer<T>,
        val id: (T) -> String,
        /** Colonnes supplémentaires indexables : nom → valeur. */
        val extra: List<Pair<String, (T) -> Any?>> = emptyList(),
        val extraDdl: String = "",
        val select: (BankState) -> List<T>,
    )

    private val tables: List<Table<*>> = listOf(
        Table("accounts", Account.serializer(), { it.id }, listOf("sort_order" to { a: Account -> a.sortOrder.toLong() }), ", sort_order INTEGER NOT NULL", select = { it.accounts }),
        Table("cards", Card.serializer(), { it.id }, listOf("account_id" to { c: Card -> c.accountId }), ", account_id TEXT NOT NULL", select = { it.cards }),
        Table(
            "transactions", Transaction.serializer(), { it.id },
            listOf(
                "account_id" to { t: Transaction -> t.accountId },
                "ts" to { t: Transaction -> t.timestamp },
                "category" to { t: Transaction -> t.category.name },
                "kind" to { t: Transaction -> t.kind.name },
                "amount" to { t: Transaction -> t.signed.cents },
                "seq" to { t: Transaction -> t.seq },
            ),
            ", account_id TEXT NOT NULL, ts INTEGER NOT NULL, category TEXT NOT NULL, kind TEXT NOT NULL, amount INTEGER NOT NULL, seq INTEGER NOT NULL DEFAULT 0",
            select = { it.transactions },
        ),
        Table("beneficiaries", Beneficiary.serializer(), { it.id }, select = { it.beneficiaries }),
        Table("scheduled", ScheduledTransfer.serializer(), { it.id }, select = { it.scheduled }),
        Table("goals", SavingsGoal.serializer(), { it.id }, select = { it.goals }),
        Table("goal_movements", GoalMovement.serializer(), { it.id }, listOf("goal_id" to { m: GoalMovement -> m.goalId }, "ts" to { m: GoalMovement -> m.timestamp }), ", goal_id TEXT NOT NULL, ts INTEGER NOT NULL", select = { it.goalMovements }),
        Table("holdings", Holding.serializer(), { it.symbol }, select = { it.holdings }),
        Table("trades", Trade.serializer(), { it.id }, listOf("ts" to { t: Trade -> t.timestamp }), ", ts INTEGER NOT NULL", select = { it.trades }),
        Table("loans", LoanSimulation.serializer(), { it.id }, select = { it.loans }),
        Table("notifications", AppNotification.serializer(), { it.id }, listOf("ts" to { n: AppNotification -> n.timestamp }), ", ts INTEGER NOT NULL", select = { it.notifications }),
    )

    init { migrate() }

    private fun migrate() {
        driver.transaction {
            driver.execute("CREATE TABLE IF NOT EXISTS kv (key TEXT PRIMARY KEY NOT NULL, value TEXT NOT NULL)")
            for (t in tables) {
                driver.execute("CREATE TABLE IF NOT EXISTS ${t.name} (id TEXT PRIMARY KEY NOT NULL${t.extraDdl}, data TEXT NOT NULL)")
            }
            driver.execute("CREATE INDEX IF NOT EXISTS idx_tx_account_ts ON transactions(account_id, ts)")
            driver.execute("CREATE INDEX IF NOT EXISTS idx_tx_ts ON transactions(ts)")
            driver.execute("CREATE INDEX IF NOT EXISTS idx_tx_category ON transactions(category)")
            driver.execute("CREATE INDEX IF NOT EXISTS idx_cards_account ON cards(account_id)")
            driver.execute("CREATE INDEX IF NOT EXISTS idx_gmv_goal ON goal_movements(goal_id)")
            val version = kv("schema_version")?.toIntOrNull() ?: 0
            if (version < SCHEMA_VERSION) putKv("schema_version", SCHEMA_VERSION.toString())
        }
    }

    fun hasData(): Boolean = kv("profile") != null

    /** Charge l'état complet, ou null si aucun profil n'a encore été créé. */
    fun load(): BankState? {
        val profileJson = kv("profile") ?: return null
        val profile = BankJson.decodeFromString(Profile.serializer(), profileJson)
        val budgets = kv("budgets")?.let { BankJson.decodeFromString(Budgets.serializer(), it) } ?: Budgets()
        val meta = kv("meta")?.let { BankJson.decodeFromString(Meta.serializer(), it) } ?: Meta()
        return BankState(
            profile = profile,
            accounts = read("accounts", Account.serializer(), "sort_order").sortedBy { it.sortOrder },
            cards = read("cards", Card.serializer()).sortedBy { it.createdAt },
            transactions = read("transactions", Transaction.serializer(), "ts, seq"),
            beneficiaries = read("beneficiaries", Beneficiary.serializer()).sortedBy { it.createdAt },
            scheduled = read("scheduled", ScheduledTransfer.serializer()).sortedBy { it.createdAt },
            budgets = budgets,
            goals = read("goals", SavingsGoal.serializer()).sortedBy { it.sortOrder },
            goalMovements = read("goal_movements", GoalMovement.serializer(), "ts"),
            holdings = read("holdings", Holding.serializer()).sortedBy { it.symbol },
            trades = read("trades", Trade.serializer(), "ts"),
            loans = read("loans", LoanSimulation.serializer()).sortedByDescending { it.createdAt },
            notifications = read("notifications", AppNotification.serializer(), "ts").sortedByDescending { it.timestamp },
            meta = meta,
        ).canonical()
    }

    /**
     * Enregistre le passage de [old] à [new] : seules les lignes ajoutées, modifiées ou supprimées
     * sont écrites, dans une transaction unique (tout ou rien).
     */
    fun save(old: BankState?, new: BankState) {
        driver.transaction {
            if (old?.profile != new.profile) putKv("profile", BankJson.encodeToString(Profile.serializer(), new.profile))
            if (old?.budgets != new.budgets) putKv("budgets", BankJson.encodeToString(Budgets.serializer(), new.budgets))
            if (old?.meta != new.meta) putKv("meta", BankJson.encodeToString(Meta.serializer(), new.meta))
            for (t in tables) diff(t, old, new)
        }
    }

    /** Remplace intégralement le contenu (réinitialisation, restauration). */
    fun replaceAll(state: BankState) {
        driver.transaction {
            clearTables()
            save(null, state)
        }
    }

    /** Efface toutes les données locales. */
    fun wipe() {
        driver.transaction { clearTables() }
    }

    fun transactionCount(): Long = driver.query("SELECT COUNT(*) FROM transactions") { it.long(0) ?: 0L }.first()

    /** Somme des écritures d'un compte calculée directement en SQL (contrôle de cohérence). */
    fun sqlBalance(accountId: String): Long =
        driver.query("SELECT COALESCE(SUM(amount), 0) FROM transactions WHERE account_id = ?", listOf(accountId)) { it.long(0) ?: 0L }.first()

    fun close() = driver.close()

    // ------------------------------------------------------------------ interne

    private fun clearTables() {
        driver.execute("DELETE FROM kv WHERE key <> 'schema_version'")
        for (t in tables) driver.execute("DELETE FROM ${t.name}")
    }

    private fun <T> diff(t: Table<T>, old: BankState?, new: BankState) {
        val newList = t.select(new)
        val oldList = old?.let { t.select(it) } ?: emptyList()
        if (old != null && oldList === newList) return
        val oldById = HashMap<String, T>(oldList.size * 2)
        for (e in oldList) oldById[t.id(e)] = e
        val newIds = HashSet<String>(newList.size * 2)
        val cols = listOf("id") + t.extra.map { it.first } + "data"
        val sql = "INSERT OR REPLACE INTO ${t.name} (${cols.joinToString()}) VALUES (${cols.joinToString { "?" }})"
        for (e in newList) {
            val id = t.id(e)
            newIds += id
            if (oldById[id] == e) continue
            val args = ArrayList<Any?>(cols.size)
            args += id
            for ((_, f) in t.extra) args += f(e)
            args += BankJson.encodeToString(t.serializer, e)
            driver.execute(sql, args)
        }
        for (id in oldById.keys) if (id !in newIds) driver.execute("DELETE FROM ${t.name} WHERE id = ?", listOf(id))
    }

    private fun <T> read(table: String, serializer: KSerializer<T>, orderBy: String? = null): List<T> =
        driver.query("SELECT data FROM $table" + (orderBy?.let { " ORDER BY $it" } ?: "")) { row ->
            BankJson.decodeFromString(serializer, row.string(0)!!)
        }

    private fun kv(key: String): String? =
        driver.query("SELECT value FROM kv WHERE key = ?", listOf(key)) { it.string(0) }.firstOrNull()

    private fun putKv(key: String, value: String) =
        driver.execute("INSERT OR REPLACE INTO kv (key, value) VALUES (?, ?)", listOf(key, value))

    companion object {
        const val SCHEMA_VERSION = 1
    }
}
