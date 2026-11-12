package com.superessence.essabank.core.engine

import com.superessence.essabank.core.market.MarketSimulator
import com.superessence.essabank.core.model.Account
import com.superessence.essabank.core.model.AccountType
import com.superessence.essabank.core.model.AppNotification
import com.superessence.essabank.core.model.AutoSave
import com.superessence.essabank.core.model.BankState
import com.superessence.essabank.core.model.Beneficiary
import com.superessence.essabank.core.model.Card
import com.superessence.essabank.core.model.CardDesign
import com.superessence.essabank.core.model.CardStatus
import com.superessence.essabank.core.model.CardType
import com.superessence.essabank.core.model.Category
import com.superessence.essabank.core.model.Direction
import com.superessence.essabank.core.model.Frequency
import com.superessence.essabank.core.model.GoalKind
import com.superessence.essabank.core.model.GoalMoveType
import com.superessence.essabank.core.model.GoalMovement
import com.superessence.essabank.core.model.Holding
import com.superessence.essabank.core.model.LoanSimulation
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
import com.superessence.essabank.core.model.TxStatus
import com.superessence.essabank.core.money.Money
import com.superessence.essabank.core.money.sumOfMoney
import java.math.BigDecimal
import java.math.RoundingMode
import java.time.LocalDate

/**
 * Moteur de simulation financière ESSABANK.
 *
 * Toutes les opérations sont des fonctions pures : elles reçoivent un [BankState] et
 * renvoient soit un nouvel état complet ([EngineResult.Ok]), soit une erreur
 * ([EngineResult.Err]) sans aucune modification. Une opération est donc atomique par
 * construction ; la couche de persistance enregistre le nouvel état dans une seule
 * transaction SQLite.
 *
 * Règles garanties :
 * - un débit diminue le solde, un crédit l'augmente ;
 * - un virement interne débite un compte et crédite l'autre du même montant ;
 * - les montants nuls, négatifs ou hors plafond sont refusés ;
 * - une même requête (requestId) n'est jamais exécutée deux fois ;
 * - chaque écriture conserve le solde après opération ;
 * - les montants sont des centièmes entiers, arrondis au centième (arrondi bancaire).
 */
class BankEngine(
    val clock: BankClock = BankClock.SYSTEM,
    val ids: IdGenerator = IdGenerator.SECURE,
    val market: MarketSimulator = MarketSimulator(clock.zone),
) {

    // ================================================================ Comptes

    fun createAccount(s: BankState, type: AccountType, name: String, colorIndex: Int, icon: String): EngineResult<Account> {
        val clean = cleanName(name) ?: return err(BankError.InvalidName)
        if (s.accounts.size >= s.profile.tier.maxAccounts) return err(BankError.LimitReached)
        val acc = Account(
            id = ids.id("acc"), type = type, name = clean, colorIndex = colorIndex, icon = icon,
            balance = Money.ZERO, reference = FictionalIdentifiers.accountReference(ids), createdAt = clock.now(),
            overdraft = Money.ZERO, interestBp = if (type == AccountType.SAVINGS) 250 else 0,
            isPrimary = s.accounts.isEmpty(), sortOrder = (s.accounts.maxOfOrNull { it.sortOrder } ?: -1) + 1,
        )
        return ok(s.copy(accounts = s.accounts + acc), acc)
    }

    fun updateAccount(
        s: BankState, id: String, name: String? = null, colorIndex: Int? = null, icon: String? = null,
        includeInTotal: Boolean? = null, roundUp: Boolean? = null,
    ): EngineResult<Account> {
        val acc = s.account(id) ?: return err(BankError.AccountNotFound)
        val newName = if (name != null) cleanName(name) ?: return err(BankError.InvalidName) else acc.name
        val updated = acc.copy(
            name = newName, colorIndex = colorIndex ?: acc.colorIndex, icon = icon ?: acc.icon,
            includeInTotal = includeInTotal ?: acc.includeInTotal, roundUp = roundUp ?: acc.roundUp,
        )
        return ok(s.replaceAccount(updated), updated)
    }

    /** Supprime un compte vide. Ses cartes et virements programmés sont supprimés avec lui. */
    fun deleteAccount(s: BankState, id: String): EngineResult<Unit> {
        val acc = s.account(id) ?: return err(BankError.AccountNotFound)
        if (acc.isPrimary) return err(BankError.PrimaryAccount)
        if (s.accounts.size <= 1) return err(BankError.LastAccount)
        if (!acc.balance.isZero) return err(BankError.AccountNotEmpty)
        if (acc.type == AccountType.INVESTMENT && s.holdings.isNotEmpty() && s.accounts.count { it.type == AccountType.INVESTMENT } == 1)
            return err(BankError.AccountNotEmpty)
        val ns = s.copy(
            accounts = s.accounts.filterNot { it.id == id },
            cards = s.cards.filterNot { it.accountId == id },
            scheduled = s.scheduled.filterNot { it.fromAccountId == id || it.toAccountId == id },
            transactions = s.transactions.filterNot { it.accountId == id },
            goals = s.goals.map { g -> if (g.autoSave?.fromAccountId == id) g.copy(autoSave = null) else g },
        )
        return ok(ns, Unit)
    }

    fun reorderAccounts(s: BankState, orderedIds: List<String>): EngineResult<Unit> {
        val pos = orderedIds.withIndex().associate { it.value to it.index }
        return ok(s.copy(accounts = s.accounts.map { it.copy(sortOrder = pos[it.id] ?: it.sortOrder) }), Unit)
    }

    // ================================================================ Virements

    /** Virement entre deux comptes de l'utilisateur. Deux écritures liées, solde global inchangé. */
    fun transferInternal(
        s: BankState, fromId: String, toId: String, amount: Money, label: String = "", note: String = "", requestId: String? = null,
    ): EngineResult<List<Transaction>> {
        checkRequest(s, requestId)?.let { return err(it) }
        checkAmount(amount)?.let { return err(it) }
        if (fromId == toId) return err(BankError.SameAccount)
        val from = s.account(fromId) ?: return err(BankError.AccountNotFound)
        val to = s.account(toId) ?: return err(BankError.AccountNotFound)
        if (from.available < amount) return err(BankError.InsufficientFunds(from.available.coerceNonNegative()))
        val t = texts(s)
        val fr = isFr(s)
        val transferId = ids.id("trf")
        val ref = FictionalIdentifiers.receiptReference(ids)
        val now = clock.now()
        val lbl = label.trim().ifEmpty { if (fr) "Virement interne" else "Internal transfer" }
        var st = s
        val (s1, debit) = post(st, from.id, Direction.DEBIT, amount, Category.TRANSFERS, TxKind.INTERNAL_TRANSFER, lbl, note,
            counterparty = to.name, transferId = transferId, reference = ref, timestamp = now)
        st = s1
        val (s2, credit) = post(st, to.id, Direction.CREDIT, amount, Category.TRANSFERS, TxKind.INTERNAL_TRANSFER, lbl, note,
            counterparty = from.name, transferId = transferId, reference = ref, timestamp = now)
        st = s2
        val (title, body) = t.internal(from.name, to.name, amount)
        st = notify(st, NotificationType.NEW_TRANSACTION, title, body, debit.id)
        st = markRequest(st, requestId)
        return ok(st, listOf(debit, credit), listOf(BankEvent.TransactionsCreated(listOf(debit.id, credit.id))))
    }

    /** Virement simulé vers un bénéficiaire fictif : seul le compte de l'utilisateur est débité. */
    fun sendToBeneficiary(
        s: BankState, fromId: String, beneficiaryId: String, amount: Money, motif: String, note: String = "", requestId: String? = null,
    ): EngineResult<Transaction> {
        checkRequest(s, requestId)?.let { return err(it) }
        checkAmount(amount)?.let { return err(it) }
        val from = s.account(fromId) ?: return err(BankError.AccountNotFound)
        val ben = s.beneficiary(beneficiaryId) ?: return err(BankError.BeneficiaryNotFound)
        if (from.available < amount) return err(BankError.InsufficientFunds(from.available.coerceNonNegative()))
        val fr = isFr(s)
        val lbl = motif.trim().ifEmpty { if (fr) "Virement à ${ben.name}" else "Transfer to ${ben.name}" }
        var (st, tx) = post(s, from.id, Direction.DEBIT, amount, Category.TRANSFERS, TxKind.TRANSFER_OUT, lbl, note,
            counterparty = ben.name, reference = FictionalIdentifiers.receiptReference(ids))
        val (title, body) = texts(s).transferSent(ben.name, amount)
        st = notify(st, NotificationType.TRANSFER_SENT, title, body, tx.id)
        val (st2, events) = checkBudgets(st)
        return ok(markRequest(st2, requestId), tx, listOf(BankEvent.TransactionsCreated(listOf(tx.id))) + events)
    }

    /** Simule la réception d'un virement fictif (aucune somme réelle n'est reçue). */
    fun receiveSimulated(
        s: BankState, toId: String, senderName: String, amount: Money, motif: String = "", requestId: String? = null,
    ): EngineResult<Transaction> {
        checkRequest(s, requestId)?.let { return err(it) }
        checkAmount(amount)?.let { return err(it) }
        val to = s.account(toId) ?: return err(BankError.AccountNotFound)
        val sender = cleanName(senderName) ?: return err(BankError.InvalidName)
        val fr = isFr(s)
        val lbl = motif.trim().ifEmpty { if (fr) "Virement reçu de $sender" else "Transfer from $sender" }
        var (st, tx) = post(s, to.id, Direction.CREDIT, amount, Category.TRANSFERS, TxKind.TRANSFER_IN, lbl, "",
            counterparty = sender, reference = FictionalIdentifiers.receiptReference(ids))
        val (title, body) = texts(s).transferReceived(sender, amount)
        st = notify(st, NotificationType.TRANSFER_RECEIVED, title, body, tx.id)
        return ok(markRequest(st, requestId), tx, listOf(BankEvent.TransactionsCreated(listOf(tx.id))))
    }

    /** Revenu fictif (salaire de démonstration, prime…). */
    fun addIncome(s: BankState, toId: String, label: String, amount: Money, requestId: String? = null): EngineResult<Transaction> {
        checkRequest(s, requestId)?.let { return err(it) }
        checkAmount(amount)?.let { return err(it) }
        s.account(toId) ?: return err(BankError.AccountNotFound)
        val lbl = cleanName(label) ?: return err(BankError.InvalidName)
        val (st, tx) = post(s, toId, Direction.CREDIT, amount, Category.INCOME, TxKind.INCOME, lbl, "",
            reference = FictionalIdentifiers.receiptReference(ids))
        return ok(markRequest(st, requestId), tx, listOf(BankEvent.TransactionsCreated(listOf(tx.id))))
    }

    // ================================================================ Bénéficiaires

    fun addBeneficiary(s: BankState, name: String, reference: String? = null, note: String = "", colorIndex: Int = 0): EngineResult<Beneficiary> {
        val clean = cleanName(name) ?: return err(BankError.InvalidName)
        val ref = reference?.trim()?.uppercase()?.takeIf { it.isNotEmpty() }
        if (ref != null && !FictionalIdentifiers.isValidBeneficiaryReference(ref)) return err(BankError.InvalidBeneficiary)
        if (ref != null && s.beneficiaries.any { it.reference == ref }) return err(BankError.DuplicateRequest)
        val b = Beneficiary(
            id = ids.id("ben"), name = clean, reference = ref ?: FictionalIdentifiers.beneficiaryReference(ids),
            note = note.trim().take(120), colorIndex = colorIndex, createdAt = clock.now(),
        )
        return ok(s.copy(beneficiaries = s.beneficiaries + b), b)
    }

    fun updateBeneficiary(s: BankState, id: String, name: String? = null, note: String? = null, favorite: Boolean? = null): EngineResult<Beneficiary> {
        val b = s.beneficiary(id) ?: return err(BankError.BeneficiaryNotFound)
        val newName = if (name != null) cleanName(name) ?: return err(BankError.InvalidName) else b.name
        val u = b.copy(name = newName, note = note?.trim()?.take(120) ?: b.note, favorite = favorite ?: b.favorite)
        return ok(s.copy(beneficiaries = s.beneficiaries.map { if (it.id == id) u else it }), u)
    }

    fun deleteBeneficiary(s: BankState, id: String): EngineResult<Unit> {
        s.beneficiary(id) ?: return err(BankError.BeneficiaryNotFound)
        return ok(s.copy(
            beneficiaries = s.beneficiaries.filterNot { it.id == id },
            scheduled = s.scheduled.filterNot { it.beneficiaryId == id },
        ), Unit)
    }

    // ================================================================ Virements programmés

    fun scheduleTransfer(
        s: BankState, fromId: String, toAccountId: String?, beneficiaryId: String?, amount: Money, label: String, note: String,
        firstDate: LocalDate, frequency: Frequency, endDate: LocalDate? = null, requestId: String? = null,
    ): EngineResult<ScheduledTransfer> {
        checkRequest(s, requestId)?.let { return err(it) }
        checkAmount(amount)?.let { return err(it) }
        s.account(fromId) ?: return err(BankError.AccountNotFound)
        if ((toAccountId == null) == (beneficiaryId == null)) return err(BankError.AccountNotFound)
        if (toAccountId != null) {
            s.account(toAccountId) ?: return err(BankError.AccountNotFound)
            if (toAccountId == fromId) return err(BankError.SameAccount)
        }
        val ben = beneficiaryId?.let { s.beneficiary(it) ?: return err(BankError.BeneficiaryNotFound) }
        val today = clock.today()
        if (firstDate.isBefore(today)) return err(BankError.InvalidDate)
        if (endDate != null && endDate.isBefore(firstDate)) return err(BankError.InvalidDate)
        val fr = isFr(s)
        val lbl = label.trim().ifEmpty {
            ben?.let { if (fr) "Virement à ${it.name}" else "Transfer to ${it.name}" } ?: (if (fr) "Virement programmé" else "Scheduled transfer")
        }
        val sch = ScheduledTransfer(
            id = ids.id("sch"), fromAccountId = fromId, toAccountId = toAccountId, beneficiaryId = beneficiaryId,
            amount = amount, label = lbl, note = note.trim(), nextDate = firstDate.toEpochDay(), frequency = frequency,
            endDate = endDate?.toEpochDay(), createdAt = clock.now(),
        )
        return ok(markRequest(s.copy(scheduled = s.scheduled + sch), requestId), sch)
    }

    fun setScheduledActive(s: BankState, id: String, active: Boolean): EngineResult<Unit> {
        s.scheduled.firstOrNull { it.id == id } ?: return err(BankError.ScheduledNotFound)
        return ok(s.copy(scheduled = s.scheduled.map { if (it.id == id) it.copy(active = active) else it }), Unit)
    }

    fun cancelScheduled(s: BankState, id: String): EngineResult<Unit> {
        s.scheduled.firstOrNull { it.id == id } ?: return err(BankError.ScheduledNotFound)
        return ok(s.copy(scheduled = s.scheduled.filterNot { it.id == id }), Unit)
    }

    /**
     * Exécute tout ce qui est arrivé à échéance : virements programmés, épargne automatique,
     * intérêts fictifs mensuels, résumé mensuel. À appeler au démarrage et au retour dans l'application.
     */
    fun processDue(s: BankState): EngineResult<Int> {
        var st = s
        var executed = 0
        val today = clock.today().toEpochDay()
        val t = texts(s)
        val events = mutableListOf<BankEvent>()

        // Virements programmés (avec rattrapage limité à 24 échéances par passage).
        for (sch0 in s.scheduled) {
            var sch = sch0
            var guard = 0
            while (sch.active && sch.nextDate <= today && guard < 24) {
                guard++
                // Exécution horodatée à l'instant du traitement : l'historique reste chronologique.
                val execAt = clock.now()
                val result = if (sch.toAccountId != null)
                    transferInternalAt(st, sch.fromAccountId, sch.toAccountId, sch.amount, sch.label, sch.note, execAt)
                else
                    sendAt(st, sch.fromAccountId, sch.beneficiaryId!!, sch.amount, sch.label, sch.note, execAt)
                sch = if (result != null) {
                    st = result
                    executed++
                    val (ti, bo) = t.scheduledDone(sch.label, sch.amount)
                    st = notify(st, NotificationType.TRANSFER_SENT, ti, bo, sch.id)
                    sch.copy(executions = sch.executions + 1)
                } else {
                    val (ti, bo) = t.scheduledFailed(sch.label, sch.amount)
                    st = notify(st, NotificationType.SCHEDULED_FAILED, ti, bo, sch.id)
                    sch.copy(failures = sch.failures + 1)
                }
                val next = nextDate(sch.nextDate, sch.frequency)
                sch = when {
                    next == null -> sch.copy(active = false)
                    sch.endDate != null && next > sch.endDate -> sch.copy(active = false, nextDate = next)
                    else -> sch.copy(nextDate = next)
                }
            }
            st = st.copy(scheduled = st.scheduled.map { if (it.id == sch.id) sch else it })
        }

        // Épargne automatique des coffres.
        for (g0 in s.goals) {
            var goal = st.goal(g0.id) ?: continue
            var guard = 0
            while (true) {
                val auto = goal.autoSave ?: break
                if (!auto.active || auto.nextDate > today || guard >= 24) break
                guard++
                val r = depositToGoal(st, goal.id, auto.fromAccountId, auto.amount, null, automatic = true)
                if (r is EngineResult.Ok) {
                    st = r.state; executed++
                    events += r.events
                    val (ti, bo) = t.autoSave(goal.name, auto.amount)
                    st = notify(st, NotificationType.NEW_TRANSACTION, ti, bo, goal.id)
                }
                goal = st.goal(goal.id) ?: break
                val next = nextDate(auto.nextDate, auto.frequency)
                goal = goal.copy(autoSave = if (next == null) auto.copy(active = false) else auto.copy(nextDate = next))
                st = st.copy(goals = st.goals.map { if (it.id == goal.id) goal else it })
            }
        }

        // Changement de mois : intérêts fictifs et résumé du mois écoulé.
        val currentMonth = Dates.monthKey(clock.today())
        val last = st.meta.lastSummaryMonth
        if (last == null) {
            st = st.copy(meta = st.meta.copy(lastSummaryMonth = currentMonth))
        } else if (last != currentMonth) {
            val prev = clock.today().minusMonths(1)
            val prevKey = Dates.monthKey(prev)
            // Intérêts fictifs mensuels sur les comptes d'épargne.
            for (acc in st.accounts.filter { it.interestBp > 0 && it.balance.isPositive }) {
                val interest = acc.balance.times(BigDecimal(acc.interestBp).divide(BigDecimal(120_000), 10, RoundingMode.HALF_EVEN))
                if (interest.isPositive) {
                    val (s2, _) = post(st, acc.id, Direction.CREDIT, interest, Category.INCOME, TxKind.INTEREST,
                        if (isFr(s)) "Intérêts fictifs" else "Fictional interest", "", reference = FictionalIdentifiers.receiptReference(ids))
                    st = s2
                    val (ti, bo) = t.interest(acc.name, interest)
                    st = notify(st, NotificationType.NEW_TRANSACTION, ti, bo, acc.id)
                }
            }
            val zone = clock.zone
            val monthTx = st.transactions.filter { Dates.monthKey(it.timestamp, zone) == prevKey && it.status == TxStatus.COMPLETED }
            val income = monthTx.filter { it.kind.isIncome && it.direction == Direction.CREDIT }.sumOfMoney { it.amount }
            val spent = monthTx.filter { it.kind.isSpending && it.direction == Direction.DEBIT }.sumOfMoney { it.amount }
            val (ti, bo) = t.monthlySummary(Labels.month(prev.year, prev.monthValue, s.profile.preferences.language), income, spent)
            st = notify(st, NotificationType.MONTHLY_SUMMARY, ti, bo, prevKey)
            st = st.copy(meta = st.meta.copy(lastSummaryMonth = currentMonth))
        }

        // Cartes expirées : désactivation automatique.
        val now = clock.today()
        st = st.copy(cards = st.cards.map { c ->
            if (c.status == CardStatus.ACTIVE && isExpired(c, now)) c.copy(status = CardStatus.INACTIVE) else c
        })
        return EngineResult.Ok(st, executed, events)
    }

    private fun transferInternalAt(s: BankState, fromId: String, toId: String, amount: Money, label: String, note: String, at: Long): BankState? {
        val from = s.account(fromId) ?: return null
        val to = s.account(toId) ?: return null
        if (from.available < amount) return null
        val transferId = ids.id("trf"); val ref = FictionalIdentifiers.receiptReference(ids)
        val (s1, _) = post(s, fromId, Direction.DEBIT, amount, Category.TRANSFERS, TxKind.INTERNAL_TRANSFER, label, note,
            counterparty = to.name, transferId = transferId, reference = ref, timestamp = at)
        val (s2, _) = post(s1, toId, Direction.CREDIT, amount, Category.TRANSFERS, TxKind.INTERNAL_TRANSFER, label, note,
            counterparty = from.name, transferId = transferId, reference = ref, timestamp = at)
        return s2
    }

    private fun sendAt(s: BankState, fromId: String, benId: String, amount: Money, label: String, note: String, at: Long): BankState? {
        val from = s.account(fromId) ?: return null
        val ben = s.beneficiary(benId) ?: return null
        if (from.available < amount) return null
        val (s1, _) = post(s, fromId, Direction.DEBIT, amount, Category.TRANSFERS, TxKind.TRANSFER_OUT, label, note,
            counterparty = ben.name, reference = FictionalIdentifiers.receiptReference(ids), timestamp = at)
        return checkBudgets(s1).first
    }

    private fun nextDate(epochDay: Long, f: Frequency): Long? = when (f) {
        Frequency.ONCE -> null
        Frequency.WEEKLY -> epochDay + 7
        Frequency.MONTHLY -> LocalDate.ofEpochDay(epochDay).plusMonths(1).toEpochDay()
    }

    // ================================================================ Cartes

    fun createCard(
        s: BankState, accountId: String, type: CardType, label: String, design: CardDesign, monthlyLimit: Money,
    ): EngineResult<Card> {
        val acc = s.account(accountId) ?: return err(BankError.AccountNotFound)
        val tier = s.profile.tier
        if (s.cards.size >= tier.maxCards) return err(BankError.LimitReached)
        if (design.premium && !tier.premiumDesigns) return err(BankError.PremiumRequired)
        if (type == CardType.PREMIUM && tier == Tier.STANDARD) return err(BankError.PremiumRequired)
        checkLimit(monthlyLimit)?.let { return err(it) }
        val clean = cleanName(label) ?: return err(BankError.InvalidName)
        val today = clock.today()
        val exp = if (type == CardType.EPHEMERAL) today.plusMonths(1) else today.plusYears(4)
        val card = Card(
            id = ids.id("card"), accountId = acc.id, type = type, label = clean,
            holderName = s.profile.fullName.uppercase().ifBlank { "TITULAIRE FICTIF" },
            number = FictionalIdentifiers.cardNumber(ids), expMonth = exp.monthValue, expYear = exp.year,
            cvv = FictionalIdentifiers.cvv(ids), design = design, monthlyLimit = monthlyLimit, createdAt = clock.now(),
            usesLeft = if (type == CardType.EPHEMERAL) 1 else null,
            onlinePayments = true, contactless = type != CardType.VIRTUAL && type != CardType.EPHEMERAL,
        )
        val (ti, bo) = texts(s).cardCreated(clean, card.last4)
        val st = notify(s.copy(cards = s.cards + card), NotificationType.CARD_CREATED, ti, bo, card.id)
        return ok(st, card)
    }

    fun setCardStatus(s: BankState, id: String, status: CardStatus): EngineResult<Card> {
        val c = s.card(id) ?: return err(BankError.CardNotFound)
        if (status == CardStatus.ACTIVE && isExpired(c, clock.today())) return err(BankError.CardExpired)
        if (status == CardStatus.ACTIVE && c.usesLeft == 0) return err(BankError.CardExpired)
        val u = c.copy(status = status)
        val t = texts(s)
        val (ti, bo) = t.cardStatus(c.label, t.cardStatusLabel(status))
        val st = notify(s.replaceCard(u), NotificationType.CARD_UPDATED, ti, bo, id)
        return ok(st, u)
    }

    fun updateCard(
        s: BankState, id: String, label: String? = null, design: CardDesign? = null, monthlyLimit: Money? = null,
        onlinePayments: Boolean? = null, contactless: Boolean? = null,
    ): EngineResult<Card> {
        val c = s.card(id) ?: return err(BankError.CardNotFound)
        if (design != null && design.premium && !s.profile.tier.premiumDesigns) return err(BankError.PremiumRequired)
        if (monthlyLimit != null) checkLimit(monthlyLimit)?.let { return err(it) }
        val newLabel = if (label != null) cleanName(label) ?: return err(BankError.InvalidName) else c.label
        val u = c.copy(
            label = newLabel, design = design ?: c.design, monthlyLimit = monthlyLimit ?: c.monthlyLimit,
            onlinePayments = onlinePayments ?: c.onlinePayments, contactless = contactless ?: c.contactless,
        )
        return ok(s.replaceCard(u), u)
    }

    fun deleteCard(s: BankState, id: String): EngineResult<Unit> {
        s.card(id) ?: return err(BankError.CardNotFound)
        return ok(s.copy(cards = s.cards.filterNot { it.id == id }), Unit)
    }

    /** Dépenses d'une carte sur le mois civil en cours. */
    fun cardSpentThisMonth(s: BankState, cardId: String): Money {
        val key = Dates.monthKey(clock.today())
        return s.transactions.filter {
            it.cardId == cardId && it.direction == Direction.DEBIT && it.status == TxStatus.COMPLETED &&
                Dates.monthKey(it.timestamp, clock.zone) == key
        }.sumOfMoney { it.amount }
    }

    /**
     * Paiement simulé par carte chez un commerçant fictif. Aucun réseau de paiement n'est contacté.
     * Si l'arrondi est activé sur le compte, la différence à l'unité supérieure est versée dans le premier coffre actif.
     */
    fun cardPayment(
        s: BankState, cardId: String, merchant: String, category: Category, amount: Money, online: Boolean = false,
        requestId: String? = null,
    ): EngineResult<Transaction> {
        checkRequest(s, requestId)?.let { return err(it) }
        checkAmount(amount)?.let { return err(it) }
        val card = s.card(cardId) ?: return err(BankError.CardNotFound)
        when (card.status) {
            CardStatus.LOCKED -> return err(BankError.CardLocked)
            CardStatus.INACTIVE -> return err(BankError.CardInactive)
            CardStatus.ACTIVE -> Unit
        }
        if (isExpired(card, clock.today())) return err(BankError.CardExpired)
        if (online && !card.onlinePayments) return err(BankError.OnlineDisabled)
        val remaining = card.monthlyLimit - cardSpentThisMonth(s, cardId)
        if (amount > remaining) return err(BankError.CardLimitExceeded(remaining.coerceNonNegative()))
        val acc = s.account(card.accountId) ?: return err(BankError.AccountNotFound)
        if (acc.available < amount) return err(BankError.InsufficientFunds(acc.available.coerceNonNegative()))
        val name = cleanName(merchant) ?: return err(BankError.InvalidName)

        var (st, tx) = post(s, acc.id, Direction.DEBIT, amount, category, TxKind.CARD_PAYMENT, name, "",
            cardId = card.id, counterparty = name, reference = FictionalIdentifiers.receiptReference(ids))
        val events = mutableListOf<BankEvent>(BankEvent.TransactionsCreated(listOf(tx.id)))
        // Carte éphémère : un seul usage.
        if (card.usesLeft != null) {
            val left = card.usesLeft - 1
            st = st.replaceCard(card.copy(usesLeft = left, status = if (left <= 0) CardStatus.INACTIVE else card.status))
        }
        val (ti, bo) = texts(s).payment(name, amount, card.last4)
        st = notify(st, NotificationType.PAYMENT, ti, bo, tx.id)
        // Arrondi automatique vers l'épargne.
        if (acc.roundUp) {
            val rest = amount.cents % 100
            if (rest != 0L) {
                val round = Money(100 - rest)
                val goal = st.goals.sortedBy { it.sortOrder }.firstOrNull { !it.isReached }
                if (goal != null) {
                    val r = depositToGoal(st, goal.id, acc.id, round, null, automatic = true)
                    if (r is EngineResult.Ok) { st = r.state; events += r.events }
                }
            }
        }
        val (st2, budgetEvents) = checkBudgets(st)
        return ok(markRequest(st2, requestId), tx, events + budgetEvents)
    }

    /** Une carte est valable jusqu'au dernier jour de son mois d'expiration. */
    private fun isExpired(c: Card, today: LocalDate): Boolean =
        !today.isBefore(LocalDate.of(c.expYear, c.expMonth, 1).plusMonths(1))

    // ================================================================ Épargne

    fun createGoal(s: BankState, name: String, kind: GoalKind, target: Money, targetDate: LocalDate?, colorIndex: Int): EngineResult<SavingsGoal> {
        val clean = cleanName(name) ?: return err(BankError.InvalidName)
        checkAmount(target)?.let { return err(it) }
        if (s.goals.size >= s.profile.tier.maxGoals) return err(BankError.LimitReached)
        if (targetDate != null && !targetDate.isAfter(clock.today())) return err(BankError.InvalidDate)
        val g = SavingsGoal(
            id = ids.id("goal"), name = clean, kind = kind, target = target, targetDate = targetDate?.toEpochDay(),
            colorIndex = colorIndex, createdAt = clock.now(), sortOrder = (s.goals.maxOfOrNull { it.sortOrder } ?: -1) + 1,
        )
        return ok(s.copy(goals = s.goals + g), g)
    }

    fun updateGoal(
        s: BankState, id: String, name: String? = null, target: Money? = null, targetDate: LocalDate? = null,
        clearDate: Boolean = false, colorIndex: Int? = null, kind: GoalKind? = null,
    ): EngineResult<SavingsGoal> {
        val g = s.goal(id) ?: return err(BankError.GoalNotFound)
        val newName = if (name != null) cleanName(name) ?: return err(BankError.InvalidName) else g.name
        if (target != null) checkAmount(target)?.let { return err(it) }
        if (targetDate != null && !targetDate.isAfter(clock.today())) return err(BankError.InvalidDate)
        var u = g.copy(
            name = newName, target = target ?: g.target, colorIndex = colorIndex ?: g.colorIndex, kind = kind ?: g.kind,
            targetDate = if (clearDate) null else targetDate?.toEpochDay() ?: g.targetDate,
        )
        u = u.copy(completedAt = if (u.isReached) (g.completedAt ?: clock.now()) else null)
        return ok(s.copy(goals = s.goals.map { if (it.id == id) u else it }), u)
    }

    /** Supprime un coffre ; l'épargne restante est reversée sur le compte indiqué. */
    fun deleteGoal(s: BankState, id: String, refundAccountId: String): EngineResult<Unit> {
        val g = s.goal(id) ?: return err(BankError.GoalNotFound)
        var st = s
        if (g.saved.isPositive) {
            val r = withdrawFromGoal(st, id, refundAccountId, g.saved, null)
            if (r is EngineResult.Err) return r
            st = (r as EngineResult.Ok).state
        }
        return ok(st.copy(goals = st.goals.filterNot { it.id == id }, goalMovements = st.goalMovements.filterNot { it.goalId == id }), Unit)
    }

    fun depositToGoal(s: BankState, goalId: String, fromId: String, amount: Money, requestId: String?, automatic: Boolean = false): EngineResult<SavingsGoal> {
        checkRequest(s, requestId)?.let { return err(it) }
        checkAmount(amount)?.let { return err(it) }
        val g = s.goal(goalId) ?: return err(BankError.GoalNotFound)
        val acc = s.account(fromId) ?: return err(BankError.AccountNotFound)
        if (acc.balance < amount) return err(BankError.InsufficientFunds(acc.balance.coerceNonNegative()))
        val fr = isFr(s)
        var (st, tx) = post(s, acc.id, Direction.DEBIT, amount, Category.TRANSFERS, TxKind.SAVINGS_DEPOSIT,
            if (automatic) (if (fr) "Épargne automatique · ${g.name}" else "Auto-save · ${g.name}") else (if (fr) "Coffre · ${g.name}" else "Vault · ${g.name}"),
            "", counterparty = g.name, reference = FictionalIdentifiers.receiptReference(ids))
        val wasReached = g.isReached
        var u = g.copy(saved = g.saved + amount)
        val events = mutableListOf<BankEvent>(BankEvent.TransactionsCreated(listOf(tx.id)))
        if (u.isReached && !wasReached) {
            u = u.copy(completedAt = clock.now())
            val (ti, bo) = texts(s).goalReached(g.name)
            st = notify(st, NotificationType.GOAL_REACHED, ti, bo, g.id)
            events += BankEvent.GoalReached(g.id)
        }
        val mv = GoalMovement(ids.id("gmv"), g.id, acc.id, amount, GoalMoveType.DEPOSIT, clock.now(), automatic)
        st = st.copy(goals = st.goals.map { if (it.id == goalId) u else it }, goalMovements = st.goalMovements + mv)
        return ok(markRequest(st, requestId), u, events)
    }

    fun withdrawFromGoal(s: BankState, goalId: String, toId: String, amount: Money, requestId: String?): EngineResult<SavingsGoal> {
        checkRequest(s, requestId)?.let { return err(it) }
        checkAmount(amount)?.let { return err(it) }
        val g = s.goal(goalId) ?: return err(BankError.GoalNotFound)
        s.account(toId) ?: return err(BankError.AccountNotFound)
        if (g.saved < amount) return err(BankError.GoalInsufficient)
        val fr = isFr(s)
        var (st, tx) = post(s, toId, Direction.CREDIT, amount, Category.TRANSFERS, TxKind.SAVINGS_WITHDRAWAL,
            if (fr) "Retrait coffre · ${g.name}" else "Vault withdrawal · ${g.name}", "", counterparty = g.name,
            reference = FictionalIdentifiers.receiptReference(ids))
        var u = g.copy(saved = g.saved - amount)
        if (!u.isReached) u = u.copy(completedAt = null)
        val mv = GoalMovement(ids.id("gmv"), g.id, toId, amount, GoalMoveType.WITHDRAWAL, clock.now())
        st = st.copy(goals = st.goals.map { if (it.id == goalId) u else it }, goalMovements = st.goalMovements + mv)
        return ok(markRequest(st, requestId), u, listOf(BankEvent.TransactionsCreated(listOf(tx.id))))
    }

    fun setAutoSave(s: BankState, goalId: String, amount: Money?, frequency: Frequency, fromId: String, firstDate: LocalDate): EngineResult<SavingsGoal> {
        val g = s.goal(goalId) ?: return err(BankError.GoalNotFound)
        val u = if (amount == null) g.copy(autoSave = null) else {
            checkAmount(amount)?.let { return err(it) }
            s.account(fromId) ?: return err(BankError.AccountNotFound)
            if (firstDate.isBefore(clock.today())) return err(BankError.InvalidDate)
            if (frequency == Frequency.ONCE) return err(BankError.InvalidDate)
            g.copy(autoSave = AutoSave(amount, frequency, fromId, firstDate.toEpochDay()))
        }
        return ok(s.copy(goals = s.goals.map { if (it.id == goalId) u else it }), u)
    }

    // ================================================================ Budgets

    fun setMonthlyBudget(s: BankState, amount: Money?): EngineResult<Unit> {
        if (amount != null) checkAmount(amount)?.let { return err(it) }
        val prefix = Dates.monthKey(clock.today()) + ":ALL:"
        val b = s.budgets.copy(monthly = amount, alertsSent = s.budgets.alertsSent.filterNot { it.startsWith(prefix) }.toSet())
        return checkBudgets(s.copy(budgets = b)).let { (st, ev) -> ok(st, Unit, ev) }
    }

    fun setCategoryBudget(s: BankState, category: Category, amount: Money?): EngineResult<Unit> {
        if (amount != null) checkAmount(amount)?.let { return err(it) }
        val prefix = Dates.monthKey(clock.today()) + ":${category.name}:"
        val cats = if (amount == null) s.budgets.categories - category else s.budgets.categories + (category to amount)
        val b = s.budgets.copy(categories = cats, alertsSent = s.budgets.alertsSent.filterNot { it.startsWith(prefix) }.toSet())
        return checkBudgets(s.copy(budgets = b)).let { (st, ev) -> ok(st, Unit, ev) }
    }

    /** Dépenses du mois courant (paiements et virements sortants), globales et par catégorie. */
    fun monthSpending(s: BankState): Pair<Money, Map<Category, Money>> {
        val key = Dates.monthKey(clock.today())
        val tx = s.transactions.filter {
            it.kind.isSpending && it.direction == Direction.DEBIT && it.status == TxStatus.COMPLETED &&
                Dates.monthKey(it.timestamp, clock.zone) == key
        }
        val byCat = tx.groupBy { it.category }.mapValues { (_, l) -> l.sumOfMoney { it.amount } }
        return tx.sumOfMoney { it.amount } to byCat
    }

    /** Émet les alertes simulées de budget (80 % et 100 %), une seule fois par mois et par seuil. */
    private fun checkBudgets(s: BankState): Pair<BankState, List<BankEvent>> {
        val b = s.budgets
        if (b.monthly == null && b.categories.isEmpty()) return s to emptyList()
        val (total, byCat) = monthSpending(s)
        val month = Dates.monthKey(clock.today())
        val t = texts(s)
        var st = s
        val sent = b.alertsSent.toMutableSet()
        val events = mutableListOf<BankEvent>()
        fun evaluate(scopeKey: String, scopeName: String, spent: Money, budget: Money) {
            if (!budget.isPositive) return
            val pct = (spent.cents * 100 / budget.cents).toInt()
            val k100 = "$month:$scopeKey:100"; val k80 = "$month:$scopeKey:80"
            if (pct >= 100 && k100 !in sent) {
                sent += k100; sent += k80
                val (ti, bo) = t.budgetExceeded(scopeName, spent, budget)
                st = notify(st, NotificationType.BUDGET_EXCEEDED, ti, bo, scopeKey)
                events += BankEvent.BudgetAlert(k100)
            } else if (pct in 80..99 && k80 !in sent) {
                sent += k80
                val (ti, bo) = t.budgetWarning(scopeName, pct)
                st = notify(st, NotificationType.BUDGET_WARNING, ti, bo, scopeKey)
                events += BankEvent.BudgetAlert(k80)
            }
        }
        b.monthly?.let { evaluate("ALL", if (isFr(s)) "Budget mensuel" else "Monthly budget", total, it) }
        for ((cat, budget) in b.categories) evaluate(cat.name, t.categoryName(cat), byCat[cat] ?: Money.ZERO, budget)
        // On ne conserve que les clés des trois derniers mois.
        val keep = (0L..2L).map { Dates.monthKey(clock.today().minusMonths(it)) }.toSet()
        st = st.copy(budgets = st.budgets.copy(alertsSent = sent.filter { it.substringBefore(':') in keep }.toSet()))
        return st to events
    }

    // ================================================================ Investissements fictifs

    fun buy(s: BankState, symbol: String, amount: Money, accountId: String, requestId: String? = null): EngineResult<Trade> {
        checkRequest(s, requestId)?.let { return err(it) }
        checkAmount(amount)?.let { return err(it) }
        val asset = market.asset(symbol) ?: return err(BankError.AssetNotFound)
        val acc = s.account(accountId) ?: return err(BankError.AccountNotFound)
        if (acc.type != AccountType.INVESTMENT) return err(BankError.InvalidAccountType)
        val price = market.price(symbol, clock.now())
        val qty = MarketSimulator.quantityFor(amount, price)
        if (qty <= 0) return err(BankError.InvalidQuantity)
        val total = MarketSimulator.value(price, qty)
        if (acc.balance < total) return err(BankError.InsufficientFunds(acc.balance.coerceNonNegative()))
        val fr = isFr(s)
        var (st, _) = post(s, acc.id, Direction.DEBIT, total, Category.TRANSFERS, TxKind.INVEST_BUY,
            (if (fr) "Achat virtuel · " else "Virtual buy · ") + asset.name, "", counterparty = asset.symbol,
            reference = FictionalIdentifiers.receiptReference(ids))
        val h = st.holdings.firstOrNull { it.symbol == symbol }
        val nh = if (h == null) Holding(symbol, qty, total) else h.copy(quantityMicro = h.quantityMicro + qty, costBasis = h.costBasis + total)
        val trade = Trade(ids.id("trd"), symbol, TradeSide.BUY, qty, price, total, Money.ZERO, clock.now(), acc.id)
        st = st.copy(holdings = st.holdings.filterNot { it.symbol == symbol } + nh, trades = st.trades + trade)
        val (ti, bo) = texts(s).trade(true, asset.name, total)
        st = notify(st, NotificationType.INVESTMENT, ti, bo, trade.id)
        return ok(markRequest(st, requestId), trade)
    }

    fun sell(s: BankState, symbol: String, quantityMicro: Long, accountId: String, requestId: String? = null): EngineResult<Trade> {
        checkRequest(s, requestId)?.let { return err(it) }
        if (quantityMicro <= 0) return err(BankError.InvalidQuantity)
        val asset = market.asset(symbol) ?: return err(BankError.AssetNotFound)
        val acc = s.account(accountId) ?: return err(BankError.AccountNotFound)
        if (acc.type != AccountType.INVESTMENT) return err(BankError.InvalidAccountType)
        val h = s.holdings.firstOrNull { it.symbol == symbol } ?: return err(BankError.InsufficientQuantity)
        if (quantityMicro > h.quantityMicro) return err(BankError.InsufficientQuantity)
        val price = market.price(symbol, clock.now())
        val proceeds = MarketSimulator.value(price, quantityMicro)
        // Coût moyen pondéré de la part vendue.
        val costPart = if (quantityMicro == h.quantityMicro) h.costBasis else
            Money.of(h.costBasis.toBigDecimal().multiply(BigDecimal(quantityMicro)).divide(BigDecimal(h.quantityMicro), 10, RoundingMode.HALF_EVEN))
        val realized = proceeds - costPart
        val fr = isFr(s)
        var st = s
        if (proceeds.isPositive) {
            st = post(st, acc.id, Direction.CREDIT, proceeds, Category.TRANSFERS, TxKind.INVEST_SELL,
                (if (fr) "Vente virtuelle · " else "Virtual sale · ") + asset.name, "", counterparty = asset.symbol,
                reference = FictionalIdentifiers.receiptReference(ids)).first
        }
        val remainingQty = h.quantityMicro - quantityMicro
        val holdings = st.holdings.filterNot { it.symbol == symbol } +
            (if (remainingQty > 0) listOf(h.copy(quantityMicro = remainingQty, costBasis = h.costBasis - costPart)) else emptyList())
        val trade = Trade(ids.id("trd"), symbol, TradeSide.SELL, quantityMicro, price, proceeds, realized, clock.now(), acc.id)
        st = st.copy(holdings = holdings, trades = st.trades + trade)
        val (ti, bo) = texts(s).trade(false, asset.name, proceeds)
        st = notify(st, NotificationType.INVESTMENT, ti, bo, trade.id)
        return ok(markRequest(st, requestId), trade)
    }

    /** Valeur de marché fictive du portefeuille. */
    fun portfolioValue(s: BankState): Money =
        s.holdings.sumOfMoney { MarketSimulator.value(market.price(it.symbol, clock.now()), it.quantityMicro) }

    // ================================================================ Profil, formules, notifications

    fun updateProfile(s: BankState, transform: (Profile) -> Profile): EngineResult<Profile> {
        val p = transform(s.profile)
        if (p.firstName.isBlank()) return err(BankError.InvalidName)
        val clean = p.copy(firstName = p.firstName.trim().take(32), lastName = p.lastName.trim().take(32), email = p.email.trim().take(80), city = p.city.trim().take(48))
        // Le nom du titulaire suit le profil sur toutes les cartes.
        val holder = clean.fullName.uppercase()
        val cards = if (holder != s.profile.fullName.uppercase()) s.cards.map { it.copy(holderName = holder) } else s.cards
        return ok(s.copy(profile = clean, cards = cards), clean)
    }

    fun updatePreferences(s: BankState, transform: (Preferences) -> Preferences): EngineResult<Preferences> {
        val p = transform(s.profile.preferences)
        return ok(s.copy(profile = s.profile.copy(preferences = p)), p)
    }

    /** Change de formule. Toutes les formules sont gratuites : aucun paiement n'est demandé. */
    fun setTier(s: BankState, tier: Tier): EngineResult<Tier> {
        var p = s.profile.copy(tier = tier, tierSince = clock.now())
        // Retour à une formule inférieure : on désactive les options exclusives.
        if (!tier.advancedThemes && p.preferences.theme == com.superessence.essabank.core.model.ThemeMode.ONYX)
            p = p.copy(preferences = p.preferences.copy(theme = com.superessence.essabank.core.model.ThemeMode.DARK))
        if (!tier.customization) p = p.copy(preferences = p.preferences.copy(accent = com.superessence.essabank.core.model.Accent.ELECTRIC))
        if (!tier.customDashboard) p = p.copy(preferences = p.preferences.copy(homeWidgets = com.superessence.essabank.core.model.HomeWidget.entries.toList()))
        val cards = if (!tier.premiumDesigns) s.cards.map { if (it.design.premium) it.copy(design = CardDesign.MIDNIGHT) else it } else s.cards
        val name = "ESSA " + tier.name
        val (ti, bo) = texts(s).tier(name)
        val st = notify(s.copy(profile = p, cards = cards), NotificationType.SECURITY, ti, bo, null, force = true)
        return ok(st, tier)
    }

    fun markNotificationRead(s: BankState, id: String): EngineResult<Unit> =
        ok(s.copy(notifications = s.notifications.map { if (it.id == id) it.copy(read = true) else it }), Unit)

    fun markAllNotificationsRead(s: BankState): EngineResult<Unit> =
        ok(s.copy(notifications = s.notifications.map { it.copy(read = true) }), Unit)

    fun deleteNotification(s: BankState, id: String): EngineResult<Unit> =
        ok(s.copy(notifications = s.notifications.filterNot { it.id == id }), Unit)

    fun clearNotifications(s: BankState): EngineResult<Unit> = ok(s.copy(notifications = emptyList()), Unit)

    fun securityNotice(s: BankState, text: String): EngineResult<Unit> {
        val (ti, bo) = texts(s).security(text)
        return ok(notify(s, NotificationType.SECURITY, ti, bo, null), Unit)
    }

    fun saveLoan(s: BankState, sim: LoanSimulation): EngineResult<LoanSimulation> {
        val withId = sim.copy(id = ids.id("loan"), createdAt = clock.now())
        return ok(s.copy(loans = (listOf(withId) + s.loans).take(20)), withId)
    }

    fun deleteLoan(s: BankState, id: String): EngineResult<Unit> = ok(s.copy(loans = s.loans.filterNot { it.id == id }), Unit)

    /**
     * Supprime les transactions de démonstration. Les soldes sont conservés : une écriture
     * « Solde d'ouverture » par compte reconstitue le point de départ de l'historique restant.
     */
    fun clearDemoTransactions(s: BankState): EngineResult<Int> {
        val demo = s.transactions.filter { it.isDemo }
        if (demo.isEmpty()) return ok(s, 0)
        val remaining = s.transactions.filterNot { it.isDemo }
        val fr = isFr(s)
        val openings = s.accounts.mapNotNull { acc ->
            val accTx = remaining.filter { it.accountId == acc.id }.sortedBy { it.timestamp }
            val opening = acc.balance - accTx.sumOfMoney { it.signed }
            if (opening.isZero) null else {
                val ts = (accTx.firstOrNull()?.timestamp ?: clock.now()) - 1000
                Transaction(
                    id = ids.id("tx"), accountId = acc.id, amount = opening.abs(),
                    direction = if (opening.isNegative) Direction.DEBIT else Direction.CREDIT,
                    category = Category.OTHER, kind = TxKind.OPENING,
                    label = if (fr) "Solde d'ouverture (simulation)" else "Opening balance (simulation)",
                    timestamp = ts, balanceAfter = opening, reference = FictionalIdentifiers.receiptReference(ids),
                )
            }
        }
        val demoIds = demo.map { it.id }.toSet()
        return ok(s.copy(transactions = (openings + remaining).sortedWith(BankState.TX_ORDER), notifications = s.notifications.filterNot { it.refId in demoIds }), demo.size)
    }

    // ================================================================ Écritures (cœur comptable)

    /**
     * Passe une écriture sur un compte : met à jour le solde et enregistre la transaction
     * avec le solde après opération. Fonction interne : les contrôles sont faits par l'appelant.
     */
    internal fun post(
        s: BankState, accountId: String, direction: Direction, amount: Money, category: Category, kind: TxKind,
        label: String, note: String, cardId: String? = null, counterparty: String? = null, transferId: String? = null,
        reference: String, timestamp: Long = clock.now(), status: TxStatus = TxStatus.COMPLETED,
    ): Pair<BankState, Transaction> {
        val acc = s.account(accountId) ?: error("Compte introuvable : $accountId")
        val newBalance = if (direction == Direction.DEBIT) acc.balance - amount else acc.balance + amount
        val tx = Transaction(
            id = ids.id("tx"), accountId = accountId, amount = amount, direction = direction, category = category,
            kind = kind, label = label.take(80), note = note.trim().take(240), timestamp = timestamp, status = status,
            balanceAfter = newBalance, cardId = cardId, counterparty = counterparty, transferId = transferId, reference = reference,
            seq = s.meta.sequence + 1,
        )
        val ns = s.replaceAccount(acc.copy(balance = newBalance))
        return ns.copy(transactions = ns.transactions + tx, meta = ns.meta.copy(sequence = ns.meta.sequence + 1)) to tx
    }

    private fun notify(s: BankState, type: NotificationType, title: String, body: String, refId: String?, force: Boolean = false): BankState {
        if (!force && !NotificationTexts.typeEnabled(type, s.profile.preferences)) return s
        val n = AppNotification(ids.id("ntf"), type, title, body, clock.now(), false, refId)
        return s.copy(notifications = (listOf(n) + s.notifications).take(MAX_NOTIFICATIONS))
    }

    private fun checkAmount(amount: Money): BankError? = when {
        !amount.isPositive -> BankError.InvalidAmount
        amount > Money.MAX_OPERATION -> BankError.AmountTooLarge
        else -> null
    }

    private fun checkLimit(limit: Money): BankError? = when {
        limit < Money.units(10) -> BankError.InvalidAmount
        limit > Money.units(100_000) -> BankError.AmountTooLarge
        else -> null
    }

    private fun checkRequest(s: BankState, requestId: String?): BankError? =
        if (requestId != null && requestId in s.meta.processedRequests) BankError.DuplicateRequest else null

    private fun markRequest(s: BankState, requestId: String?): BankState =
        if (requestId == null) s else s.copy(meta = s.meta.copy(processedRequests = (s.meta.processedRequests + requestId).takeLast(MAX_REQUESTS)))

    private fun cleanName(name: String): String? = name.trim().replace(Regex("\\s+"), " ").take(40).takeIf { it.isNotEmpty() }

    private fun texts(s: BankState) = NotificationTexts(s.profile.preferences)
    private fun isFr(s: BankState) = s.profile.preferences.language == com.superessence.essabank.core.money.AppLanguage.FR

    private fun <T> ok(s: BankState, v: T, events: List<BankEvent> = emptyList()) = EngineResult.Ok(s, v, events)
    private fun err(e: BankError) = EngineResult.Err(e)

    companion object {
        const val MAX_NOTIFICATIONS = 300
        const val MAX_REQUESTS = 200
    }
}

internal fun BankState.replaceAccount(a: Account) = copy(accounts = accounts.map { if (it.id == a.id) a else it })
internal fun BankState.replaceCard(c: Card) = copy(cards = cards.map { if (it.id == c.id) c else it })
internal fun Money.coerceNonNegative(): Money = if (isNegative) Money.ZERO else this
