package com.superessence.noctea.billing

import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow

/**
 * Droits d'accès NOCTÉA (gratuit / Premium).
 *
 * L'architecture est prête pour Google Play Billing : il suffit de fournir une implémentation
 * de [BillingGateway] basée sur la bibliothèque `com.android.billingclient:billing`.
 * Tant qu'aucune intégration de paiement réelle n'existe, AUCUN paiement n'est simulé :
 * la passerelle « offre de lancement » accorde l'accès complet gratuitement et l'écran Premium
 * l'indique clairement.
 */
enum class Tier { FREE, PREMIUM }

data class Plan(val id: String, val title: String, val priceLabel: String, val period: String)

interface BillingGateway {
    val tier: StateFlow<Tier>
    val purchasesAvailable: Boolean
    val plans: List<Plan>
    /** Message affiché si les achats ne sont pas disponibles. */
    val statusMessage: String
}

/** Offre de lancement : tout est accessible, aucun achat n'est proposé. */
class LaunchOfferGateway : BillingGateway {
    private val _tier = MutableStateFlow(Tier.PREMIUM)
    override val tier: StateFlow<Tier> = _tier.asStateFlow()
    override val purchasesAvailable = false
    override val plans = listOf(
        Plan("noctea_premium_monthly", "Mensuel", "—", "par mois"),
        Plan("noctea_premium_yearly", "Annuel", "—", "par an"),
    )
    override val statusMessage =
        "Offre de lancement : NOCTÉA Premium est entièrement offert. Les abonnements via Google Play seront proposés dans une prochaine version."
}

/** Règles d'accès, appliquées partout dans l'application. */
class Entitlements(private val gateway: BillingGateway) {
    val tier get() = gateway.tier.value
    val isPremium get() = tier == Tier.PREMIUM
    val gatewayInfo get() = gateway

    /** Les fonctions essentielles (lecteur, sons, rituel, bibliothèque classique) ne sont jamais bloquées. */
    fun canPlay(premiumContent: Boolean) = !premiumContent || isPremium
    fun dailyGenerationLimit(): Int = if (isPremium) Int.MAX_VALUE else FREE_GENERATIONS_PER_DAY
    fun maxGenerationMinutes(): Int = if (isPremium) 60 else 20
    fun maxMixerLayers(): Int = if (isPremium) 6 else 3

    companion object {
        const val FREE_GENERATIONS_PER_DAY = 3
    }
}
