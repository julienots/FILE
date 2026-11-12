package com.superessence.essabank.core.engine

/**
 * Génère des identifiants FICTIFS et volontairement inutilisables.
 *
 * - Numéros de carte : 16 chiffres commençant par « 0 » (préfixe d'émetteur non attribué
 *   par la norme ISO/IEC 7812) et dont la clé de Luhn est volontairement fausse. Ils sont
 *   donc rejetés par tout réseau de paiement réel.
 * - Références de compte : format « ESSA-SIM-XXXX-XXXX », sans rapport avec un IBAN.
 */
object FictionalIdentifiers {

    fun cardNumber(ids: IdGenerator): String {
        val body = "0" + ids.digits(14)
        val valid = luhnCheckDigit(body)
        // On choisit délibérément un chiffre de contrôle différent du chiffre valide.
        val invalid = (valid + 1 + ids.nextInt(8)) % 10
        return body + invalid
    }

    fun cvv(ids: IdGenerator): String = ids.digits(3)

    fun accountReference(ids: IdGenerator): String =
        "ESSA-SIM-" + ids.digits(4) + "-" + ids.digits(4)

    fun beneficiaryReference(ids: IdGenerator): String =
        "ESSA-SIM-" + ids.digits(4) + "-" + ids.digits(4) + "-" + ids.digits(2)

    fun clientNumber(ids: IdGenerator): String = "SIM" + ids.digits(8)

    fun receiptReference(ids: IdGenerator): String = "ESB-" + ids.digits(4) + "-" + ids.digits(6)

    /** Une référence bénéficiaire acceptée : « ESSA-SIM- » suivi de chiffres et de tirets. */
    fun isValidBeneficiaryReference(ref: String): Boolean =
        Regex("""^ESSA-SIM-\d{4}-\d{4}(-\d{2})?$""").matches(ref.trim().uppercase())

    fun luhnCheckDigit(body: String): Int {
        var sum = 0
        // On double un chiffre sur deux en partant de la droite du corps (le chiffre de contrôle sera ajouté à droite).
        for ((i, ch) in body.reversed().withIndex()) {
            var d = ch - '0'
            if (i % 2 == 0) { d *= 2; if (d > 9) d -= 9 }
            sum += d
        }
        return (10 - sum % 10) % 10
    }

    fun isLuhnValid(number: String): Boolean =
        number.length >= 2 && luhnCheckDigit(number.dropLast(1)) == number.last() - '0'
}
