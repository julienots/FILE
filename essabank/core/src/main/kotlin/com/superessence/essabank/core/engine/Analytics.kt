package com.superessence.essabank.core.engine

import com.superessence.essabank.core.market.MarketSimulator
import com.superessence.essabank.core.model.AccountType
import com.superessence.essabank.core.model.BankState
import com.superessence.essabank.core.model.Category
import com.superessence.essabank.core.model.Direction
import com.superessence.essabank.core.model.GoalMoveType
import com.superessence.essabank.core.model.TradeSide
import com.superessence.essabank.core.model.Transaction
import com.superessence.essabank.core.model.TxStatus
import com.superessence.essabank.core.money.Money
import com.superessence.essabank.core.money.sumOfMoney
import java.math.BigDecimal
import java.math.RoundingMode
import java.time.DayOfWeek
import java.time.LocalDate
import java.time.ZoneId
import java.time.temporal.TemporalAdjusters

data class DayAmount(val date: LocalDate, val amount: Money)
data class PeriodFlow(val start: LocalDate, val end: LocalDate, val income: Money, val spending: Money) {
    val net: Money get() = income - spending
}
data class CategoryShare(val category: Category, val amount: Money, val share: Float, val count: Int)
data class BudgetStatus(val category: Category?, val budget: Money, val spent: Money) {
    val ratio: Float get() = if (budget.cents <= 0) 0f else (spent.cents.toFloat() / budget.cents)
    val remaining: Money get() = budget - spent
    val exceeded: Boolean get() = spent > budget
}
data class Overview(
    val totalBalance: Money,
    val primaryBalance: Money,
    val primaryAvailable: Money,
    val savingsTotal: Money,
    val portfolioValue: Money,
    val netWorth: Money,
    val monthIncome: Money,
    val monthSpending: Money,
    val previousMonthSpending: Money,
)

/** Calculs d'analyse financière (lecture seule) sur l'état virtuel. */
class Analytics(private val clock: BankClock, private val market: MarketSimulator) {
    private val zone: ZoneId get() = clock.zone

    private fun completed(s: BankState) = s.transactions.asSequence().filter { it.status == TxStatus.COMPLETED }
    private fun date(t: Transaction) = Dates.localDate(t.timestamp, zone)
    private fun isSpend(t: Transaction) = t.kind.isSpending && t.direction == Direction.DEBIT
    private fun isIncome(t: Transaction) = t.kind.isIncome && t.direction == Direction.CREDIT

    fun overview(s: BankState): Overview {
        val included = s.accounts.filter { it.includeInTotal }
        val total = included.sumOfMoney { it.balance }
        val primary = s.primaryAccount
        val savings = s.goals.sumOfMoney { it.saved }
        val portfolio = s.holdings.sumOfMoney { MarketSimulator.value(market.price(it.symbol, clock.now()), it.quantityMicro) }
        val today = clock.today()
        val m = flow(s, today.withDayOfMonth(1), today)
        val prevStart = today.minusMonths(1).withDayOfMonth(1)
        val prev = flow(s, prevStart, prevStart.with(TemporalAdjusters.lastDayOfMonth()))
        return Overview(
            totalBalance = total, primaryBalance = primary?.balance ?: Money.ZERO,
            primaryAvailable = primary?.available ?: Money.ZERO, savingsTotal = savings, portfolioValue = portfolio,
            netWorth = total + savings + portfolio, monthIncome = m.income, monthSpending = m.spending,
            previousMonthSpending = prev.spending,
        )
    }

    /** Revenus et dépenses entre deux dates incluses. */
    fun flow(s: BankState, start: LocalDate, end: LocalDate, accountId: String? = null): PeriodFlow {
        var income = Money.ZERO; var spending = Money.ZERO
        for (t in completed(s)) {
            if (accountId != null && t.accountId != accountId) continue
            val d = date(t)
            if (d.isBefore(start) || d.isAfter(end)) continue
            if (isIncome(t)) income += t.amount
            if (isSpend(t)) spending += t.amount
        }
        return PeriodFlow(start, end, income, spending)
    }

    /** Dépenses quotidiennes sur les [days] derniers jours (aujourd'hui inclus). */
    fun dailySpending(s: BankState, days: Int, accountId: String? = null): List<DayAmount> {
        val today = clock.today()
        val start = today.minusDays(days.toLong() - 1)
        val map = HashMap<LocalDate, Long>()
        for (t in completed(s)) {
            if (!isSpend(t) || (accountId != null && t.accountId != accountId)) continue
            val d = date(t)
            if (d.isBefore(start) || d.isAfter(today)) continue
            map[d] = (map[d] ?: 0L) + t.amount.cents
        }
        return (0 until days).map { i -> start.plusDays(i.toLong()).let { DayAmount(it, Money(map[it] ?: 0L)) } }
    }

    /** Revenus/dépenses par semaine (lundi → dimanche) sur [weeks] semaines. */
    fun weeklyFlows(s: BankState, weeks: Int): List<PeriodFlow> {
        val monday = clock.today().with(TemporalAdjusters.previousOrSame(DayOfWeek.MONDAY))
        return (weeks - 1 downTo 0).map { i ->
            val start = monday.minusWeeks(i.toLong())
            flow(s, start, start.plusDays(6))
        }
    }

    /** Revenus/dépenses par mois civil sur [months] mois. */
    fun monthlyFlows(s: BankState, months: Int, accountId: String? = null): List<PeriodFlow> {
        val first = clock.today().withDayOfMonth(1)
        return (months - 1 downTo 0).map { i ->
            val start = first.minusMonths(i.toLong())
            flow(s, start, start.with(TemporalAdjusters.lastDayOfMonth()), accountId)
        }
    }

    /** Répartition des dépenses par catégorie sur une période. */
    fun categoryBreakdown(s: BankState, start: LocalDate, end: LocalDate): List<CategoryShare> {
        val sums = HashMap<Category, Long>(); val counts = HashMap<Category, Int>()
        for (t in completed(s)) {
            if (!isSpend(t)) continue
            val d = date(t)
            if (d.isBefore(start) || d.isAfter(end)) continue
            sums[t.category] = (sums[t.category] ?: 0L) + t.amount.cents
            counts[t.category] = (counts[t.category] ?: 0) + 1
        }
        val total = sums.values.sum().coerceAtLeast(1L)
        return sums.entries.sortedByDescending { it.value }
            .map { CategoryShare(it.key, Money(it.value), it.value.toFloat() / total, counts[it.key] ?: 0) }
    }

    fun budgetStatuses(s: BankState): List<BudgetStatus> {
        val today = clock.today()
        val month = categoryBreakdown(s, today.withDayOfMonth(1), today).associateBy { it.category }
        val total = month.values.sumOfMoney { it.amount }
        val out = mutableListOf<BudgetStatus>()
        s.budgets.monthly?.let { out += BudgetStatus(null, it, total) }
        for ((c, b) in s.budgets.categories.entries.sortedBy { it.key.ordinal }) out += BudgetStatus(c, b, month[c]?.amount ?: Money.ZERO)
        return out
    }

    /** Taux d'épargne du mois : (revenus − dépenses) / revenus, en pourcentage. */
    fun savingsRate(s: BankState): Int {
        val today = clock.today()
        val f = flow(s, today.withDayOfMonth(1), today)
        if (!f.income.isPositive) return 0
        return ((f.income.cents - f.spending.cents) * 100 / f.income.cents).toInt().coerceIn(-100, 100)
    }

    /** Dépense moyenne par jour sur le mois courant. */
    fun averageDailySpending(s: BankState): Money {
        val today = clock.today()
        val f = flow(s, today.withDayOfMonth(1), today)
        return Money(f.spending.cents / today.dayOfMonth)
    }

    /** Projection des dépenses à fin de mois au rythme actuel. */
    fun projectedMonthSpending(s: BankState): Money {
        val today = clock.today()
        return Money(averageDailySpending(s).cents * today.lengthOfMonth())
    }

    /**
     * Évolution du patrimoine virtuel (comptes inclus + coffres + portefeuille fictif),
     * reconstituée jour par jour à partir de l'historique.
     */
    fun wealthSeries(s: BankState, days: Int): List<DayAmount> {
        val today = clock.today()
        val includedIds = s.accounts.filter { it.includeInTotal }.map { it.id }.toSet()
        val txs = completed(s).filter { it.accountId in includedIds }.toList()
        var accounts = s.accounts.filter { it.includeInTotal }.sumOfMoney { it.balance }.cents
        var goals = s.goals.sumOfMoney { it.saved }.cents
        val result = ArrayList<DayAmount>(days)
        // Agrégation par jour des mouvements, pour remonter le temps.
        val txByDay = txs.groupBy { date(it) }.mapValues { (_, l) -> l.sumOf { it.signed.cents } }
        val goalByDay = s.goalMovements.groupBy { Dates.localDate(it.timestamp, zone) }
            .mapValues { (_, l) -> l.sumOf { if (it.type == GoalMoveType.DEPOSIT) it.amount.cents else -it.amount.cents } }
        val trades = s.trades.sortedBy { it.timestamp }
        for (i in 0 until days) {
            val d = today.minusDays(i.toLong())
            val endOfDay = if (i == 0) clock.now() else Dates.startOfDay(d.plusDays(1), zone) - 1
            val qty = HashMap<String, Long>()
            for (t in trades) {
                if (t.timestamp > endOfDay) break
                qty[t.symbol] = (qty[t.symbol] ?: 0L) + if (t.side == TradeSide.BUY) t.quantityMicro else -t.quantityMicro
            }
            val portfolio = qty.entries.sumOf { (sym, q) -> if (q <= 0) 0L else MarketSimulator.value(market.price(sym, endOfDay), q).cents }
            result += DayAmount(d, Money(accounts + goals + portfolio))
            accounts -= txByDay[d] ?: 0L
            goals -= goalByDay[d] ?: 0L
        }
        return result.reversed()
    }

    /** Variation en % entre deux montants. */
    fun change(from: Money, to: Money): BigDecimal {
        if (from.isZero) return BigDecimal.ZERO
        return (to.toBigDecimal() - from.toBigDecimal()).multiply(BigDecimal(100))
            .divide(from.toBigDecimal().abs(), 2, RoundingMode.HALF_EVEN)
    }

    /** Principales dépenses du mois (pour la vue analytique). */
    fun topExpenses(s: BankState, limit: Int): List<Transaction> {
        val today = clock.today(); val start = today.withDayOfMonth(1)
        return completed(s).filter { isSpend(it) && !date(it).isBefore(start) }.sortedByDescending { it.amount.cents }.take(limit).toList()
    }

    /** Solde d'un compte reconstitué sur les [days] derniers jours. */
    fun accountBalanceSeries(s: BankState, accountId: String, days: Int): List<DayAmount> {
        val acc = s.account(accountId) ?: return emptyList()
        val byDay = completed(s).filter { it.accountId == accountId }.groupBy { date(it) }.mapValues { (_, l) -> l.sumOf { it.signed.cents } }
        var bal = acc.balance.cents
        val today = clock.today()
        val out = ArrayList<DayAmount>()
        for (i in 0 until days) {
            val d = today.minusDays(i.toLong())
            out += DayAmount(d, Money(bal))
            bal -= byDay[d] ?: 0L
        }
        return out.reversed()
    }

    fun accountsByType(s: BankState): Map<AccountType, Money> =
        s.accounts.groupBy { it.type }.mapValues { (_, l) -> l.sumOfMoney { it.balance } }
}
