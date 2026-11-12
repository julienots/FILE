package com.superessence.essabank.core.model

import com.superessence.essabank.core.money.AppLanguage
import com.superessence.essabank.core.money.DisplayCurrency
import com.superessence.essabank.core.money.Money
import kotlinx.serialization.Serializable

/*
 * Modèle de données ESSABANK.
 * Toutes les entités représentent des objets VIRTUELS : aucun compte, aucune carte,
 * aucun bénéficiaire n'existe en dehors de l'application.
 */

// ---------------------------------------------------------------- Profil

@Serializable
enum class Tier { STANDARD, PLUS, BLACK;
    val maxCards: Int get() = when (this) { STANDARD -> 3; PLUS -> 10; BLACK -> 20 }
    val maxGoals: Int get() = when (this) { STANDARD -> 3; PLUS, BLACK -> Int.MAX_VALUE }
    val maxAccounts: Int get() = when (this) { STANDARD -> 6; PLUS -> 12; BLACK -> 24 }
    val advancedStats: Boolean get() = this != STANDARD
    val customization: Boolean get() = this != STANDARD
    val premiumDesigns: Boolean get() = this == BLACK
    val advancedThemes: Boolean get() = this == BLACK
    val customDashboard: Boolean get() = this == BLACK
}

@Serializable
enum class ThemeMode { SYSTEM, LIGHT, DARK, ONYX }

@Serializable
enum class Accent { ELECTRIC, AZURE, VIOLET, EMERALD, GOLD }

@Serializable
enum class HomeWidget { BALANCE, QUICK_ACTIONS, INSIGHTS, WEALTH, ACCOUNTS, CARDS, BUDGET, GOALS, TRANSACTIONS }

@Serializable
data class NotificationPrefs(
    val payments: Boolean = true,
    val transfers: Boolean = true,
    val savings: Boolean = true,
    val budget: Boolean = true,
    val cards: Boolean = true,
    val summaries: Boolean = true,
    val investments: Boolean = true,
    val security: Boolean = true,
)

@Serializable
data class Preferences(
    val language: AppLanguage = AppLanguage.FR,
    val currency: DisplayCurrency = DisplayCurrency.UV,
    val theme: ThemeMode = ThemeMode.SYSTEM,
    val accent: Accent = Accent.ELECTRIC,
    val hideBalances: Boolean = false,
    val notifications: NotificationPrefs = NotificationPrefs(),
    val homeWidgets: List<HomeWidget> = HomeWidget.entries.toList(),
    val animations: Boolean = true,
)

@Serializable
data class Profile(
    val firstName: String,
    val lastName: String,
    val email: String = "",
    val city: String = "",
    /** Index de l'avatar vectoriel, ou -1 si une photo locale est utilisée. */
    val avatar: Int = 0,
    val photoPath: String? = null,
    val clientNumber: String,
    val createdAt: Long,
    val tier: Tier = Tier.STANDARD,
    val tierSince: Long = createdAt,
    val preferences: Preferences = Preferences(),
    val isDemo: Boolean = false,
) {
    val fullName: String get() = "$firstName $lastName".trim()
    val initials: String get() =
        (firstName.firstOrNull()?.uppercase() ?: "") + (lastName.firstOrNull()?.uppercase() ?: "")
}

// ---------------------------------------------------------------- Comptes

@Serializable
enum class AccountType { CURRENT, SAVINGS, PROJECT, VACATION, BUSINESS, INVESTMENT }

@Serializable
data class Account(
    val id: String,
    val type: AccountType,
    val name: String,
    val colorIndex: Int,
    val icon: String,
    val balance: Money,
    /** Identifiant fictif interne, volontairement différent d'un IBAN. */
    val reference: String,
    val createdAt: Long,
    /** Découvert virtuel autorisé (compte courant uniquement). */
    val overdraft: Money = Money.ZERO,
    /** Taux de rémunération fictif annuel, en points de base (250 = 2,50 %). */
    val interestBp: Int = 0,
    val isPrimary: Boolean = false,
    val includeInTotal: Boolean = true,
    val roundUp: Boolean = false,
    val sortOrder: Int = 0,
) {
    /** Montant disponible pour une opération (solde + découvert virtuel). */
    val available: Money get() = balance + overdraft
}

// ---------------------------------------------------------------- Cartes

@Serializable
enum class CardType { VIRTUAL, PHYSICAL, PREMIUM, EPHEMERAL, SECONDARY }

@Serializable
enum class CardStatus { ACTIVE, INACTIVE, LOCKED }

@Serializable
enum class CardDesign(val premium: Boolean) {
    MIDNIGHT(false), ELECTRIC(false), GLACIER(false), AURORA(false), GRAPHITE(false), OCEAN(false),
    ONYX(true), GOLD_LEAF(true), TITANIUM(true), NEBULA(true),
}

@Serializable
data class Card(
    val id: String,
    val accountId: String,
    val type: CardType,
    val label: String,
    val holderName: String,
    /** 16 chiffres commençant par 0 : préfixe non attribué, numéro inutilisable sur un réseau réel. */
    val number: String,
    val expMonth: Int,
    val expYear: Int,
    val cvv: String,
    val design: CardDesign,
    val status: CardStatus = CardStatus.ACTIVE,
    val monthlyLimit: Money,
    val onlinePayments: Boolean = true,
    val contactless: Boolean = true,
    val createdAt: Long,
    /** Carte éphémère : nombre de paiements restants avant désactivation automatique. */
    val usesLeft: Int? = null,
) {
    val last4: String get() = number.takeLast(4)
    val maskedNumber: String get() = "•••• •••• •••• $last4"
    val formattedNumber: String get() = number.chunked(4).joinToString(" ")
    val expiry: String get() = "%02d/%02d".format(expMonth, expYear % 100)
}

// ---------------------------------------------------------------- Transactions

@Serializable
enum class Category {
    FOOD, SHOPPING, TRANSPORT, LEISURE, HOUSING, SUBSCRIPTIONS, HEALTH, RESTAURANTS, TRANSFERS, INCOME, OTHER;

    companion object {
        val spending = listOf(FOOD, SHOPPING, TRANSPORT, LEISURE, HOUSING, SUBSCRIPTIONS, HEALTH, RESTAURANTS, TRANSFERS, OTHER)
    }
}

@Serializable
enum class Direction { DEBIT, CREDIT }

@Serializable
enum class TxStatus { COMPLETED, PENDING, DECLINED }

@Serializable
enum class TxKind {
    /** Paiement par carte chez un commerçant fictif. */
    CARD_PAYMENT,
    /** Prélèvement ou paiement divers. */
    PAYMENT,
    /** Virement entre deux comptes de l'utilisateur (deux écritures). */
    INTERNAL_TRANSFER,
    /** Virement vers un bénéficiaire fictif. */
    TRANSFER_OUT,
    /** Virement fictif reçu. */
    TRANSFER_IN,
    INCOME,
    SAVINGS_DEPOSIT,
    SAVINGS_WITHDRAWAL,
    INVEST_BUY,
    INVEST_SELL,
    INTEREST,
    OPENING,
    ;

    /** Opération comptée dans les dépenses (exclut les mouvements internes). */
    val isSpending: Boolean get() = this == CARD_PAYMENT || this == PAYMENT || this == TRANSFER_OUT
    /** Opération comptée dans les revenus. */
    val isIncome: Boolean get() = this == INCOME || this == TRANSFER_IN || this == INTEREST
    val isInternal: Boolean get() = !isSpending && !isIncome && this != OPENING
}

@Serializable
data class Transaction(
    val id: String,
    val accountId: String,
    /** Montant toujours positif ; le sens est donné par [direction]. */
    val amount: Money,
    val direction: Direction,
    val category: Category,
    val kind: TxKind,
    val label: String,
    val note: String = "",
    val timestamp: Long,
    val status: TxStatus = TxStatus.COMPLETED,
    /** Solde du compte immédiatement après l'opération. */
    val balanceAfter: Money,
    val cardId: String? = null,
    val counterparty: String? = null,
    /** Identifiant commun aux deux écritures d'un virement interne. */
    val transferId: String? = null,
    /** Référence de reçu « ESB-… ». */
    val reference: String,
    val isDemo: Boolean = false,
    /** Numéro d'ordre global : départage les écritures passées à la même milliseconde. */
    val seq: Long = 0,
) {
    val signed: Money get() = if (direction == Direction.DEBIT) -amount else amount
    val isDebit: Boolean get() = direction == Direction.DEBIT
}

// ---------------------------------------------------------------- Bénéficiaires et virements programmés

@Serializable
data class Beneficiary(
    val id: String,
    val name: String,
    /** Référence fictive « ESSA-SIM-XXXX-XXXX-XX ». */
    val reference: String,
    val bankLabel: String = "Banque fictive",
    val note: String = "",
    val favorite: Boolean = false,
    val colorIndex: Int = 0,
    val createdAt: Long,
)

@Serializable
enum class Frequency { ONCE, WEEKLY, MONTHLY }

@Serializable
data class ScheduledTransfer(
    val id: String,
    val fromAccountId: String,
    /** Destination : un compte de l'utilisateur OU un bénéficiaire. */
    val toAccountId: String? = null,
    val beneficiaryId: String? = null,
    val amount: Money,
    val label: String,
    val note: String = "",
    /** Prochaine exécution (jour, en epochDay). */
    val nextDate: Long,
    val frequency: Frequency,
    val endDate: Long? = null,
    val active: Boolean = true,
    val createdAt: Long,
    val executions: Int = 0,
    val failures: Int = 0,
)

// ---------------------------------------------------------------- Budgets

@Serializable
data class Budgets(
    val monthly: Money? = null,
    val categories: Map<Category, Money> = emptyMap(),
    /** Alertes déjà émises, clé « 2026-11:FOOD:100 », pour éviter les doublons. */
    val alertsSent: Set<String> = emptySet(),
)

// ---------------------------------------------------------------- Épargne

@Serializable
enum class GoalKind { CAR, HOUSE, VACATION, PHONE, PROJECT, FREE }

@Serializable
data class AutoSave(
    val amount: Money,
    val frequency: Frequency,
    val fromAccountId: String,
    val nextDate: Long,
    val active: Boolean = true,
)

@Serializable
data class SavingsGoal(
    val id: String,
    val name: String,
    val kind: GoalKind,
    val target: Money,
    val saved: Money = Money.ZERO,
    val targetDate: Long? = null,
    val colorIndex: Int = 0,
    val createdAt: Long,
    val completedAt: Long? = null,
    val autoSave: AutoSave? = null,
    val sortOrder: Int = 0,
) {
    val progress: Float get() =
        if (target.cents <= 0) 0f else (saved.cents.toDouble() / target.cents).coerceIn(0.0, 1.0).toFloat()
    val remaining: Money get() = if (saved >= target) Money.ZERO else target - saved
    val isReached: Boolean get() = saved >= target
}

@Serializable
enum class GoalMoveType { DEPOSIT, WITHDRAWAL }

@Serializable
data class GoalMovement(
    val id: String,
    val goalId: String,
    val accountId: String,
    val amount: Money,
    val type: GoalMoveType,
    val timestamp: Long,
    val automatic: Boolean = false,
)

// ---------------------------------------------------------------- Investissements fictifs

@Serializable
enum class AssetType { STOCK, CRYPTO, ETF }

@Serializable
enum class TradeSide { BUY, SELL }

/**
 * Position sur un actif fictif. La quantité est stockée en millionièmes d'unité.
 * [costBasis] = coût d'acquisition total restant (méthode du coût moyen pondéré).
 */
@Serializable
data class Holding(
    val symbol: String,
    val quantityMicro: Long,
    val costBasis: Money,
)

@Serializable
data class Trade(
    val id: String,
    val symbol: String,
    val side: TradeSide,
    val quantityMicro: Long,
    /** Prix unitaire fictif en centièmes. */
    val price: Money,
    val total: Money,
    /** Plus ou moins-value réalisée (ventes uniquement). */
    val realized: Money = Money.ZERO,
    val timestamp: Long,
    val accountId: String,
)

// ---------------------------------------------------------------- Crédits simulés

@Serializable
enum class LoanType { PERSONAL, AUTO, HOME, STUDENT }

@Serializable
data class LoanSimulation(
    val id: String,
    val type: LoanType,
    val principal: Money,
    /** Taux annuel fictif en points de base. */
    val rateBp: Int,
    val months: Int,
    /** Assurance fictive annuelle en points de base du capital. */
    val insuranceBp: Int = 0,
    val createdAt: Long,
    val label: String = "",
)

// ---------------------------------------------------------------- Notifications

@Serializable
enum class NotificationType {
    PAYMENT, TRANSFER_RECEIVED, TRANSFER_SENT, GOAL_REACHED, BUDGET_EXCEEDED, BUDGET_WARNING,
    NEW_TRANSACTION, MONTHLY_SUMMARY, CARD_CREATED, CARD_UPDATED, SCHEDULED_FAILED, INVESTMENT, SECURITY, WELCOME,
}

@Serializable
data class AppNotification(
    val id: String,
    val type: NotificationType,
    val title: String,
    val body: String,
    val timestamp: Long,
    val read: Boolean = false,
    val refId: String? = null,
)

// ---------------------------------------------------------------- État global

@Serializable
data class Meta(
    val schemaVersion: Int = 1,
    /** Identifiants de requêtes déjà traitées (anti-doublon), limités aux plus récents. */
    val processedRequests: List<String> = emptyList(),
    val lastSummaryMonth: String? = null,
    val sequence: Long = 0,
)

@Serializable
data class BankState(
    val profile: Profile,
    val accounts: List<Account> = emptyList(),
    val cards: List<Card> = emptyList(),
    val transactions: List<Transaction> = emptyList(),
    val beneficiaries: List<Beneficiary> = emptyList(),
    val scheduled: List<ScheduledTransfer> = emptyList(),
    val budgets: Budgets = Budgets(),
    val goals: List<SavingsGoal> = emptyList(),
    val goalMovements: List<GoalMovement> = emptyList(),
    val holdings: List<Holding> = emptyList(),
    val trades: List<Trade> = emptyList(),
    val loans: List<LoanSimulation> = emptyList(),
    val notifications: List<AppNotification> = emptyList(),
    val meta: Meta = Meta(),
) {
    fun account(id: String): Account? = accounts.firstOrNull { it.id == id }
    fun card(id: String): Card? = cards.firstOrNull { it.id == id }
    fun goal(id: String): SavingsGoal? = goals.firstOrNull { it.id == id }
    fun beneficiary(id: String): Beneficiary? = beneficiaries.firstOrNull { it.id == id }
    val primaryAccount: Account? get() = accounts.firstOrNull { it.isPrimary } ?: accounts.firstOrNull()
    val unreadNotifications: Int get() = notifications.count { !it.read }
    fun transactionsFor(accountId: String): List<Transaction> = transactions.filter { it.accountId == accountId }

    /** Ordre canonique des collections (utilisé au chargement et pour comparer deux états). */
    fun canonical(): BankState = copy(
        accounts = accounts.sortedWith(compareBy({ it.sortOrder }, { it.id })),
        cards = cards.sortedWith(compareBy({ it.createdAt }, { it.id })),
        transactions = transactions.sortedWith(TX_ORDER),
        beneficiaries = beneficiaries.sortedWith(compareBy({ it.createdAt }, { it.id })),
        scheduled = scheduled.sortedWith(compareBy({ it.createdAt }, { it.id })),
        goals = goals.sortedWith(compareBy({ it.sortOrder }, { it.id })),
        goalMovements = goalMovements.sortedWith(compareBy({ it.timestamp }, { it.id })),
        holdings = holdings.sortedBy { it.symbol },
        trades = trades.sortedWith(compareBy({ it.timestamp }, { it.id })),
        loans = loans.sortedWith(compareByDescending<LoanSimulation> { it.createdAt }.thenBy { it.id }),
        notifications = notifications.sortedWith(compareByDescending<AppNotification> { it.timestamp }.thenBy { it.id }),
    )

    companion object {
        /** Ordre chronologique des écritures. */
        val TX_ORDER: Comparator<Transaction> = compareBy({ it.timestamp }, { it.seq }, { it.id })
    }
}
