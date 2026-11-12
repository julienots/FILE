package com.superessence.essabank.core

import com.superessence.essabank.core.engine.Analytics
import com.superessence.essabank.core.loan.LoanCalculator
import com.superessence.essabank.core.loan.LoanProfile
import com.superessence.essabank.core.market.ChartRange
import com.superessence.essabank.core.market.MarketSimulator
import com.superessence.essabank.core.model.LoanType
import com.superessence.essabank.core.money.Money
import com.superessence.essabank.core.money.sumOfMoney
import com.superessence.essabank.core.pdf.PdfDocumentBuilder
import com.superessence.essabank.core.pdf.Reports
import com.superessence.essabank.core.security.PinHasher
import com.superessence.essabank.core.security.PinLockout
import org.junit.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertNotEquals
import kotlin.test.assertTrue

class LoanTest {
    @Test fun `mensualite de reference`() {
        // 10 000 à 5 % sur 12 mois : mensualité 856,07 (formule d'annuité classique : 856,0748…)
        val r = LoanCalculator.compute(Money.units(10_000), 500, 12)
        assertEquals(Money.of("856.07"), r.monthlyPayment)
        assertEquals(Money.ZERO, r.schedule.last().remaining)
        assertEquals(12, r.schedule.size)
        assertEquals(Money.units(10_000), r.schedule.sumOfMoney { it.principal })
        assertEquals(r.totalInterest, r.schedule.sumOfMoney { it.interest })
        assertTrue(r.totalInterest in Money.of("272.00")..Money.of("274.00"))
        assertEquals(r.principal + r.totalCost, r.totalRepaid)
    }

    @Test fun `taux nul et assurance`() {
        val r = LoanCalculator.compute(Money.units(1200), 0, 12, insuranceBp = 30)
        assertEquals(Money.units(100), r.monthlyPayment)
        assertEquals(Money.ZERO, r.totalInterest)
        assertEquals(Money.of("0.30"), r.monthlyInsurance) // 1 200 × 0,30 % / 12
        assertEquals(Money.of("3.60"), r.totalInsurance)
    }

    @Test fun `credit immobilier long coherent`() {
        val p = LoanProfile.of(LoanType.HOME)
        val r = LoanCalculator.compute(p.defaultPrincipal, p.defaultRateBp, p.defaultMonths, p.defaultInsuranceBp)
        assertEquals(p.defaultMonths, r.schedule.size)
        assertEquals(Money.ZERO, r.schedule.last().remaining)
        assertEquals(p.defaultPrincipal, r.schedule.sumOfMoney { it.principal })
        // Le capital restant décroît strictement.
        assertTrue(r.schedule.zipWithNext().all { (a, b) -> b.remaining < a.remaining })
    }
}

class MarketTest {
    private val clock = T.clock()
    private val market = MarketSimulator(clock.zone)

    @Test fun `prix deterministes positifs et evolutifs`() {
        for (a in market.assets) {
            assertTrue(a.symbol.endsWith(".SIM"), "symbole clairement fictif")
            val p = market.price(a.symbol, clock.now())
            assertTrue(p.isPositive)
            assertEquals(p, MarketSimulator(clock.zone).price(a.symbol, clock.now()), "déterminisme")
        }
        val p1 = market.price("ESC.SIM", clock.now())
        clock.advanceMinutes(60)
        assertNotEquals(p1, market.price("ESC.SIM", clock.now()))
    }

    @Test fun `historiques par periode`() {
        val d = market.history("EWLD.SIM", clock.now(), ChartRange.DAY)
        assertTrue(d.size > 100)
        assertEquals(31, market.history("EWLD.SIM", clock.now(), ChartRange.MONTH).size)
        assertEquals(366, market.history("EWLD.SIM", clock.now(), ChartRange.YEAR).size)
        assertTrue(market.history("EWLD.SIM", clock.now(), ChartRange.WEEK).zipWithNext().all { (a, b) -> b.time > a.time })
        // Le dernier point correspond au prix courant.
        assertEquals(market.price("EWLD.SIM", clock.now()), d.last().price)
    }

    @Test fun `quantites fractionnaires`() {
        val q = MarketSimulator.quantityFor(Money.units(100), Money.of("33.33"))
        assertEquals(3_000_300L, q)
        assertTrue(MarketSimulator.value(Money.of("33.33"), q) <= Money.units(100))
        assertEquals("3.0003", MarketSimulator.formatQuantity(q))
    }
}

class SecurityTest {
    @Test fun `hachage et verification du pin`() {
        val h = PinHasher.hash("2580", iterations = 1000)
        assertTrue(h.startsWith("pbkdf2$"))
        assertFalse(h.contains("2580"))
        assertTrue(PinHasher.verify("2580", h))
        assertFalse(PinHasher.verify("2581", h))
        assertNotEquals(h, PinHasher.hash("2580", iterations = 1000), "sel aléatoire")
        assertFalse(PinHasher.verify("2580", "corrompu"))
    }

    @Test fun `pins triviaux refuses`() {
        for (p in listOf("0000", "1111", "1234", "4321", "123456", "98765", "12a4", "12")) assertFalse(PinHasher.isValidPin(p), p)
        for (p in listOf("2580", "7391", "040812")) assertTrue(PinHasher.isValidPin(p), p)
    }

    @Test fun `delai croissant apres echecs`() {
        assertEquals(0, PinLockout.delayMillis(4))
        assertEquals(30_000, PinLockout.delayMillis(5))
        assertEquals(60_000, PinLockout.delayMillis(6))
        assertEquals(15 * 60_000L, PinLockout.delayMillis(30))
    }
}

class AnalyticsAndPdfTest {
    private val clock = T.clock()
    private val engine = T.engine(clock)
    private val demo = T.demo(engine)
    private val analytics = Analytics(clock, engine.market)

    @Test fun `vue d ensemble et series`() {
        val o = analytics.overview(demo)
        assertEquals(demo.accounts.sumOfMoney { it.balance }, o.totalBalance)
        assertEquals(Money.of("4850.75"), o.primaryBalance)
        assertEquals(Money.of("5350.75"), o.primaryAvailable) // découvert virtuel de 500
        assertTrue(o.monthSpending.isPositive)
        assertEquals(o.totalBalance + o.savingsTotal + o.portfolioValue, o.netWorth)
        val wealth = analytics.wealthSeries(demo, 90)
        assertEquals(90, wealth.size)
        assertEquals(o.netWorth, wealth.last().amount)
        assertEquals(30, analytics.dailySpending(demo, 30).size)
        assertEquals(6, analytics.monthlyFlows(demo, 6).size)
        assertEquals(8, analytics.weeklyFlows(demo, 8).size)
        val cats = analytics.categoryBreakdown(demo, clock.today().minusDays(60), clock.today())
        assertTrue(cats.zipWithNext().all { (a, b) -> a.amount >= b.amount })
        assertEquals(1f, cats.sumOf { it.share.toDouble() }.toFloat(), 0.001f)
        assertEquals(6, analytics.budgetStatuses(demo).size)
        val acc = demo.accounts.first()
        assertEquals(acc.balance, analytics.accountBalanceSeries(demo, acc.id, 30).last().amount)
    }

    @Test fun `releve pdf marque simulation`() {
        val bytes = Reports.statement(demo, demo.transactions, "novembre 2026", clock.zone, clock.now())
        val text = String(bytes, Charsets.ISO_8859_1)
        assertTrue(text.startsWith("%PDF-1.4"))
        assertTrue(text.trimEnd().endsWith("%%EOF"))
        assertTrue(text.contains("SIMULATION"))
        assertTrue(text.contains("AUCUN TRANSFERT RÉEL"))
        val pages = Regex("/Type /Page ").findAll(text).count()
        assertTrue(pages > 5, "plusieurs pages attendues pour ${demo.transactions.size} transactions")
        // Table xref cohérente : chaque offset pointe sur « n 0 obj ».
        val xref = text.substringAfter("xref\n").substringBefore("trailer").lines().drop(2).filter { it.isNotBlank() }
        xref.forEachIndexed { i, line ->
            val off = line.take(10).toInt()
            assertTrue(text.startsWith("${i + 1} 0 obj", off), "offset xref invalide pour l'objet ${i + 1}")
        }
    }

    @Test fun `recu pdf`() {
        val tx = demo.transactions.last { it.kind == com.superessence.essabank.core.model.TxKind.CARD_PAYMENT }
        val text = String(Reports.receipt(demo, tx, clock.zone), Charsets.ISO_8859_1)
        assertTrue(text.contains("SIMULATION \u0097 AUCUN TRANSFERT RÉEL"))
        assertTrue(text.contains(tx.reference))
        assertEquals("café", String(PdfDocumentBuilder.toWinAnsi("café"), charset("windows-1252")))
    }
}
