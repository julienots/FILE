package com.superessence.essabank.core.loan

import com.superessence.essabank.core.model.LoanType
import com.superessence.essabank.core.money.Money
import java.math.BigDecimal
import java.math.MathContext
import java.math.RoundingMode

/** Paramètres par défaut et bornes FICTIFS de chaque type de crédit simulé. */
data class LoanProfile(
    val type: LoanType,
    val defaultPrincipal: Money,
    val minPrincipal: Money,
    val maxPrincipal: Money,
    val defaultMonths: Int,
    val minMonths: Int,
    val maxMonths: Int,
    val defaultRateBp: Int,
    val minRateBp: Int,
    val maxRateBp: Int,
    val defaultInsuranceBp: Int,
) {
    companion object {
        fun of(type: LoanType): LoanProfile = when (type) {
            LoanType.PERSONAL -> LoanProfile(type, Money.units(8_000), Money.units(500), Money.units(75_000), 48, 6, 84, 590, 100, 1500, 30)
            LoanType.AUTO -> LoanProfile(type, Money.units(18_000), Money.units(3_000), Money.units(100_000), 60, 12, 96, 450, 100, 1200, 30)
            LoanType.HOME -> LoanProfile(type, Money.units(220_000), Money.units(30_000), Money.units(1_500_000), 300, 60, 360, 360, 50, 900, 25)
            LoanType.STUDENT -> LoanProfile(type, Money.units(15_000), Money.units(1_000), Money.units(75_000), 84, 12, 120, 190, 0, 800, 10)
        }
    }
}

data class AmortizationRow(
    val index: Int,
    val payment: Money,
    val interest: Money,
    val principal: Money,
    val insurance: Money,
    val remaining: Money,
)

data class LoanResult(
    val principal: Money,
    val rateBp: Int,
    val months: Int,
    /** Mensualité hors assurance. */
    val monthlyPayment: Money,
    val monthlyInsurance: Money,
    val totalInterest: Money,
    val totalInsurance: Money,
    /** Coût total du crédit = intérêts + assurance. */
    val totalCost: Money,
    /** Montant total remboursé = capital + coût total. */
    val totalRepaid: Money,
    val schedule: List<AmortizationRow>,
) {
    val monthlyTotal: Money get() = monthlyPayment + monthlyInsurance
}

/**
 * Simulateur de crédit à mensualités constantes (amortissement classique).
 * SIMULATION PÉDAGOGIQUE : aucun crédit n'est proposé ni accordé.
 */
object LoanCalculator {
    private val MC = MathContext(34, RoundingMode.HALF_EVEN)

    fun compute(principal: Money, rateBp: Int, months: Int, insuranceBp: Int = 0): LoanResult {
        require(principal.isPositive) { "Capital invalide" }
        require(months in 1..600) { "Durée invalide" }
        require(rateBp in 0..5000 && insuranceBp in 0..1000) { "Taux invalide" }

        val p = principal.toBigDecimal()
        val r = BigDecimal(rateBp).divide(BigDecimal(120_000), MC) // taux mensuel
        val payment: Money = if (rateBp == 0) {
            Money.of(p.divide(BigDecimal(months), MC), RoundingMode.HALF_UP)
        } else {
            val factor = BigDecimal.ONE.add(r).pow(months, MC)
            Money.of(p.multiply(r, MC).multiply(factor, MC).divide(factor.subtract(BigDecimal.ONE), MC), RoundingMode.HALF_UP)
        }
        val insurance = Money.of(p.multiply(BigDecimal(insuranceBp), MC).divide(BigDecimal(120_000), MC))

        val rows = ArrayList<AmortizationRow>(months)
        var remaining = principal
        var totalInterest = Money.ZERO
        for (i in 1..months) {
            val interest = Money.of(remaining.toBigDecimal().multiply(r, MC))
            var principalPart = payment - interest
            var pay = payment
            if (i == months || principalPart > remaining) {
                // Dernière échéance ajustée pour solder exactement le capital.
                principalPart = remaining
                pay = principalPart + interest
            }
            remaining -= principalPart
            totalInterest += interest
            rows += AmortizationRow(i, pay, interest, principalPart, insurance, remaining)
            if (remaining.isZero && i < months) {
                // Cas théorique (arrondis) : les échéances suivantes sont nulles.
                for (j in i + 1..months) rows += AmortizationRow(j, Money.ZERO, Money.ZERO, Money.ZERO, insurance, Money.ZERO)
                break
            }
        }
        val totalInsurance = insurance * months
        val totalCost = totalInterest + totalInsurance
        return LoanResult(
            principal = principal, rateBp = rateBp, months = months,
            monthlyPayment = payment, monthlyInsurance = insurance,
            totalInterest = totalInterest, totalInsurance = totalInsurance,
            totalCost = totalCost, totalRepaid = principal + totalCost, schedule = rows,
        )
    }
}
