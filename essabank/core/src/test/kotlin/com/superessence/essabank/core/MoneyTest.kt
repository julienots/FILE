package com.superessence.essabank.core

import com.superessence.essabank.core.money.AppLanguage
import com.superessence.essabank.core.money.DisplayCurrency
import com.superessence.essabank.core.money.Money
import com.superessence.essabank.core.money.MoneyFormat
import com.superessence.essabank.core.money.ParseError
import com.superessence.essabank.core.money.ParseResult
import org.junit.Test
import java.math.BigDecimal
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith

class MoneyTest {

    private fun parse(s: String) = (Money.parse(s) as? ParseResult.Valid)?.money
    private fun parseErr(s: String) = (Money.parse(s) as? ParseResult.Invalid)?.error

    @Test fun `saisies valides`() {
        assertEquals(Money(123456), parse("1 234,56"))
        assertEquals(Money(123456), parse("1234.56"))
        assertEquals(Money(123450), parse("1234,5"))
        assertEquals(Money(1200), parse("12"))
        assertEquals(Money(123456789), parse("1.234.567,89"))
        assertEquals(Money(123456789), parse("1,234,567.89"))
        assertEquals(Money(5), parse("0,05"))
        assertEquals(Money(1200), parse("12,"))
    }

    @Test fun `saisies invalides`() {
        assertEquals(ParseError.EMPTY, parseErr("  "))
        assertEquals(ParseError.NEGATIVE, parseErr("-5"))
        assertEquals(ParseError.TOO_MANY_DECIMALS, parseErr("1,234"))
        assertEquals(ParseError.ZERO, parseErr("0,00"))
        assertEquals(ParseError.FORMAT, parseErr("12a"))
        assertEquals(ParseError.FORMAT, parseErr("1,2,3"))
        assertEquals(ParseError.TOO_LARGE, parseErr("10000000,01"))
    }

    @Test fun `arithmetique exacte et debordement`() {
        assertEquals(Money(30), Money(10) + Money(20))
        // 0,1 + 0,2 = 0,3 exactement (pas d'erreur binaire)
        assertEquals(Money.of("0.30"), Money.of("0.10") + Money.of("0.20"))
        assertFailsWith<ArithmeticException> { Money(Long.MAX_VALUE) + Money(1) }
    }

    @Test fun `arrondi bancaire au centieme`() {
        assertEquals(Money(2), Money.of(BigDecimal("0.025"))) // demi vers le pair
        assertEquals(Money(4), Money.of(BigDecimal("0.035")))
        assertEquals(Money(333), Money(1000).times(BigDecimal("0.333333")))
    }

    @Test fun `formatage francais et anglais`() {
        assertEquals("4 850,75 UV", MoneyFormat.format(Money.of("4850.75")))
        assertEquals("4,850.75 UV", MoneyFormat.format(Money.of("4850.75"), language = AppLanguage.EN))
        assertEquals("−12,50 UV", MoneyFormat.format(Money.of("-12.50")))
        assertEquals("+12,50 UV", MoneyFormat.format(Money.of("12.50"), signed = true))
        assertEquals("\$v5,238.81", MoneyFormat.format(Money.of("4850.75"), DisplayCurrency.USD_V, AppLanguage.EN))
        assertEquals("•••••• UV", MoneyFormat.format(Money.of("4850.75"), masked = true))
        assertEquals("4 850" to ",75", MoneyFormat.split(Money.of("4850.75"), DisplayCurrency.UV, AppLanguage.FR))
    }
}
