package com.superessence.essabank.core

import com.superessence.essabank.core.demo.DemoData
import com.superessence.essabank.core.engine.BankError
import com.superessence.essabank.core.engine.BankEvent
import com.superessence.essabank.core.engine.EngineResult
import com.superessence.essabank.core.engine.FictionalIdentifiers
import com.superessence.essabank.core.model.AccountType
import com.superessence.essabank.core.model.CardDesign
import com.superessence.essabank.core.model.CardStatus
import com.superessence.essabank.core.model.CardType
import com.superessence.essabank.core.model.Category
import com.superessence.essabank.core.model.Frequency
import com.superessence.essabank.core.model.GoalKind
import com.superessence.essabank.core.model.NotificationType
import com.superessence.essabank.core.model.Tier
import com.superessence.essabank.core.model.TxKind
import com.superessence.essabank.core.money.AppLanguage
import com.superessence.essabank.core.money.Money
import org.junit.Test
import kotlin.random.Random
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertIs
import kotlin.test.assertNotNull
import kotlin.test.assertTrue

class EngineTest {
    private val clock = T.clock()
    private val engine = T.engine(clock)
    private val demo = T.demo(engine)
    private val current = demo.accounts.first { it.type == AccountType.CURRENT }
    private val savings = demo.accounts.first { it.type == AccountType.SAVINGS }
    private val invest = demo.accounts.first { it.type == AccountType.INVESTMENT }

    // ------------------------------------------------------------ données de démonstration

    @Test fun `profil de demonstration aux soldes demandes`() {
        assertEquals(Money.of("4850.75"), current.balance)
        assertEquals(Money.of("12400.00"), savings.balance)
        assertEquals(Money.of("2150.00"), demo.accounts.first { it.type == AccountType.PROJECT }.balance)
        assertEquals(6, demo.accounts.size)
        assertEquals(AccountType.entries.toSet(), demo.accounts.map { it.type }.toSet())
        assertTrue(demo.transactions.size > 200, "historique réaliste attendu")
        assertTrue(demo.transactions.all { it.isDemo })
        assertConsistent(demo)
        // Toutes les catégories de dépenses principales sont représentées.
        val cats = demo.transactions.map { it.category }.toSet()
        for (c in listOf(Category.FOOD, Category.SHOPPING, Category.TRANSPORT, Category.LEISURE, Category.HOUSING,
            Category.SUBSCRIPTIONS, Category.RESTAURANTS, Category.TRANSFERS, Category.INCOME)) assertTrue(c in cats, "catégorie $c absente")
    }

    @Test fun `le jeu de demonstration est deterministe`() {
        val again = DemoData.create(engine.clock, com.superessence.essabank.core.engine.SeededIds(42), engine.market)
        assertEquals(demo.transactions.size, again.transactions.size)
        assertEquals(demo.transactions.map { it.amount }, again.transactions.map { it.amount })
    }

    @Test fun `numeros de carte fictifs inutilisables`() {
        val ids = com.superessence.essabank.core.engine.SeededIds(1)
        repeat(2000) {
            val n = FictionalIdentifiers.cardNumber(ids)
            assertEquals(16, n.length)
            assertTrue(n.startsWith("0"), "préfixe non attribué attendu")
            assertFalse(FictionalIdentifiers.isLuhnValid(n), "un numéro fictif ne doit jamais passer le contrôle de Luhn")
        }
        assertTrue(FictionalIdentifiers.isLuhnValid("4539578763621486")) // contrôle de l'algorithme sur un exemple de test public
        for (c in demo.cards) assertFalse(FictionalIdentifiers.isLuhnValid(c.number))
    }

    // ------------------------------------------------------------ virements internes

    @Test fun `virement interne debite et credite le meme montant`() {
        val amount = Money.of("321.45")
        val r = engine.transferInternal(demo, current.id, savings.id, amount, "Test", requestId = "req-1").ok()
        val s = r.state
        assertEquals(current.balance - amount, s.account(current.id)!!.balance)
        assertEquals(savings.balance + amount, s.account(savings.id)!!.balance)
        assertEquals(demo.accounts.sumOf { it.balance.cents }, s.accounts.sumOf { it.balance.cents }, "le total doit être conservé")
        assertEquals(2, r.value.size)
        assertEquals(r.value[0].transferId, r.value[1].transferId)
        assertEquals(s.account(current.id)!!.balance, r.value[0].balanceAfter)
        assertConsistent(s)
    }

    @Test fun `montants invalides refuses`() {
        assertEquals(BankError.InvalidAmount, engine.transferInternal(demo, current.id, savings.id, Money.ZERO).errorOrNull())
        assertEquals(BankError.InvalidAmount, engine.transferInternal(demo, current.id, savings.id, Money(-100)).errorOrNull())
        assertEquals(BankError.AmountTooLarge, engine.transferInternal(demo, current.id, savings.id, Money.units(10_000_001)).errorOrNull())
        assertEquals(BankError.SameAccount, engine.transferInternal(demo, current.id, current.id, Money(100)).errorOrNull())
        assertEquals(BankError.AccountNotFound, engine.transferInternal(demo, "nope", current.id, Money(100)).errorOrNull())
    }

    @Test fun `solde insuffisant refuse sans aucune modification`() {
        val tooMuch = current.available + Money(1)
        val r = engine.transferInternal(demo, current.id, savings.id, tooMuch)
        assertIs<EngineResult.Err>(r)
        assertIs<BankError.InsufficientFunds>(r.error)
        assertEquals(current.available, (r.error as BankError.InsufficientFunds).available)
        // Le découvert virtuel autorisé est utilisable jusqu'au dernier centime.
        val exact = engine.transferInternal(demo, current.id, savings.id, current.available).ok().state
        assertEquals(-current.overdraft, exact.account(current.id)!!.balance)
        assertConsistent(exact)
    }

    @Test fun `une meme requete ne peut pas etre executee deux fois`() {
        val s1 = engine.transferInternal(demo, current.id, savings.id, Money.units(10), requestId = "double-tap").ok().state
        val r2 = engine.transferInternal(s1, current.id, savings.id, Money.units(10), requestId = "double-tap")
        assertEquals(BankError.DuplicateRequest, r2.errorOrNull())
        assertEquals(current.balance - Money.units(10), s1.account(current.id)!!.balance)
    }

    // ------------------------------------------------------------ bénéficiaires

    @Test fun `virement vers beneficiaire fictif et reception simulee`() {
        val ben = engine.addBeneficiary(demo, "Zoé Fictive", null, "Amie").ok()
        assertTrue(FictionalIdentifiers.isValidBeneficiaryReference(ben.value.reference))
        val sent = engine.sendToBeneficiary(ben.state, current.id, ben.value.id, Money.of("75.20"), "Concert", "Places", "r1").ok()
        assertEquals(current.balance - Money.of("75.20"), sent.state.account(current.id)!!.balance)
        assertEquals(TxKind.TRANSFER_OUT, sent.value.kind)
        assertEquals("Zoé Fictive", sent.value.counterparty)
        assertTrue(sent.state.notifications.any { it.type == NotificationType.TRANSFER_SENT })
        val recv = engine.receiveSimulated(sent.state, current.id, "Zoé Fictive", Money.of("20.00"), "Remboursement", "r2").ok()
        assertEquals(current.balance - Money.of("55.20"), recv.state.account(current.id)!!.balance)
        assertConsistent(recv.state)
        // Références invalides refusées
        assertEquals(BankError.InvalidBeneficiary, engine.addBeneficiary(demo, "X", "FR76 3000 6000 0112 3456 7890 189").errorOrNull())
        assertEquals(BankError.InvalidName, engine.addBeneficiary(demo, "   ").errorOrNull())
    }

    @Test fun `supprimer un beneficiaire annule ses virements programmes`() {
        val ben = demo.beneficiaries.first { b -> demo.scheduled.any { it.beneficiaryId == b.id } }
        val s = engine.deleteBeneficiary(demo, ben.id).ok().state
        assertTrue(s.scheduled.none { it.beneficiaryId == ben.id })
    }

    // ------------------------------------------------------------ virements programmés

    @Test fun `virement programme mensuel avec rattrapage`() {
        val today = clock.today()
        val sch = engine.scheduleTransfer(demo, current.id, savings.id, null, Money.units(100), "Mensuel", "", today, Frequency.MONTHLY).ok()
        val s1 = engine.processDue(sch.state).ok()
        assertEquals(1, s1.value)
        assertEquals(current.balance - Money.units(100), s1.state.account(current.id)!!.balance)
        // Trois mois plus tard : trois exécutions de rattrapage.
        clock.advanceDays(92)
        val s2 = engine.processDue(s1.state).ok()
        val updated = s2.state.scheduled.first { it.id == sch.value.id }
        assertTrue(updated.executions >= 4)
        assertEquals(today.plusMonths(updated.executions.toLong()).toEpochDay(), updated.nextDate)
        assertConsistent(s2.state)
    }

    @Test fun `virement programme unique puis desactive`() {
        val tomorrow = clock.today().plusDays(1)
        val sch = engine.scheduleTransfer(demo, current.id, savings.id, null, Money.units(50), "", "", tomorrow, Frequency.ONCE).ok()
        assertEquals(0, engine.processDue(sch.state).ok().value, "pas encore à échéance")
        clock.advanceDays(1)
        val r = engine.processDue(sch.state).ok()
        assertEquals(1, r.value)
        assertFalse(r.state.scheduled.first { it.id == sch.value.id }.active)
        assertEquals(BankError.InvalidDate, engine.scheduleTransfer(demo, current.id, savings.id, null, Money.units(5), "", "",
            clock.today().minusDays(1), Frequency.ONCE).errorOrNull())
    }

    @Test fun `virement programme sans provision non execute et notifie`() {
        val sch = engine.scheduleTransfer(demo, current.id, savings.id, null, current.available + Money.units(1), "Trop", "",
            clock.today(), Frequency.ONCE).ok()
        val r = engine.processDue(sch.state).ok()
        assertEquals(0, r.value)
        assertEquals(current.balance, r.state.account(current.id)!!.balance)
        assertTrue(r.state.notifications.any { it.type == NotificationType.SCHEDULED_FAILED })
        assertEquals(1, r.state.scheduled.first { it.id == sch.value.id }.failures)
    }

    // ------------------------------------------------------------ cartes

    @Test fun `cycle de vie d une carte`() {
        val s0 = engine.setTier(demo, Tier.BLACK).ok().state
        val created = engine.createCard(s0, current.id, CardType.PREMIUM, "Voyage", CardDesign.ONYX, Money.units(1000)).ok()
        val card = created.value
        assertFalse(FictionalIdentifiers.isLuhnValid(card.number))
        assertEquals("CAMILLE LAURENT", card.holderName)
        assertTrue(created.state.notifications.first().type == NotificationType.CARD_CREATED)
        var s = created.state
        val pay = engine.cardPayment(s, card.id, "Librairie Papyrus", Category.SHOPPING, Money.of("42.90")).ok()
        assertEquals(card.id, pay.value.cardId)
        s = pay.state
        s = engine.setCardStatus(s, card.id, CardStatus.LOCKED).ok().state
        assertEquals(BankError.CardLocked, engine.cardPayment(s, card.id, "X", Category.FOOD, Money.units(1)).errorOrNull())
        s = engine.setCardStatus(s, card.id, CardStatus.INACTIVE).ok().state
        assertEquals(BankError.CardInactive, engine.cardPayment(s, card.id, "X", Category.FOOD, Money.units(1)).errorOrNull())
        s = engine.setCardStatus(s, card.id, CardStatus.ACTIVE).ok().state
        s = engine.updateCard(s, card.id, monthlyLimit = Money.units(50), design = CardDesign.AURORA, onlinePayments = false).ok().state
        assertEquals(BankError.OnlineDisabled, engine.cardPayment(s, card.id, "Web", Category.SHOPPING, Money.units(1), online = true).errorOrNull())
        // Plafond mensuel : 42,90 déjà dépensés sur 50.
        val limit = engine.cardPayment(s, card.id, "X", Category.FOOD, Money.units(8)).errorOrNull()
        assertEquals(BankError.CardLimitExceeded(Money.of("7.10")), limit)
        assertTrue(engine.cardPayment(s, card.id, "X", Category.FOOD, Money.of("7.10")).isOk)
        s = engine.deleteCard(s, card.id).ok().state
        assertTrue(s.card(card.id) == null)
        assertConsistent(s)
    }

    @Test fun `carte ephemere a usage unique`() {
        val c = engine.createCard(demo, current.id, CardType.EPHEMERAL, "Unique", CardDesign.GLACIER, Money.units(200))
        // La formule PLUS autorise 10 cartes.
        val created = c.ok()
        val r = engine.cardPayment(created.state, created.value.id, "Boutique Céleste", Category.SHOPPING, Money.units(20)).ok()
        val after = r.state.card(created.value.id)!!
        assertEquals(CardStatus.INACTIVE, after.status)
        assertEquals(0, after.usesLeft)
        assertEquals(BankError.CardInactive, engine.cardPayment(r.state, after.id, "Y", Category.SHOPPING, Money.units(1)).errorOrNull())
        assertEquals(BankError.CardExpired, engine.setCardStatus(r.state, after.id, CardStatus.ACTIVE).errorOrNull())
    }

    @Test fun `limites de formule et designs premium`() {
        val std = engine.setTier(demo, Tier.STANDARD).ok().state
        assertEquals(3, std.cards.size)
        assertEquals(BankError.LimitReached, engine.createCard(std, current.id, CardType.VIRTUAL, "4e", CardDesign.MIDNIGHT, Money.units(100)).errorOrNull())
        assertEquals(BankError.PremiumRequired, engine.updateCard(std, std.cards.first().id, design = CardDesign.GOLD_LEAF).errorOrNull())
        assertEquals(BankError.LimitReached, engine.createGoal(std, "4e objectif", GoalKind.FREE, Money.units(100), null, 0).errorOrNull())
        val plus = engine.setTier(std, Tier.PLUS).ok().state
        assertTrue(engine.createGoal(plus, "4e objectif", GoalKind.FREE, Money.units(100), null, 0).isOk)
    }

    @Test fun `arrondi automatique vers le coffre`() {
        val s0 = engine.updateAccount(demo, current.id, roundUp = true).ok().state
        val card = s0.cards.first { it.accountId == current.id && it.status == CardStatus.ACTIVE }
        val goal = s0.goals.sortedBy { it.sortOrder }.first { !it.isReached }
        val r = engine.cardPayment(s0, card.id, "Café Horizon", Category.RESTAURANTS, Money.of("3.40")).ok()
        assertEquals(goal.saved + Money.of("0.60"), r.state.goal(goal.id)!!.saved)
        assertEquals(current.balance - Money.of("4.00"), r.state.account(current.id)!!.balance)
        assertConsistent(r.state)
    }

    // ------------------------------------------------------------ budgets

    @Test fun `alerte de budget emise une seule fois par seuil`() {
        var s = engine.setMonthlyBudget(demo, null).ok().state
        s = s.copy(budgets = s.budgets.copy(categories = emptyMap(), alertsSent = emptySet()))
        val (spent, _) = engine.monthSpending(s)
        val healthSpent = engine.monthSpending(s).second[Category.HEALTH] ?: Money.ZERO
        val budget = healthSpent + Money.units(100)
        s = engine.setCategoryBudget(s, Category.HEALTH, budget).ok().state
        val card = s.cards.first { it.accountId == current.id }
        // Passage à 85 %
        val to85 = Money(budget.cents * 85 / 100) - healthSpent
        val r1 = engine.cardPayment(s, card.id, "Pharmacie des Tilleuls", Category.HEALTH, to85).ok()
        assertTrue(r1.events.any { it is BankEvent.BudgetAlert && it.key.endsWith("HEALTH:80") })
        // Dépassement
        val r2 = engine.cardPayment(r1.state, card.id, "Pharmacie des Tilleuls", Category.HEALTH, Money(budget.cents * 20 / 100)).ok()
        assertTrue(r2.events.any { it is BankEvent.BudgetAlert && it.key.endsWith("HEALTH:100") })
        assertEquals(NotificationType.BUDGET_EXCEEDED, r2.state.notifications.first().type)
        // Pas de doublon
        val r3 = engine.cardPayment(r2.state, card.id, "Pharmacie des Tilleuls", Category.HEALTH, Money.units(5)).ok()
        assertTrue(r3.events.none { it is BankEvent.BudgetAlert })
        assertTrue(spent.isPositive)
    }

    // ------------------------------------------------------------ épargne

    @Test fun `coffres depot retrait objectif atteint et suppression`() {
        val g = engine.createGoal(demo, "Vélo", GoalKind.FREE, Money.units(300), clock.today().plusMonths(3), 2).ok()
        val d1 = engine.depositToGoal(g.state, g.value.id, current.id, Money.units(200), "g1").ok()
        assertEquals(Money.units(200), d1.value.saved)
        assertFalse(d1.value.isReached)
        val d2 = engine.depositToGoal(d1.state, g.value.id, current.id, Money.units(100), "g2").ok()
        assertTrue(d2.value.isReached)
        assertTrue(d2.events.any { it is BankEvent.GoalReached })
        assertEquals(NotificationType.GOAL_REACHED, d2.state.notifications.first().type)
        val w = engine.withdrawFromGoal(d2.state, g.value.id, savings.id, Money.units(50), "g3").ok()
        assertEquals(Money.units(250), w.value.saved)
        assertEquals(BankError.GoalInsufficient, engine.withdrawFromGoal(w.state, g.value.id, savings.id, Money.units(251), null).errorOrNull())
        val del = engine.deleteGoal(w.state, g.value.id, current.id).ok().state
        assertEquals(current.balance - Money.units(300) + Money.units(250), del.account(current.id)!!.balance)
        assertEquals(savings.balance + Money.units(50), del.account(savings.id)!!.balance)
        assertConsistent(del)
    }

    @Test fun `epargne automatique hebdomadaire`() {
        val g = engine.createGoal(demo, "Auto", GoalKind.FREE, Money.units(1000), null, 0).ok()
        val s = engine.setAutoSave(g.state, g.value.id, Money.units(25), Frequency.WEEKLY, current.id, clock.today()).ok().state
        val r1 = engine.processDue(s).ok()
        assertEquals(Money.units(25), r1.state.goal(g.value.id)!!.saved)
        clock.advanceDays(21)
        val r2 = engine.processDue(r1.state).ok()
        assertEquals(Money.units(100), r2.state.goal(g.value.id)!!.saved)
        assertConsistent(r2.state)
    }

    // ------------------------------------------------------------ investissements

    @Test fun `achat et vente virtuels avec plus-value realisee`() {
        val buy = engine.buy(demo, "QNTX.SIM", Money.units(500), invest.id, "b1").ok()
        val price = engine.market.price("QNTX.SIM", clock.now())
        assertEquals(price, buy.value.price)
        assertTrue(buy.value.total <= Money.units(500))
        val h = buy.state.holdings.first { it.symbol == "QNTX.SIM" }
        assertEquals(buy.value.quantityMicro, h.quantityMicro)
        clock.advanceDays(30)
        val half = h.quantityMicro / 2
        val sell = engine.sell(buy.state, "QNTX.SIM", half, invest.id, "s1").ok()
        val newPrice = engine.market.price("QNTX.SIM", clock.now())
        val proceeds = com.superessence.essabank.core.market.MarketSimulator.value(newPrice, half)
        assertEquals(proceeds, sell.value.total)
        val costHalf = Money.of(h.costBasis.toBigDecimal().multiply(java.math.BigDecimal(half)).divide(java.math.BigDecimal(h.quantityMicro), 10, java.math.RoundingMode.HALF_EVEN))
        assertEquals(proceeds - costHalf, sell.value.realized)
        assertEquals(BankError.InsufficientQuantity, engine.sell(sell.state, "QNTX.SIM", h.quantityMicro, invest.id).errorOrNull())
        assertEquals(BankError.InvalidAccountType, engine.buy(demo, "QNTX.SIM", Money.units(10), current.id).errorOrNull())
        assertConsistent(sell.state)
    }

    // ------------------------------------------------------------ comptes, profil, démonstration

    @Test fun `creation renommage et suppression de compte`() {
        val r = engine.createAccount(demo, AccountType.PROJECT, "  Studio   photo ", 3, "camera").ok()
        assertEquals("Studio photo", r.value.name)
        var s = engine.updateAccount(r.state, r.value.id, name = "Studio", colorIndex = 4).ok().state
        assertEquals("Studio", s.account(r.value.id)!!.name)
        s = engine.transferInternal(s, current.id, r.value.id, Money.units(10)).ok().state
        assertEquals(BankError.AccountNotEmpty, engine.deleteAccount(s, r.value.id).errorOrNull())
        assertEquals(BankError.PrimaryAccount, engine.deleteAccount(s, current.id).errorOrNull())
        s = engine.transferInternal(s, r.value.id, current.id, Money.units(10)).ok().state
        s = engine.deleteAccount(s, r.value.id).ok().state
        assertTrue(s.account(r.value.id) == null)
        assertConsistent(s)
    }

    @Test fun `suppression des donnees de demonstration conserve les soldes`() {
        val withReal = engine.transferInternal(demo, current.id, savings.id, Money.units(15)).ok().state
        val r = engine.clearDemoTransactions(withReal).ok()
        assertTrue(r.value > 200)
        assertTrue(r.state.transactions.none { it.isDemo })
        for (a in withReal.accounts) assertEquals(a.balance, r.state.account(a.id)!!.balance)
        assertTrue(r.state.transactions.any { it.kind == TxKind.OPENING })
        assertConsistent(r.state.copy(goals = emptyList()))
    }

    @Test fun `changement de nom du profil repercute sur les cartes`() {
        val s = engine.updateProfile(demo) { it.copy(firstName = "Alex", lastName = "Durand") }.ok().state
        assertTrue(s.cards.all { it.holderName == "ALEX DURAND" })
        assertEquals(BankError.InvalidName, engine.updateProfile(demo) { it.copy(firstName = " ") }.errorOrNull())
    }

    @Test fun `resume mensuel et interets fictifs au changement de mois`() {
        clock.advanceDays(25) // passage en décembre
        val r = engine.processDue(demo).ok()
        assertTrue(r.state.notifications.any { it.type == NotificationType.MONTHLY_SUMMARY && it.refId == "2026-11" })
        val interest = r.state.transactions.filter { it.kind == TxKind.INTEREST && it.accountId == savings.id }
        assertEquals(1, interest.size)
        // 12 400 + virements programmés exécutés, à 2,50 % / 12
        assertTrue(interest.first().amount.isPositive)
        // Pas de second résumé au passage suivant
        val again = engine.processDue(r.state).ok()
        assertEquals(r.state.notifications.count { it.type == NotificationType.MONTHLY_SUMMARY },
            again.state.notifications.count { it.type == NotificationType.MONTHLY_SUMMARY })
        assertConsistent(again.state)
    }

    @Test fun `notifications desactivees par type`() {
        val s = engine.updatePreferences(demo) { it.copy(notifications = it.notifications.copy(transfers = false)) }.ok().state
        val ben = s.beneficiaries.first()
        val r = engine.sendToBeneficiary(s, current.id, ben.id, Money.units(1), "x").ok()
        assertEquals(s.notifications.count { it.type == NotificationType.TRANSFER_SENT },
            r.state.notifications.count { it.type == NotificationType.TRANSFER_SENT })
    }

    @Test fun `textes en anglais`() {
        val en = engine.updatePreferences(demo) { it.copy(language = AppLanguage.EN) }.ok().state
        val r = engine.transferInternal(en, current.id, savings.id, Money.units(1)).ok()
        assertEquals("Internal transfer", r.value.first().label)
    }

    /** Test de propriété : 1 500 opérations aléatoires ne cassent jamais les invariants comptables. */
    @Test fun `invariants comptables sous operations aleatoires`() {
        val rnd = Random(2026)
        var s = demo
        val accounts = s.accounts.map { it.id }
        val total0 = s.accounts.sumOf { it.balance.cents } + s.goals.sumOf { it.saved.cents }
        var external = 0L
        var investFlows = 0L
        repeat(1500) { i ->
            val a = accounts[rnd.nextInt(accounts.size)]; val b = accounts[rnd.nextInt(accounts.size)]
            val amount = Money(rnd.nextLong(-500, 300_000))
            val r = when (rnd.nextInt(6)) {
                0, 1 -> engine.transferInternal(s, a, b, amount, requestId = "p$i")
                2 -> engine.sendToBeneficiary(s, a, s.beneficiaries.first().id, amount, "p").also { if (it.isOk) external -= amount.cents }
                3 -> engine.receiveSimulated(s, a, "Test", amount).also { if (it.isOk) external += amount.cents }
                4 -> engine.depositToGoal(s, s.goals[rnd.nextInt(s.goals.size)].id, a, amount, null)
                else -> engine.cardPayment(s, s.cards.first().id, "Commerce", Category.OTHER, amount).also { if (it.isOk) external -= amount.cents }
            }
            r.stateOrNull()?.let { s = it }
            if (i % 100 == 0) { assertConsistent(s); assertNotNull(s.primaryAccount) }
        }
        assertConsistent(s)
        val total = s.accounts.sumOf { it.balance.cents } + s.goals.sumOf { it.saved.cents }
        assertEquals(total0 + external + investFlows, total, "aucune unité virtuelle ne doit apparaître ou disparaître")
        // Aucun compte sous son découvert autorisé
        for (acc in s.accounts) assertTrue(acc.balance >= -acc.overdraft)
    }
}
