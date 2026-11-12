package com.superessence.essabank.core.engine

import com.superessence.essabank.core.model.Category
import com.superessence.essabank.core.model.NotificationType
import com.superessence.essabank.core.model.Preferences
import com.superessence.essabank.core.money.AppLanguage
import com.superessence.essabank.core.money.Money
import com.superessence.essabank.core.money.MoneyFormat

/** Textes des notifications générées par le moteur, en français ou en anglais. */
class NotificationTexts(private val prefs: Preferences) {
    private val fr = prefs.language == AppLanguage.FR
    fun m(money: Money) = MoneyFormat.format(money, prefs.currency, prefs.language)

    fun payment(merchant: String, amount: Money, last4: String) =
        if (fr) "Paiement virtuel effectué" to "${m(amount)} chez $merchant avec la carte •••• $last4."
        else "Virtual payment made" to "${m(amount)} at $merchant with card •••• $last4."

    fun transferSent(name: String, amount: Money) =
        if (fr) "Virement simulé envoyé" to "${m(amount)} envoyés à $name. Aucun transfert réel."
        else "Simulated transfer sent" to "${m(amount)} sent to $name. No real transfer."

    fun transferReceived(name: String, amount: Money) =
        if (fr) "Virement simulé reçu" to "Vous avez reçu ${m(amount)} de $name."
        else "Simulated transfer received" to "You received ${m(amount)} from $name."

    fun internal(from: String, to: String, amount: Money) =
        if (fr) "Nouvelle transaction" to "${m(amount)} transférés de $from vers $to."
        else "New transaction" to "${m(amount)} moved from $from to $to."

    fun goalReached(name: String) =
        if (fr) "Objectif d'épargne atteint" to "Félicitations, l'objectif « $name » est atteint."
        else "Savings goal reached" to "Congratulations, your goal \"$name\" has been reached."

    fun budgetExceeded(scope: String, spent: Money, budget: Money) =
        if (fr) "Budget dépassé" to "$scope : ${m(spent)} dépensés pour un budget de ${m(budget)}."
        else "Budget exceeded" to "$scope: ${m(spent)} spent against a budget of ${m(budget)}."

    fun budgetWarning(scope: String, percent: Int) =
        if (fr) "Budget bientôt atteint" to "$scope : $percent % du budget mensuel est déjà utilisé."
        else "Budget almost reached" to "$scope: $percent% of the monthly budget is already used."

    fun monthlySummary(month: String, income: Money, spent: Money) =
        if (fr) "Résumé mensuel disponible" to "$month : ${m(income)} de revenus, ${m(spent)} de dépenses."
        else "Monthly summary available" to "$month: ${m(income)} income, ${m(spent)} spending."

    fun cardCreated(label: String, last4: String) =
        if (fr) "Carte virtuelle créée" to "Votre carte « $label » •••• $last4 est prête."
        else "Virtual card created" to "Your card \"$label\" •••• $last4 is ready."

    fun cardStatus(label: String, status: String) =
        if (fr) "Carte mise à jour" to "La carte « $label » est désormais $status."
        else "Card updated" to "The card \"$label\" is now $status."

    fun scheduledFailed(label: String, amount: Money) =
        if (fr) "Virement programmé non exécuté" to "« $label » (${m(amount)}) : solde virtuel insuffisant."
        else "Scheduled transfer not executed" to "\"$label\" (${m(amount)}): insufficient virtual balance."

    fun scheduledDone(label: String, amount: Money) =
        if (fr) "Virement programmé exécuté" to "« $label » : ${m(amount)} transférés."
        else "Scheduled transfer executed" to "\"$label\": ${m(amount)} transferred."

    fun autoSave(goal: String, amount: Money) =
        if (fr) "Épargne automatique" to "${m(amount)} versés dans le coffre « $goal »."
        else "Automatic savings" to "${m(amount)} added to the \"$goal\" vault."

    fun trade(buy: Boolean, name: String, amount: Money) =
        if (fr) (if (buy) "Achat virtuel exécuté" else "Vente virtuelle exécutée") to "$name : ${m(amount)} (marché de démonstration)."
        else (if (buy) "Virtual purchase executed" else "Virtual sale executed") to "$name: ${m(amount)} (demo market)."

    fun interest(account: String, amount: Money) =
        if (fr) "Intérêts fictifs versés" to "${m(amount)} d'intérêts fictifs sur « $account »."
        else "Fictional interest paid" to "${m(amount)} of fictional interest on \"$account\"."

    fun welcome(name: String) =
        if (fr) "Bienvenue chez ESSABANK" to "Bonjour $name, votre espace de simulation est prêt. Toutes les opérations sont virtuelles."
        else "Welcome to ESSABANK" to "Hello $name, your simulation space is ready. All operations are virtual."

    fun tier(name: String) =
        if (fr) "Formule activée" to "La formule $name est active. Aucun paiement n'a été demandé."
        else "Plan activated" to "The $name plan is active. No payment was requested."

    fun security(text: String) = (if (fr) "Sécurité" else "Security") to text

    fun cardStatusLabel(status: com.superessence.essabank.core.model.CardStatus) = when (status) {
        com.superessence.essabank.core.model.CardStatus.ACTIVE -> if (fr) "active" else "active"
        com.superessence.essabank.core.model.CardStatus.INACTIVE -> if (fr) "désactivée" else "disabled"
        com.superessence.essabank.core.model.CardStatus.LOCKED -> if (fr) "verrouillée" else "locked"
    }

    fun categoryName(c: Category): String = Labels.category(c, prefs.language)

    companion object {
        fun typeEnabled(type: NotificationType, prefs: Preferences): Boolean {
            val n = prefs.notifications
            return when (type) {
                NotificationType.PAYMENT, NotificationType.NEW_TRANSACTION -> n.payments
                NotificationType.TRANSFER_RECEIVED, NotificationType.TRANSFER_SENT, NotificationType.SCHEDULED_FAILED -> n.transfers
                NotificationType.GOAL_REACHED -> n.savings
                NotificationType.BUDGET_EXCEEDED, NotificationType.BUDGET_WARNING -> n.budget
                NotificationType.CARD_CREATED, NotificationType.CARD_UPDATED -> n.cards
                NotificationType.MONTHLY_SUMMARY -> n.summaries
                NotificationType.INVESTMENT -> n.investments
                NotificationType.SECURITY -> n.security
                NotificationType.WELCOME -> true
            }
        }
    }
}

/** Libellés partagés entre le moteur, l'export PDF et l'interface. */
object Labels {
    fun category(c: Category, lang: AppLanguage): String {
        val fr = lang == AppLanguage.FR
        return when (c) {
            Category.FOOD -> if (fr) "Alimentation" else "Groceries"
            Category.SHOPPING -> "Shopping"
            Category.TRANSPORT -> "Transport"
            Category.LEISURE -> if (fr) "Loisirs" else "Leisure"
            Category.HOUSING -> if (fr) "Logement" else "Housing"
            Category.SUBSCRIPTIONS -> if (fr) "Abonnements" else "Subscriptions"
            Category.HEALTH -> if (fr) "Santé" else "Health"
            Category.RESTAURANTS -> "Restaurants"
            Category.TRANSFERS -> if (fr) "Transferts" else "Transfers"
            Category.INCOME -> if (fr) "Revenus" else "Income"
            Category.OTHER -> if (fr) "Autres" else "Other"
        }
    }

    fun month(year: Int, month: Int, lang: AppLanguage): String {
        val fr = listOf("janvier", "février", "mars", "avril", "mai", "juin", "juillet", "août", "septembre", "octobre", "novembre", "décembre")
        val en = listOf("January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December")
        val name = if (lang == AppLanguage.FR) fr[month - 1] else en[month - 1]
        return "$name $year"
    }
}
