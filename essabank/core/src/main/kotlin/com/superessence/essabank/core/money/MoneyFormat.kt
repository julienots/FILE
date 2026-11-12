package com.superessence.essabank.core.money

import kotlinx.serialization.Serializable
import java.math.BigDecimal
import java.math.RoundingMode

/**
 * Devise d'affichage. Le moteur compte en euros VIRTUELS (constante [UV], nom conservé
 * pour la compatibilité des données enregistrées) ; les autres devises sont des conversions
 * d'affichage à taux fictifs fixes. Aucune n'a de valeur réelle.
 */
@Serializable
enum class DisplayCurrency(
    val code: String,
    val symbol: String,
    /** Valeur d'un euro virtuel dans cette devise d'affichage (taux fictif fixe). */
    val rate: BigDecimal,
    val symbolBefore: Boolean,
) {
    /** Euro virtuel : devise de référence de l'application. */
    UV("EUR", "€", BigDecimal.ONE, false),
    /** Ancienne option, identique à l'euro virtuel : conservée pour relire les anciennes données, masquée dans les réglages. */
    EUR_V("EUR", "€", BigDecimal.ONE, false),
    USD_V("USD-V", "\$v", BigDecimal("1.08"), true),
    GBP_V("GBP-V", "£v", BigDecimal("0.86"), true),
    CHF_V("CHF-V", "CHFv", BigDecimal("0.95"), false);

    fun convert(money: Money): Money = money.times(rate)
}

@Serializable
enum class AppLanguage { FR, EN }

object MoneyFormat {
    const val MASK = "••••••"

    /**
     * Formate un montant : « 4 850,75 € » (FR) ou « 4,850.75 € » (EN).
     * @param signed ajoute « + » pour les montants positifs.
     */
    fun format(
        money: Money,
        currency: DisplayCurrency = DisplayCurrency.UV,
        language: AppLanguage = AppLanguage.FR,
        signed: Boolean = false,
        masked: Boolean = false,
        showSymbol: Boolean = true,
        decimals: Boolean = true,
    ): String {
        if (masked) return if (showSymbol) withSymbol(MASK, currency) else MASK
        val converted = currency.convert(money)
        val number = formatNumber(converted.toBigDecimal().abs(), language, if (decimals) 2 else 0)
        val sign = when {
            converted.isNegative -> "−"
            signed && converted.isPositive -> "+"
            else -> ""
        }
        val body = if (showSymbol) withSymbol(number, currency) else number
        return sign + body
    }

    /** Partie entière et partie décimale séparées (pour un affichage typographique du solde). */
    fun split(money: Money, currency: DisplayCurrency, language: AppLanguage): Pair<String, String> {
        val converted = currency.convert(money)
        val abs = converted.toBigDecimal().abs()
        val intPart = formatNumber(abs.setScale(0, RoundingMode.DOWN), language, 0)
        val dec = abs.remainder(BigDecimal.ONE).movePointRight(2).setScale(0, RoundingMode.DOWN).toInt()
        val sep = if (language == AppLanguage.FR) "," else "."
        val sign = if (converted.isNegative) "−" else ""
        return (sign + intPart) to (sep + dec.toString().padStart(2, '0'))
    }

    fun formatNumber(value: BigDecimal, language: AppLanguage, decimals: Int): String {
        val scaled = value.setScale(decimals, RoundingMode.HALF_EVEN)
        val plain = scaled.toPlainString()
        val neg = plain.startsWith("-")
        val digits = plain.removePrefix("-")
        val intPart = digits.substringBefore('.')
        val decPart = digits.substringAfter('.', "")
        val group = if (language == AppLanguage.FR) " " else ","
        val decSep = if (language == AppLanguage.FR) "," else "."
        val grouped = intPart.reversed().chunked(3).joinToString(group).reversed()
        return (if (neg) "-" else "") + grouped + if (decPart.isNotEmpty()) decSep + decPart else ""
    }

    /** Pourcentage avec signe : « +2,35 % ». */
    fun percent(value: BigDecimal, language: AppLanguage, signed: Boolean = true): String {
        val n = formatNumber(value.abs(), language, 2)
        val sign = when {
            value.signum() < 0 -> "−"
            signed && value.signum() > 0 -> "+"
            else -> ""
        }
        return if (language == AppLanguage.FR) "$sign$n %" else "$sign$n%"
    }

    private fun withSymbol(number: String, currency: DisplayCurrency): String =
        if (currency.symbolBefore) currency.symbol + number else number + " " + currency.symbol
}
