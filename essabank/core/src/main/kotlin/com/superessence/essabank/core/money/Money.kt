package com.superessence.essabank.core.money

import kotlinx.serialization.Serializable
import java.math.BigDecimal
import java.math.RoundingMode

/**
 * Montant en unités virtuelles ESSABANK, stocké en centièmes (Long) pour éviter
 * toute erreur d'arrondi binaire. 1 unité virtuelle = 100 centièmes.
 *
 * Les montants n'ont aucune valeur monétaire réelle.
 */
@Serializable
@JvmInline
value class Money(val cents: Long) : Comparable<Money> {

    operator fun plus(other: Money): Money = Money(Math.addExact(cents, other.cents))
    operator fun minus(other: Money): Money = Money(Math.subtractExact(cents, other.cents))
    operator fun unaryMinus(): Money = Money(Math.negateExact(cents))
    operator fun times(factor: Int): Money = Money(Math.multiplyExact(cents, factor.toLong()))
    override operator fun compareTo(other: Money): Int = cents.compareTo(other.cents)

    val isZero: Boolean get() = cents == 0L
    val isPositive: Boolean get() = cents > 0L
    val isNegative: Boolean get() = cents < 0L
    fun abs(): Money = if (cents < 0) -this else this

    fun toBigDecimal(): BigDecimal = BigDecimal.valueOf(cents, 2)

    /** Multiplie par un décimal et arrondit au centième (arrondi bancaire). */
    fun times(factor: BigDecimal, mode: RoundingMode = RoundingMode.HALF_EVEN): Money =
        of(toBigDecimal().multiply(factor), mode)

    override fun toString(): String = toBigDecimal().toPlainString()

    companion object {
        val ZERO = Money(0)

        /** Plafond technique d'une opération : 10 millions d'unités virtuelles. */
        val MAX_OPERATION = units(10_000_000)

        fun units(units: Long): Money = Money(Math.multiplyExact(units, 100L))
        fun units(units: Int): Money = units(units.toLong())

        fun of(value: BigDecimal, mode: RoundingMode = RoundingMode.HALF_EVEN): Money =
            Money(value.setScale(2, mode).unscaledValue().longValueExact())

        fun of(value: String): Money = of(BigDecimal(value))

        fun sum(values: Iterable<Money>): Money = values.fold(ZERO) { acc, m -> acc + m }

        /**
         * Analyse une saisie utilisateur : « 1 234,56 », « 1234.5 », « 12 ».
         * Refuse les montants vides, négatifs, à plus de deux décimales ou hors plafond.
         */
        fun parse(input: String): ParseResult {
            val cleaned = input.trim()
                .replace(" ", "").replace(" ", "").replace(" ", "").replace("'", "")
            if (cleaned.isEmpty()) return ParseResult.Invalid(ParseError.EMPTY)
            if (cleaned.startsWith("-")) return ParseResult.Invalid(ParseError.NEGATIVE)
            val normalized = normalizeSeparators(cleaned) ?: return ParseResult.Invalid(ParseError.FORMAT)
            if (!Regex("""^\d+(\.\d*)?$""").matches(normalized)) return ParseResult.Invalid(ParseError.FORMAT)
            val decimals = normalized.substringAfter('.', "")
            if (decimals.length > 2) return ParseResult.Invalid(ParseError.TOO_MANY_DECIMALS)
            val bd = try { BigDecimal(normalized.trimEnd('.')) } catch (e: NumberFormatException) {
                return ParseResult.Invalid(ParseError.FORMAT)
            }
            if (bd > MAX_OPERATION.toBigDecimal()) return ParseResult.Invalid(ParseError.TOO_LARGE)
            val money = of(bd, RoundingMode.UNNECESSARY)
            if (money.isZero) return ParseResult.Invalid(ParseError.ZERO)
            return ParseResult.Valid(money)
        }

        /** Gère la virgule française, le point anglo-saxon et les séparateurs de milliers. */
        private fun normalizeSeparators(s: String): String? {
            val commas = s.count { it == ',' }
            val dots = s.count { it == '.' }
            return when {
                commas == 0 && dots <= 1 -> s
                dots == 0 && commas == 1 -> s.replace(',', '.')
                commas >= 1 && dots >= 1 -> {
                    // Le dernier séparateur est le séparateur décimal.
                    val lastComma = s.lastIndexOf(','); val lastDot = s.lastIndexOf('.')
                    if (lastComma > lastDot) {
                        if (s.substring(lastComma + 1).contains('.')) null
                        else s.replace(".", "").replace(',', '.').takeIf { it.count { c -> c == '.' } == 1 }
                    } else {
                        s.replace(",", "").takeIf { it.count { c -> c == '.' } == 1 }
                    }
                }
                commas > 1 && dots == 0 -> null
                else -> null
            }
        }
    }
}

enum class ParseError { EMPTY, NEGATIVE, FORMAT, TOO_MANY_DECIMALS, TOO_LARGE, ZERO }

sealed interface ParseResult {
    data class Valid(val money: Money) : ParseResult
    data class Invalid(val error: ParseError) : ParseResult
}

fun Iterable<Money>.sumMoney(): Money = Money.sum(this)

inline fun <T> Iterable<T>.sumOfMoney(selector: (T) -> Money): Money {
    var total = Money.ZERO
    for (e in this) total += selector(e)
    return total
}
