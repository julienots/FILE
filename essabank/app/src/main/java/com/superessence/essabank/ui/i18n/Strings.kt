package com.superessence.essabank.ui.i18n

import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.rounded.ReceiptLong
import androidx.compose.material.icons.automirrored.rounded.TrendingUp
import androidx.compose.material.icons.rounded.AccountBalance
import androidx.compose.material.icons.rounded.AccountBalanceWallet
import androidx.compose.material.icons.rounded.BeachAccess
import androidx.compose.material.icons.rounded.CardGiftcard
import androidx.compose.material.icons.rounded.Category
import androidx.compose.material.icons.rounded.CheckCircle
import androidx.compose.material.icons.rounded.CreditCard
import androidx.compose.material.icons.rounded.DirectionsCar
import androidx.compose.material.icons.rounded.Favorite
import androidx.compose.material.icons.rounded.Flight
import androidx.compose.material.icons.rounded.Home
import androidx.compose.material.icons.rounded.LocalGroceryStore
import androidx.compose.material.icons.rounded.LocalHospital
import androidx.compose.material.icons.rounded.Movie
import androidx.compose.material.icons.rounded.Notifications
import androidx.compose.material.icons.rounded.MonetizationOn
import androidx.compose.material.icons.rounded.PhotoCamera
import androidx.compose.material.icons.rounded.Restaurant
import androidx.compose.material.icons.rounded.RocketLaunch
import androidx.compose.material.icons.rounded.Savings
import androidx.compose.material.icons.rounded.School
import androidx.compose.material.icons.rounded.Security
import androidx.compose.material.icons.rounded.LocalMall
import androidx.compose.material.icons.automirrored.rounded.ShowChart
import androidx.compose.material.icons.rounded.Smartphone
import androidx.compose.material.icons.rounded.Star
import androidx.compose.material.icons.rounded.Subscriptions
import androidx.compose.material.icons.rounded.SwapHoriz
import androidx.compose.material.icons.rounded.Train
import androidx.compose.material.icons.rounded.Warning
import androidx.compose.material.icons.rounded.Work
import androidx.compose.material.icons.automirrored.rounded.CallReceived
import androidx.compose.material.icons.automirrored.rounded.CallMade
import androidx.compose.material.icons.rounded.EmojiEvents
import androidx.compose.material.icons.rounded.Insights
import androidx.compose.material.icons.rounded.Autorenew
import androidx.compose.runtime.Composable
import androidx.compose.runtime.ReadOnlyComposable
import androidx.compose.runtime.staticCompositionLocalOf
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.vector.ImageVector
import com.superessence.essabank.core.engine.BankError
import com.superessence.essabank.core.engine.Labels
import com.superessence.essabank.core.model.AccountType
import com.superessence.essabank.core.model.CardDesign
import com.superessence.essabank.core.model.CardStatus
import com.superessence.essabank.core.model.CardType
import com.superessence.essabank.core.model.Category
import com.superessence.essabank.core.model.Frequency
import com.superessence.essabank.core.model.GoalKind
import com.superessence.essabank.core.model.LoanType
import com.superessence.essabank.core.model.NotificationType
import com.superessence.essabank.core.model.Tier
import com.superessence.essabank.core.money.AppLanguage
import com.superessence.essabank.core.money.MoneyFormat
import java.time.Instant
import java.time.LocalDate
import java.time.ZoneId
import java.time.format.DateTimeFormatter
import java.time.format.TextStyle
import java.util.Locale

val LocalLang = staticCompositionLocalOf { AppLanguage.FR }

/** Texte bilingue : l'interface est entièrement disponible en français et en anglais. */
@Composable
@ReadOnlyComposable
fun tr(fr: String, en: String): String = if (LocalLang.current == AppLanguage.FR) fr else en

fun tr(lang: AppLanguage, fr: String, en: String): String = if (lang == AppLanguage.FR) fr else en

object L {
    fun locale(lang: AppLanguage): Locale = if (lang == AppLanguage.FR) Locale.FRANCE else Locale.UK

    fun error(e: BankError, lang: AppLanguage): String {
        val fr = lang == AppLanguage.FR
        return when (e) {
            BankError.InvalidAmount -> if (fr) "Montant invalide." else "Invalid amount."
            BankError.AmountTooLarge -> if (fr) "Montant supérieur au plafond de simulation." else "Amount above the simulation ceiling."
            is BankError.InsufficientFunds -> (if (fr) "Solde virtuel insuffisant. Disponible : " else "Insufficient virtual balance. Available: ") +
                MoneyFormat.format(e.available, language = lang)
            BankError.AccountNotFound -> if (fr) "Compte introuvable." else "Account not found."
            BankError.SameAccount -> if (fr) "Choisissez deux comptes différents." else "Choose two different accounts."
            BankError.CardNotFound -> if (fr) "Carte introuvable." else "Card not found."
            BankError.CardLocked -> if (fr) "Carte verrouillée : déverrouillez-la pour payer." else "Card locked: unlock it to pay."
            BankError.CardInactive -> if (fr) "Carte désactivée." else "Card disabled."
            BankError.CardExpired -> if (fr) "Carte expirée ou déjà utilisée." else "Card expired or already used."
            BankError.OnlineDisabled -> if (fr) "Paiements en ligne désactivés sur cette carte." else "Online payments are disabled on this card."
            is BankError.CardLimitExceeded -> (if (fr) "Plafond mensuel atteint. Reste : " else "Monthly limit reached. Remaining: ") +
                MoneyFormat.format(e.remaining, language = lang)
            BankError.DuplicateRequest -> if (fr) "Opération déjà enregistrée." else "Operation already recorded."
            BankError.BeneficiaryNotFound -> if (fr) "Bénéficiaire introuvable." else "Beneficiary not found."
            BankError.InvalidBeneficiary -> if (fr) "Référence fictive invalide (format ESSA-SIM-0000-0000-00)." else "Invalid fictional reference (format ESSA-SIM-0000-0000-00)."
            BankError.InvalidName -> if (fr) "Saisissez un nom." else "Enter a name."
            BankError.GoalNotFound -> if (fr) "Objectif introuvable." else "Goal not found."
            BankError.GoalInsufficient -> if (fr) "Montant supérieur à l'épargne du coffre." else "Amount higher than the vault balance."
            BankError.InvalidDate -> if (fr) "Date invalide." else "Invalid date."
            BankError.AccountNotEmpty -> if (fr) "Videz le compte avant de le supprimer." else "Empty the account before deleting it."
            BankError.PrimaryAccount -> if (fr) "Le compte principal ne peut pas être supprimé." else "The main account cannot be deleted."
            BankError.LastAccount -> if (fr) "Il faut au moins un compte." else "At least one account is required."
            BankError.LimitReached -> if (fr) "Limite de votre formule atteinte. Passez à une formule supérieure (gratuite)." else "Plan limit reached. Upgrade to a higher plan (free)."
            BankError.PremiumRequired -> if (fr) "Réservé à une formule supérieure (gratuite, simulée)." else "Requires a higher plan (free, simulated)."
            BankError.AssetNotFound -> if (fr) "Actif fictif introuvable." else "Fictional asset not found."
            BankError.InsufficientQuantity -> if (fr) "Quantité détenue insuffisante." else "Not enough units held."
            BankError.InvalidQuantity -> if (fr) "Quantité invalide." else "Invalid quantity."
            BankError.ScheduledNotFound -> if (fr) "Programmation introuvable." else "Schedule not found."
            BankError.InvalidAccountType -> if (fr) "Utilisez le compte investissement fictif." else "Use the fictional investment account."
        }
    }

    fun category(c: Category, lang: AppLanguage) = Labels.category(c, lang)

    fun categoryIcon(c: Category): ImageVector = when (c) {
        Category.FOOD -> Icons.Rounded.LocalGroceryStore
        Category.SHOPPING -> Icons.Rounded.LocalMall
        Category.TRANSPORT -> Icons.Rounded.Train
        Category.LEISURE -> Icons.Rounded.Movie
        Category.HOUSING -> Icons.Rounded.Home
        Category.SUBSCRIPTIONS -> Icons.Rounded.Subscriptions
        Category.HEALTH -> Icons.Rounded.LocalHospital
        Category.RESTAURANTS -> Icons.Rounded.Restaurant
        Category.TRANSFERS -> Icons.Rounded.SwapHoriz
        Category.INCOME -> Icons.Rounded.MonetizationOn
        Category.OTHER -> Icons.Rounded.Category
    }

    fun categoryColor(c: Category): Color = when (c) {
        Category.FOOD -> Color(0xFF12B886)
        Category.SHOPPING -> Color(0xFFE64980)
        Category.TRANSPORT -> Color(0xFF00B8D9)
        Category.LEISURE -> Color(0xFF7B5CFF)
        Category.HOUSING -> Color(0xFF2F6BFF)
        Category.SUBSCRIPTIONS -> Color(0xFFFF8A3D)
        Category.HEALTH -> Color(0xFFF0475C)
        Category.RESTAURANTS -> Color(0xFFF5A524)
        Category.TRANSFERS -> Color(0xFF5C6B8A)
        Category.INCOME -> Color(0xFF16B47E)
        Category.OTHER -> Color(0xFF8A93AE)
    }

    fun accountType(t: AccountType, lang: AppLanguage): String {
        val fr = lang == AppLanguage.FR
        return when (t) {
            AccountType.CURRENT -> if (fr) "Compte courant" else "Current account"
            AccountType.SAVINGS -> if (fr) "Compte épargne" else "Savings account"
            AccountType.PROJECT -> if (fr) "Compte projet" else "Project account"
            AccountType.VACATION -> if (fr) "Compte vacances" else "Holiday account"
            AccountType.BUSINESS -> if (fr) "Compte professionnel" else "Business account"
            AccountType.INVESTMENT -> if (fr) "Compte investissement fictif" else "Fictional investment account"
        }
    }

    fun defaultIcon(t: AccountType): String = when (t) {
        AccountType.CURRENT -> "wallet"; AccountType.SAVINGS -> "savings"; AccountType.PROJECT -> "rocket"
        AccountType.VACATION -> "beach"; AccountType.BUSINESS -> "briefcase"; AccountType.INVESTMENT -> "chart"
    }

    val accountIcons = listOf("wallet", "savings", "rocket", "beach", "briefcase", "chart", "home", "car", "heart", "star", "school", "gift", "camera", "flight")

    fun icon(key: String): ImageVector = when (key) {
        "wallet" -> Icons.Rounded.AccountBalanceWallet
        "savings" -> Icons.Rounded.Savings
        "rocket" -> Icons.Rounded.RocketLaunch
        "beach" -> Icons.Rounded.BeachAccess
        "briefcase" -> Icons.Rounded.Work
        "chart" -> Icons.AutoMirrored.Rounded.ShowChart
        "home" -> Icons.Rounded.Home
        "car" -> Icons.Rounded.DirectionsCar
        "heart" -> Icons.Rounded.Favorite
        "star" -> Icons.Rounded.Star
        "school" -> Icons.Rounded.School
        "gift" -> Icons.Rounded.CardGiftcard
        "camera" -> Icons.Rounded.PhotoCamera
        "flight" -> Icons.Rounded.Flight
        else -> Icons.Rounded.AccountBalance
    }

    fun cardType(t: CardType, lang: AppLanguage): String {
        val fr = lang == AppLanguage.FR
        return when (t) {
            CardType.VIRTUAL -> if (fr) "Carte virtuelle" else "Virtual card"
            CardType.PHYSICAL -> if (fr) "Carte physique simulée" else "Simulated physical card"
            CardType.PREMIUM -> if (fr) "Carte premium" else "Premium card"
            CardType.EPHEMERAL -> if (fr) "Carte éphémère" else "Disposable card"
            CardType.SECONDARY -> if (fr) "Carte secondaire" else "Secondary card"
        }
    }

    fun cardTypeHint(t: CardType, lang: AppLanguage): String {
        val fr = lang == AppLanguage.FR
        return when (t) {
            CardType.VIRTUAL -> if (fr) "Pour les achats en ligne simulés." else "For simulated online purchases."
            CardType.PHYSICAL -> if (fr) "Carte du quotidien, sans contact simulé." else "Everyday card with simulated contactless."
            CardType.PREMIUM -> if (fr) "Plafonds élevés et designs exclusifs." else "Higher limits and exclusive designs."
            CardType.EPHEMERAL -> if (fr) "Un seul paiement, puis désactivation automatique." else "Single payment, then disabled automatically."
            CardType.SECONDARY -> if (fr) "Pour un usage dédié (courses, freelance…)." else "For a dedicated use (groceries, freelance…)."
        }
    }

    fun cardStatus(s: CardStatus, lang: AppLanguage): String {
        val fr = lang == AppLanguage.FR
        return when (s) {
            CardStatus.ACTIVE -> if (fr) "Active" else "Active"
            CardStatus.INACTIVE -> if (fr) "Désactivée" else "Disabled"
            CardStatus.LOCKED -> if (fr) "Verrouillée" else "Locked"
        }
    }

    fun design(d: CardDesign, lang: AppLanguage): String {
        val fr = lang == AppLanguage.FR
        return when (d) {
            CardDesign.MIDNIGHT -> if (fr) "Minuit" else "Midnight"
            CardDesign.ELECTRIC -> if (fr) "Électrique" else "Electric"
            CardDesign.GLACIER -> "Glacier"
            CardDesign.AURORA -> if (fr) "Aurore" else "Aurora"
            CardDesign.GRAPHITE -> "Graphite"
            CardDesign.OCEAN -> if (fr) "Océan" else "Ocean"
            CardDesign.ONYX -> "Onyx"
            CardDesign.GOLD_LEAF -> if (fr) "Feuille d'or" else "Gold leaf"
            CardDesign.TITANIUM -> if (fr) "Titane" else "Titanium"
            CardDesign.NEBULA -> if (fr) "Nébuleuse" else "Nebula"
        }
    }

    fun goalKind(k: GoalKind, lang: AppLanguage): String {
        val fr = lang == AppLanguage.FR
        return when (k) {
            GoalKind.CAR -> if (fr) "Acheter une voiture" else "Buy a car"
            GoalKind.HOUSE -> if (fr) "Acheter une maison" else "Buy a house"
            GoalKind.VACATION -> if (fr) "Vacances" else "Holidays"
            GoalKind.PHONE -> if (fr) "Nouveau téléphone" else "New phone"
            GoalKind.PROJECT -> if (fr) "Projet personnel" else "Personal project"
            GoalKind.FREE -> if (fr) "Objectif libre" else "Custom goal"
        }
    }

    fun goalIcon(k: GoalKind): ImageVector = when (k) {
        GoalKind.CAR -> Icons.Rounded.DirectionsCar
        GoalKind.HOUSE -> Icons.Rounded.Home
        GoalKind.VACATION -> Icons.Rounded.Flight
        GoalKind.PHONE -> Icons.Rounded.Smartphone
        GoalKind.PROJECT -> Icons.Rounded.RocketLaunch
        GoalKind.FREE -> Icons.Rounded.Star
    }

    fun frequency(f: Frequency, lang: AppLanguage): String {
        val fr = lang == AppLanguage.FR
        return when (f) {
            Frequency.ONCE -> if (fr) "Une fois" else "Once"
            Frequency.WEEKLY -> if (fr) "Chaque semaine" else "Weekly"
            Frequency.MONTHLY -> if (fr) "Chaque mois" else "Monthly"
        }
    }

    fun loanType(t: LoanType, lang: AppLanguage): String {
        val fr = lang == AppLanguage.FR
        return when (t) {
            LoanType.PERSONAL -> if (fr) "Personnel" else "Personal"
            LoanType.AUTO -> if (fr) "Automobile" else "Car"
            LoanType.HOME -> if (fr) "Immobilier" else "Mortgage"
            LoanType.STUDENT -> if (fr) "Étudiant" else "Student"
        }
    }

    fun loanIcon(t: LoanType): ImageVector = when (t) {
        LoanType.PERSONAL -> Icons.Rounded.MonetizationOn
        LoanType.AUTO -> Icons.Rounded.DirectionsCar
        LoanType.HOME -> Icons.Rounded.Home
        LoanType.STUDENT -> Icons.Rounded.School
    }

    fun tierName(t: Tier) = "ESSA " + t.name

    fun notificationIcon(t: NotificationType): ImageVector = when (t) {
        NotificationType.PAYMENT -> Icons.Rounded.CreditCard
        NotificationType.TRANSFER_RECEIVED -> Icons.AutoMirrored.Rounded.CallReceived
        NotificationType.TRANSFER_SENT -> Icons.AutoMirrored.Rounded.CallMade
        NotificationType.GOAL_REACHED -> Icons.Rounded.EmojiEvents
        NotificationType.BUDGET_EXCEEDED, NotificationType.BUDGET_WARNING -> Icons.Rounded.Warning
        NotificationType.NEW_TRANSACTION -> Icons.AutoMirrored.Rounded.ReceiptLong
        NotificationType.MONTHLY_SUMMARY -> Icons.Rounded.Insights
        NotificationType.CARD_CREATED, NotificationType.CARD_UPDATED -> Icons.Rounded.CreditCard
        NotificationType.SCHEDULED_FAILED -> Icons.Rounded.Autorenew
        NotificationType.INVESTMENT -> Icons.AutoMirrored.Rounded.TrendingUp
        NotificationType.SECURITY -> Icons.Rounded.Security
        NotificationType.WELCOME -> Icons.Rounded.CheckCircle
    }

    fun notificationIconFallback(): ImageVector = Icons.Rounded.Notifications

    // ------------------------------------------------------------------ dates

    fun dayHeader(date: LocalDate, today: LocalDate, lang: AppLanguage): String {
        val fr = lang == AppLanguage.FR
        return when (date) {
            today -> if (fr) "Aujourd'hui" else "Today"
            today.minusDays(1) -> if (fr) "Hier" else "Yesterday"
            else -> {
                val pattern = if (date.year == today.year) (if (fr) "EEEE d MMMM" else "EEEE, MMMM d") else (if (fr) "EEEE d MMMM yyyy" else "EEEE, MMMM d, yyyy")
                DateTimeFormatter.ofPattern(pattern, locale(lang)).format(date).replaceFirstChar { it.titlecase(locale(lang)) }
            }
        }
    }

    fun time(millis: Long, zone: ZoneId): String = DateTimeFormatter.ofPattern("HH:mm").format(Instant.ofEpochMilli(millis).atZone(zone))

    fun date(millis: Long, zone: ZoneId, lang: AppLanguage): String =
        DateTimeFormatter.ofPattern(if (lang == AppLanguage.FR) "d MMM yyyy" else "MMM d, yyyy", locale(lang)).format(Instant.ofEpochMilli(millis).atZone(zone))

    fun dateTime(millis: Long, zone: ZoneId, lang: AppLanguage): String =
        DateTimeFormatter.ofPattern(if (lang == AppLanguage.FR) "d MMM yyyy 'à' HH:mm" else "MMM d, yyyy 'at' HH:mm", locale(lang)).format(Instant.ofEpochMilli(millis).atZone(zone))

    fun date(day: LocalDate, lang: AppLanguage): String =
        DateTimeFormatter.ofPattern(if (lang == AppLanguage.FR) "d MMMM yyyy" else "MMMM d, yyyy", locale(lang)).format(day)

    fun shortDate(day: LocalDate, lang: AppLanguage): String =
        DateTimeFormatter.ofPattern(if (lang == AppLanguage.FR) "d MMM" else "MMM d", locale(lang)).format(day)

    fun monthShort(day: LocalDate, lang: AppLanguage): String =
        day.month.getDisplayName(TextStyle.SHORT, locale(lang)).replace(".", "").replaceFirstChar { it.titlecase(locale(lang)) }

    fun monthLong(day: LocalDate, lang: AppLanguage): String =
        day.month.getDisplayName(TextStyle.FULL_STANDALONE, locale(lang)).replaceFirstChar { it.titlecase(locale(lang)) } + " " + day.year

    fun weekdayInitial(day: LocalDate, lang: AppLanguage): String =
        day.dayOfWeek.getDisplayName(TextStyle.NARROW_STANDALONE, locale(lang)).uppercase(locale(lang))

    fun relative(millis: Long, now: Long, zone: ZoneId, lang: AppLanguage): String {
        val fr = lang == AppLanguage.FR
        val diff = (now - millis).coerceAtLeast(0)
        val min = diff / 60_000
        return when {
            min < 1 -> if (fr) "À l'instant" else "Just now"
            min < 60 -> if (fr) "Il y a $min min" else "$min min ago"
            min < 24 * 60 -> if (fr) "Il y a ${min / 60} h" else "${min / 60} h ago"
            min < 7 * 24 * 60 -> if (fr) "Il y a ${min / 1440} j" else "${min / 1440} d ago"
            else -> date(millis, zone, lang)
        }
    }
}
