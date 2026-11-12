package com.superessence.essabank.core.demo

import com.superessence.essabank.core.engine.BankClock
import com.superessence.essabank.core.engine.Dates
import com.superessence.essabank.core.engine.FictionalIdentifiers
import com.superessence.essabank.core.engine.IdGenerator
import com.superessence.essabank.core.engine.Labels
import com.superessence.essabank.core.engine.NotificationTexts
import com.superessence.essabank.core.market.MarketSimulator
import com.superessence.essabank.core.model.Account
import com.superessence.essabank.core.model.AccountType
import com.superessence.essabank.core.model.AppNotification
import com.superessence.essabank.core.model.BankState
import com.superessence.essabank.core.model.Beneficiary
import com.superessence.essabank.core.model.Budgets
import com.superessence.essabank.core.model.Card
import com.superessence.essabank.core.model.CardDesign
import com.superessence.essabank.core.model.CardType
import com.superessence.essabank.core.model.Category
import com.superessence.essabank.core.model.Direction
import com.superessence.essabank.core.model.Frequency
import com.superessence.essabank.core.model.GoalKind
import com.superessence.essabank.core.model.GoalMoveType
import com.superessence.essabank.core.model.GoalMovement
import com.superessence.essabank.core.model.Holding
import com.superessence.essabank.core.model.Meta
import com.superessence.essabank.core.model.NotificationType
import com.superessence.essabank.core.model.Preferences
import com.superessence.essabank.core.model.Profile
import com.superessence.essabank.core.model.SavingsGoal
import com.superessence.essabank.core.model.ScheduledTransfer
import com.superessence.essabank.core.model.Tier
import com.superessence.essabank.core.model.Trade
import com.superessence.essabank.core.model.TradeSide
import com.superessence.essabank.core.model.Transaction
import com.superessence.essabank.core.model.TxKind
import com.superessence.essabank.core.money.AppLanguage
import com.superessence.essabank.core.money.Money
import java.time.DayOfWeek
import java.time.LocalDate
import java.time.LocalTime
import kotlin.random.Random

/**
 * Profil de démonstration prérempli. Toutes les personnes, enseignes et montants sont
 * inventés ; chaque transaction est marquée [Transaction.isDemo].
 *
 * Soldes finaux garantis : compte courant 4 850,75 €, épargne 12 400,00 €, projet 2 150,00 € (euros virtuels).
 */
object DemoData {

    val CURRENT_BALANCE = Money.of("4850.75")
    val SAVINGS_BALANCE = Money.of("12400.00")
    val PROJECT_BALANCE = Money.of("2150.00")
    val VACATION_BALANCE = Money.of("1320.00")
    val BUSINESS_BALANCE = Money.of("6780.40")
    val INVEST_CASH = Money.of("1250.00")

    private const val HISTORY_DAYS = 150

    fun create(clock: BankClock, ids: IdGenerator, market: MarketSimulator, language: AppLanguage = AppLanguage.FR, seed: Int = 7): BankState {
        val fr = language == AppLanguage.FR
        val zone = clock.zone
        val now = clock.now()
        val today = clock.today()
        val rnd = Random(seed)
        val created = now - HISTORY_DAYS * 86_400_000L

        val profile = Profile(
            firstName = "Camille", lastName = "Laurent", email = "camille.laurent@exemple.sim", city = "Lyon",
            avatar = 2, clientNumber = FictionalIdentifiers.clientNumber(ids), createdAt = created,
            tier = Tier.PLUS, tierSince = created, preferences = Preferences(language = language), isDemo = true,
        )

        fun acc(type: AccountType, name: String, color: Int, icon: String, target: Money, order: Int, primary: Boolean = false,
                overdraft: Money = Money.ZERO, interest: Int = 0) =
            Account(ids.id("acc"), type, name, color, icon, target, FictionalIdentifiers.accountReference(ids), created,
                overdraft, interest, primary, sortOrder = order)

        val current = acc(AccountType.CURRENT, if (fr) "Compte courant" else "Current account", 0, "wallet", CURRENT_BALANCE, 0, true, Money.units(500))
        val savings = acc(AccountType.SAVINGS, if (fr) "Épargne" else "Savings", 1, "savings", SAVINGS_BALANCE, 1, interest = 250)
        val project = acc(AccountType.PROJECT, if (fr) "Projet atelier" else "Workshop project", 2, "rocket", PROJECT_BALANCE, 2)
        val vacation = acc(AccountType.VACATION, if (fr) "Vacances" else "Holidays", 3, "beach", VACATION_BALANCE, 3)
        val business = acc(AccountType.BUSINESS, if (fr) "Activité freelance" else "Freelance business", 4, "briefcase", BUSINESS_BALANCE, 4)
        val invest = acc(AccountType.INVESTMENT, if (fr) "Investissement fictif" else "Fictional investing", 5, "chart", INVEST_CASH, 5)
        val accounts = listOf(current, savings, project, vacation, business, invest)

        val holder = profile.fullName.uppercase()
        fun card(type: CardType, label: String, design: CardDesign, limit: Int, ageDays: Int, account: Account = current): Card {
            val exp = today.plusYears(3).plusMonths(rnd.nextLong(1, 11))
            return Card(ids.id("card"), account.id, type, label, holder, FictionalIdentifiers.cardNumber(ids), exp.monthValue, exp.year,
                FictionalIdentifiers.cvv(ids), design, monthlyLimit = Money.units(limit), createdAt = now - ageDays * 86_400_000L,
                contactless = type != CardType.VIRTUAL)
        }
        val physical = card(CardType.PHYSICAL, if (fr) "Carte principale" else "Main card", CardDesign.MIDNIGHT, 2500, HISTORY_DAYS)
        val virtual = card(CardType.VIRTUAL, if (fr) "Achats en ligne" else "Online shopping", CardDesign.ELECTRIC, 800, 120)
        val secondary = card(CardType.SECONDARY, if (fr) "Carte freelance" else "Freelance card", CardDesign.GRAPHITE, 1500, 90, business)
        val cards = listOf(physical, virtual, secondary)

        // --------------------------------------------------------- génération des écritures
        data class Ev(val at: Long, val accountId: String, val dir: Direction, val amount: Money, val cat: Category, val kind: TxKind,
                      val label: String, val counterparty: String?, val cardId: String? = null, val transferId: String? = null)
        val events = ArrayList<Ev>()
        val start = today.minusDays(HISTORY_DAYS.toLong() - 1)
        fun at(d: LocalDate, h: Int, m: Int): Long {
            val t = Dates.startOfDay(d, zone) + LocalTime.of(h, m).toSecondOfDay() * 1000L
            return minOf(t, now - 60_000L)
        }
        fun money(min: Int, max: Int): Money = Money(rnd.nextLong(min * 100L, max * 100L + 1))

        val groceries = listOf("Marché Lumière", "Épicerie Verte", "Primeur des Halles Sim", "Boulangerie du Port", "Fromagerie Alpine Sim")
        val restaurants = listOf("Bistrot Saint-Clair", "Sushi Komorebi", "Pizzeria Bella Sim", "Café Horizon", "Brasserie des Arts Sim")
        val shopping = listOf("Maison Arcane", "Atelier Nova", "Librairie Papyrus", "Boutique Céleste", "Électro Sim")
        val transport = listOf("Métro Sim", "Taxi Comète", "Station Énergie Sim", "Vélo Libre Sim", "Rail Express Sim")
        val leisure = listOf("Cinéma Odyssée", "Salle Pulse Fitness", "Musée des Lumières Sim", "Théâtre du Parc Sim")
        val health = listOf("Pharmacie des Tilleuls", "Cabinet Santé Sim", "Opticien Clair Sim")

        var d = start
        while (!d.isAfter(today)) {
            val dom = d.dayOfMonth
            // Revenus
            if (dom == 28) events += Ev(at(d, 8, 5), current.id, Direction.CREDIT, Money.of("3240.00"), Category.INCOME, TxKind.INCOME,
                if (fr) "Salaire · Studio Fictif SAS" else "Salary · Studio Fictif SAS", "Studio Fictif SAS")
            if (dom == 12 || dom == 26) events += Ev(at(d, 11, 20), business.id, Direction.CREDIT, money(650, 1900), Category.INCOME, TxKind.INCOME,
                (if (fr) "Facture client · " else "Client invoice · ") + listOf("Agence Orion Sim", "Collectif Prisme Sim", "Maison Hélios Sim")[rnd.nextInt(3)], null)
            // Charges fixes
            if (dom == 3) events += Ev(at(d, 7, 0), current.id, Direction.DEBIT, Money.of("890.00"), Category.HOUSING, TxKind.PAYMENT,
                if (fr) "Loyer · Résidence Les Cèdres" else "Rent · Résidence Les Cèdres", "Résidence Les Cèdres")
            if (dom == 5) events += Ev(at(d, 6, 30), current.id, Direction.DEBIT, money(62, 88), Category.HOUSING, TxKind.PAYMENT,
                if (fr) "Énergie Maison Sim" else "Home Energy Sim", "Énergie Maison Sim")
            if (dom == 8) events += Ev(at(d, 9, 0), current.id, Direction.DEBIT, Money.of("13.99"), Category.SUBSCRIPTIONS, TxKind.CARD_PAYMENT,
                "StreamBox Sim", "StreamBox Sim", virtual.id)
            if (dom == 10) events += Ev(at(d, 9, 0), current.id, Direction.DEBIT, Money.of("10.99"), Category.SUBSCRIPTIONS, TxKind.CARD_PAYMENT,
                "Musique Plus Sim", "Musique Plus Sim", virtual.id)
            if (dom == 15) events += Ev(at(d, 9, 30), current.id, Direction.DEBIT, Money.of("19.99"), Category.SUBSCRIPTIONS, TxKind.PAYMENT,
                "Mobile Sim", "Mobile Sim")
            if (dom == 18) events += Ev(at(d, 10, 0), business.id, Direction.DEBIT, Money.of("29.00"), Category.SUBSCRIPTIONS, TxKind.CARD_PAYMENT,
                if (fr) "Logiciel Créa Sim" else "Creative Software Sim", "Logiciel Créa Sim", secondary.id)
            if (dom == 2) {
                val trf = ids.id("trf")
                events += Ev(at(d, 8, 0), current.id, Direction.DEBIT, Money.units(250), Category.TRANSFERS, TxKind.INTERNAL_TRANSFER,
                    if (fr) "Épargne mensuelle" else "Monthly savings", savings.name, transferId = trf)
                events += Ev(at(d, 8, 0), savings.id, Direction.CREDIT, Money.units(250), Category.TRANSFERS, TxKind.INTERNAL_TRANSFER,
                    if (fr) "Épargne mensuelle" else "Monthly savings", current.name, transferId = trf)
            }
            if (dom == 20) {
                val trf = ids.id("trf")
                events += Ev(at(d, 12, 0), current.id, Direction.DEBIT, Money.units(150), Category.TRANSFERS, TxKind.INTERNAL_TRANSFER,
                    if (fr) "Cagnotte vacances" else "Holiday fund", vacation.name, transferId = trf)
                events += Ev(at(d, 12, 0), vacation.id, Direction.CREDIT, Money.units(150), Category.TRANSFERS, TxKind.INTERNAL_TRANSFER,
                    if (fr) "Cagnotte vacances" else "Holiday fund", current.name, transferId = trf)
            }
            // Dépenses courantes
            if (rnd.nextFloat() < 0.55f) events += Ev(at(d, rnd.nextInt(8, 20), rnd.nextInt(60)), current.id, Direction.DEBIT, money(6, 74),
                Category.FOOD, TxKind.CARD_PAYMENT, groceries[rnd.nextInt(groceries.size)], null, physical.id)
            if (rnd.nextFloat() < 0.28f) events += Ev(at(d, rnd.nextInt(12, 22), rnd.nextInt(60)), current.id, Direction.DEBIT, money(11, 58),
                Category.RESTAURANTS, TxKind.CARD_PAYMENT, restaurants[rnd.nextInt(restaurants.size)], null, physical.id)
            if (rnd.nextFloat() < 0.33f) events += Ev(at(d, rnd.nextInt(7, 21), rnd.nextInt(60)), current.id, Direction.DEBIT, money(2, 36),
                Category.TRANSPORT, TxKind.CARD_PAYMENT, transport[rnd.nextInt(transport.size)], null, physical.id)
            if (rnd.nextFloat() < 0.12f) events += Ev(at(d, rnd.nextInt(10, 21), rnd.nextInt(60)), current.id, Direction.DEBIT, money(18, 140),
                Category.SHOPPING, TxKind.CARD_PAYMENT, shopping[rnd.nextInt(shopping.size)], null, if (rnd.nextBoolean()) virtual.id else physical.id)
            if ((d.dayOfWeek == DayOfWeek.SATURDAY || d.dayOfWeek == DayOfWeek.FRIDAY) && rnd.nextFloat() < 0.45f)
                events += Ev(at(d, rnd.nextInt(15, 23), rnd.nextInt(60)), current.id, Direction.DEBIT, money(9, 45),
                    Category.LEISURE, TxKind.CARD_PAYMENT, leisure[rnd.nextInt(leisure.size)], null, physical.id)
            if (rnd.nextFloat() < 0.05f) events += Ev(at(d, rnd.nextInt(9, 19), rnd.nextInt(60)), current.id, Direction.DEBIT, money(8, 65),
                Category.HEALTH, TxKind.CARD_PAYMENT, health[rnd.nextInt(health.size)], null, physical.id)
            if (rnd.nextFloat() < 0.04f) events += Ev(at(d, rnd.nextInt(9, 19), rnd.nextInt(60)), project.id, Direction.DEBIT, money(25, 180),
                Category.OTHER, TxKind.PAYMENT, if (fr) "Matériel · Atelier Bois Sim" else "Supplies · Atelier Bois Sim", "Atelier Bois Sim")
            if (rnd.nextFloat() < 0.03f) events += Ev(at(d, rnd.nextInt(9, 19), rnd.nextInt(60)), business.id, Direction.DEBIT, money(30, 220),
                Category.SHOPPING, TxKind.CARD_PAYMENT, if (fr) "Fournitures Bureau Sim" else "Office Supplies Sim", "Fournitures Bureau Sim", secondary.id)
            d = d.plusDays(1)
        }

        // Virements à des proches fictifs.
        val bens = listOf(
            Beneficiary(ids.id("ben"), "Léa Moreau", FictionalIdentifiers.beneficiaryReference(ids), note = if (fr) "Sœur" else "Sister", favorite = true, colorIndex = 1, createdAt = created),
            Beneficiary(ids.id("ben"), "Hugo Bernard", FictionalIdentifiers.beneficiaryReference(ids), note = if (fr) "Colocataire" else "Flatmate", favorite = true, colorIndex = 3, createdAt = created),
            Beneficiary(ids.id("ben"), "Inès Garnier", FictionalIdentifiers.beneficiaryReference(ids), colorIndex = 4, createdAt = created),
            Beneficiary(ids.id("ben"), if (fr) "Association Les Petits Pas (fictive)" else "Les Petits Pas charity (fictional)", FictionalIdentifiers.beneficiaryReference(ids), colorIndex = 2, createdAt = created),
        )
        events += Ev(at(today.minusDays(41), 19, 12), current.id, Direction.DEBIT, Money.of("45.00"), Category.TRANSFERS, TxKind.TRANSFER_OUT,
            if (fr) "Cadeau d'anniversaire" else "Birthday present", bens[0].name)
        events += Ev(at(today.minusDays(17), 20, 40), current.id, Direction.DEBIT, Money.of("62.50"), Category.TRANSFERS, TxKind.TRANSFER_OUT,
            if (fr) "Courses partagées" else "Shared groceries", bens[1].name)
        events += Ev(at(today.minusDays(9), 18, 2), current.id, Direction.CREDIT, Money.of("120.00"), Category.TRANSFERS, TxKind.TRANSFER_IN,
            if (fr) "Remboursement week-end" else "Weekend refund", bens[2].name)
        events += Ev(at(today.minusDays(64), 10, 15), current.id, Direction.DEBIT, Money.of("30.00"), Category.TRANSFERS, TxKind.TRANSFER_OUT,
            if (fr) "Don mensuel" else "Monthly donation", bens[3].name)

        // Coffres d'épargne et leurs dépôts.
        fun goal(name: String, kind: GoalKind, target: Money, months: Long?, color: Int, deposits: List<Pair<Int, Money>>, order: Int): Pair<SavingsGoal, List<GoalMovement>> {
            val id = ids.id("goal")
            val moves = deposits.map { (ago, amount) ->
                val t = at(today.minusDays(ago.toLong()), 9, 30)
                events += Ev(t, current.id, Direction.DEBIT, amount, Category.TRANSFERS, TxKind.SAVINGS_DEPOSIT,
                    (if (fr) "Coffre · " else "Vault · ") + name, name)
                GoalMovement(ids.id("gmv"), id, current.id, amount, GoalMoveType.DEPOSIT, t)
            }
            val saved = Money.sum(deposits.map { it.second })
            return SavingsGoal(id, name, kind, target, saved, months?.let { today.plusMonths(it).toEpochDay() }, color, created,
                completedAt = if (saved >= target) moves.last().timestamp else null, sortOrder = order) to moves
        }
        val (g1, m1) = goal(if (fr) "Voyage au Japon" else "Trip to Japan", GoalKind.VACATION, Money.units(4500), 8, 3,
            listOf(140 to Money.units(900), 110 to Money.units(600), 80 to Money.units(500), 50 to Money.units(450), 20 to Money.units(400)), 0)
        val (g2, m2) = goal(if (fr) "Nouvelle voiture" else "New car", GoalKind.CAR, Money.units(15000), 20, 0,
            listOf(130 to Money.units(1800), 95 to Money.units(1200), 60 to Money.units(700), 25 to Money.units(500)), 1)
        val (g3, m3) = goal(if (fr) "Nouveau téléphone" else "New phone", GoalKind.PHONE, Money.units(1200), null, 1,
            listOf(100 to Money.units(400), 70 to Money.units(400), 35 to Money.units(400)), 2)
        val goalAuto = g1.copy(autoSave = com.superessence.essabank.core.model.AutoSave(Money.units(100), Frequency.MONTHLY, current.id,
            today.withDayOfMonth(1).plusMonths(1).plusDays(4).toEpochDay()))

        // Achats fictifs passés sur le marché de démonstration.
        val trades = ArrayList<Trade>()
        fun buy(symbol: String, ago: Int, amount: Money) {
            val t = at(today.minusDays(ago.toLong()), 15, 30)
            val price = market.price(symbol, t)
            val q = MarketSimulator.quantityFor(amount, price)
            val total = MarketSimulator.value(price, q)
            trades += Trade(ids.id("trd"), symbol, TradeSide.BUY, q, price, total, Money.ZERO, t, invest.id)
            events += Ev(t, invest.id, Direction.DEBIT, total, Category.TRANSFERS, TxKind.INVEST_BUY,
                (if (fr) "Achat virtuel · " else "Virtual buy · ") + (market.asset(symbol)?.name ?: symbol), symbol)
        }
        buy("EWLD.SIM", 120, Money.units(1500))
        buy("ESC.SIM", 75, Money.units(600))
        buy("HLXR.SIM", 44, Money.units(700))
        buy("EGRN.SIM", 12, Money.units(400))
        val holdings = trades.groupBy { it.symbol }.map { (sym, l) -> Holding(sym, l.sumOf { it.quantityMicro }, Money.sum(l.map { it.total })) }

        // --------------------------------------------------------- soldes et solde d'ouverture
        events.sortBy { it.at }
        val txs = ArrayList<Transaction>()
        for (a in accounts) {
            val accEvents = events.filter { it.accountId == a.id }
            val net = accEvents.sumOf { if (it.dir == Direction.CREDIT) it.amount.cents else -it.amount.cents }
            val opening = Money(a.balance.cents - net)
            var bal = Money.ZERO
            if (!opening.isZero) {
                bal = opening
                txs += Transaction(ids.id("tx"), a.id, opening.abs(), if (opening.isNegative) Direction.DEBIT else Direction.CREDIT,
                    Category.OTHER, TxKind.OPENING, if (fr) "Solde d'ouverture (simulation)" else "Opening balance (simulation)",
                    timestamp = created - 3_600_000L, balanceAfter = opening, reference = FictionalIdentifiers.receiptReference(ids), isDemo = true)
            }
            for (e in accEvents) {
                bal = if (e.dir == Direction.CREDIT) bal + e.amount else bal - e.amount
                txs += Transaction(ids.id("tx"), a.id, e.amount, e.dir, e.cat, e.kind, e.label, "", e.at, balanceAfter = bal,
                    cardId = e.cardId, counterparty = e.counterparty, transferId = e.transferId,
                    reference = FictionalIdentifiers.receiptReference(ids), isDemo = true)
            }
            check(bal == a.balance) { "Incohérence du jeu de démonstration" }
        }
        txs.sortBy { it.timestamp }
        val ordered = txs.mapIndexed { i, t -> t.copy(seq = i + 1L) }
        txs.clear(); txs.addAll(ordered)

        // --------------------------------------------------------- programmations, budgets, notifications
        val nextFirst = today.withDayOfMonth(1).plusMonths(1)
        val scheduled = listOf(
            ScheduledTransfer(ids.id("sch"), current.id, toAccountId = savings.id, amount = Money.units(250),
                label = if (fr) "Épargne mensuelle" else "Monthly savings", nextDate = nextFirst.plusDays(1).toEpochDay(),
                frequency = Frequency.MONTHLY, createdAt = created, executions = 4),
            ScheduledTransfer(ids.id("sch"), current.id, beneficiaryId = bens[3].id, amount = Money.units(30),
                label = if (fr) "Don mensuel" else "Monthly donation", nextDate = today.plusDays(6).toEpochDay(),
                frequency = Frequency.MONTHLY, createdAt = created, executions = 2),
        )
        val budgets = Budgets(
            monthly = Money.units(2400),
            categories = mapOf(Category.FOOD to Money.units(450), Category.RESTAURANTS to Money.units(220),
                Category.SHOPPING to Money.units(300), Category.LEISURE to Money.units(150), Category.TRANSPORT to Money.units(160)),
        )

        val texts = NotificationTexts(profile.preferences)
        val notes = ArrayList<AppNotification>()
        fun note(type: NotificationType, p: Pair<String, String>, ago: Long, read: Boolean, ref: String? = null) {
            notes += AppNotification(ids.id("ntf"), type, p.first, p.second, now - ago, read, ref)
        }
        val prev = today.minusMonths(1)
        val prevTx = txs.filter { Dates.monthKey(it.timestamp, zone) == Dates.monthKey(prev) }
        note(NotificationType.WELCOME, texts.welcome(profile.firstName), HISTORY_DAYS * 86_400_000L, true)
        note(NotificationType.GOAL_REACHED, texts.goalReached(g3.name), 35 * 86_400_000L, true, g3.id)
        note(NotificationType.MONTHLY_SUMMARY, texts.monthlySummary(Labels.month(prev.year, prev.monthValue, language),
            Money.sum(prevTx.filter { it.kind.isIncome && it.direction == Direction.CREDIT }.map { it.amount }),
            Money.sum(prevTx.filter { it.kind.isSpending && it.direction == Direction.DEBIT }.map { it.amount })), (today.dayOfMonth - 1) * 86_400_000L + 3_600_000L, false)
        note(NotificationType.TRANSFER_RECEIVED, texts.transferReceived(bens[2].name, Money.units(120)), 9 * 86_400_000L, false)
        txs.lastOrNull { it.kind == TxKind.CARD_PAYMENT && it.accountId == current.id }?.let { t ->
            note(NotificationType.PAYMENT, texts.payment(t.label, t.amount, physical.last4), now - t.timestamp, false, t.id)
        }
        note(NotificationType.CARD_CREATED, texts.cardCreated(virtual.label, virtual.last4), 120 * 86_400_000L, true, virtual.id)
        notes.sortByDescending { it.timestamp }

        return BankState(
            profile = profile, accounts = accounts, cards = cards, transactions = txs, beneficiaries = bens,
            scheduled = scheduled, budgets = budgets, goals = listOf(goalAuto, g2, g3), goalMovements = m1 + m2 + m3,
            holdings = holdings, trades = trades, notifications = notes,
            meta = Meta(lastSummaryMonth = Dates.monthKey(today), sequence = txs.size.toLong()),
        ).canonical()
    }

    /** Profil personnel vierge : un compte courant crédité d'un capital virtuel de départ. */
    fun blank(clock: BankClock, ids: IdGenerator, firstName: String, lastName: String, language: AppLanguage, startingCapital: Money): BankState {
        val fr = language == AppLanguage.FR
        val now = clock.now()
        val profile = Profile(firstName.trim(), lastName.trim(), clientNumber = FictionalIdentifiers.clientNumber(ids), createdAt = now,
            preferences = Preferences(language = language))
        val current = Account(ids.id("acc"), AccountType.CURRENT, if (fr) "Compte courant" else "Current account", 0, "wallet",
            startingCapital, FictionalIdentifiers.accountReference(ids), now, overdraft = Money.ZERO, isPrimary = true)
        val savings = Account(ids.id("acc"), AccountType.SAVINGS, if (fr) "Épargne" else "Savings", 1, "savings", Money.ZERO,
            FictionalIdentifiers.accountReference(ids), now, interestBp = 250, sortOrder = 1)
        val txs = if (startingCapital.isPositive) listOf(Transaction(ids.id("tx"), current.id, startingCapital, Direction.CREDIT, Category.INCOME,
            TxKind.INCOME, if (fr) "Capital virtuel de départ" else "Virtual starting capital", timestamp = now,
            balanceAfter = startingCapital, reference = FictionalIdentifiers.receiptReference(ids))) else emptyList()
        val exp = clock.today().plusYears(4)
        val card = Card(ids.id("card"), current.id, CardType.VIRTUAL, if (fr) "Carte virtuelle" else "Virtual card",
            profile.fullName.uppercase(), FictionalIdentifiers.cardNumber(ids), exp.monthValue, exp.year, FictionalIdentifiers.cvv(ids),
            CardDesign.ELECTRIC, monthlyLimit = Money.units(1500), createdAt = now, contactless = false)
        val welcome = NotificationTexts(profile.preferences).welcome(profile.firstName)
        return BankState(
            profile = profile, accounts = listOf(current, savings), cards = listOf(card), transactions = txs,
            notifications = listOf(AppNotification(ids.id("ntf"), NotificationType.WELCOME, welcome.first, welcome.second, now)),
            meta = Meta(lastSummaryMonth = Dates.monthKey(clock.today())),
        )
    }
}
