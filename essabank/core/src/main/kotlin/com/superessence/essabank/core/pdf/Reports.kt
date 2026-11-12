package com.superessence.essabank.core.pdf

import com.superessence.essabank.core.engine.Labels
import com.superessence.essabank.core.model.BankState
import com.superessence.essabank.core.model.Transaction
import com.superessence.essabank.core.model.TxKind
import com.superessence.essabank.core.model.TxStatus
import com.superessence.essabank.core.money.AppLanguage
import com.superessence.essabank.core.money.MoneyFormat
import com.superessence.essabank.core.money.sumOfMoney
import java.time.Instant
import java.time.ZoneId
import java.time.format.DateTimeFormatter
import java.util.Locale

/**
 * Documents PDF de l'application. Chaque page porte la mention
 * « SIMULATION — AUCUN TRANSFERT RÉEL » et un filigrane : ces documents n'ont aucune valeur.
 */
object Reports {
    const val SIMULATION_FR = "SIMULATION — AUCUN TRANSFERT RÉEL"
    const val SIMULATION_EN = "SIMULATION — NO REAL TRANSFER"

    private val NAVY = Triple(10, 18, 48)
    private val BLUE = Triple(30, 92, 255)

    fun simulationMention(lang: AppLanguage) = if (lang == AppLanguage.FR) SIMULATION_FR else SIMULATION_EN

    /** Relevé PDF d'une liste de transactions. */
    fun statement(s: BankState, transactions: List<Transaction>, periodLabel: String, zone: ZoneId, generatedAt: Long): ByteArray {
        val prefs = s.profile.preferences
        val lang = prefs.language
        val fr = lang == AppLanguage.FR
        val pdf = PdfDocumentBuilder()
        val rows = transactions.sortedByDescending { it.timestamp }
        val dateFmt = DateTimeFormatter.ofPattern(if (fr) "dd/MM/yyyy HH:mm" else "MM/dd/yyyy HH:mm", if (fr) Locale.FRANCE else Locale.UK)
        val accountNames = s.accounts.associate { it.id to it.name }
        fun money(t: Transaction) = MoneyFormat.format(t.signed, prefs.currency, lang, signed = true)
        val rowHeight = 22f
        val top = 640f
        val bottom = 80f
        val perPage = ((top - bottom) / rowHeight).toInt()
        val chunks = if (rows.isEmpty()) listOf(emptyList()) else rows.chunked(perPage)
        val credits = rows.filter { !it.isDebit && it.status == TxStatus.COMPLETED }.sumOfMoney { it.amount }
        val debits = rows.filter { it.isDebit && it.status == TxStatus.COMPLETED }.sumOfMoney { it.amount }

        chunks.forEachIndexed { pageIndex, chunk ->
            pdf.page {
                watermark("SIMULATION")
                header(this, if (fr) "Relevé de transactions" else "Transaction statement", lang)
                text(40f, 730f, s.profile.fullName, 12f, bold = true)
                text(40f, 714f, (if (fr) "Client fictif n° " else "Fictional client no. ") + s.profile.clientNumber, 9f, r = 90, g = 100, b = 125)
                text(555f, 730f, periodLabel, 10f, bold = true, align = 2)
                text(555f, 714f, (if (fr) "Généré le " else "Generated on ") +
                    dateFmt.format(Instant.ofEpochMilli(generatedAt).atZone(zone)), 9f, r = 90, g = 100, b = 125, align = 2)
                if (pageIndex == 0) {
                    rect(40f, 664f, 515f, 36f, 238, 243, 255)
                    text(52f, 686f, if (fr) "Opérations" else "Operations", 8f, r = 90, g = 100, b = 125)
                    text(52f, 672f, rows.size.toString(), 11f, bold = true)
                    text(200f, 686f, if (fr) "Crédits" else "Credits", 8f, r = 90, g = 100, b = 125)
                    text(200f, 672f, MoneyFormat.format(credits, prefs.currency, lang, signed = true), 11f, bold = true, r = 10, g = 140, b = 90)
                    text(370f, 686f, if (fr) "Débits" else "Debits", 8f, r = 90, g = 100, b = 125)
                    text(370f, 672f, MoneyFormat.format(-debits, prefs.currency, lang), 11f, bold = true, r = 200, g = 50, b = 60)
                }
                // En-tête du tableau
                val hy = 650f
                text(40f, hy, "Date", 8f, bold = true, r = 90, g = 100, b = 125)
                text(130f, hy, if (fr) "Libellé" else "Description", 8f, bold = true, r = 90, g = 100, b = 125)
                text(300f, hy, if (fr) "Catégorie" else "Category", 8f, bold = true, r = 90, g = 100, b = 125)
                text(385f, hy, if (fr) "Compte" else "Account", 8f, bold = true, r = 90, g = 100, b = 125)
                text(490f, hy, if (fr) "Montant" else "Amount", 8f, bold = true, r = 90, g = 100, b = 125, align = 2)
                text(555f, hy, if (fr) "Solde après" else "Balance", 8f, bold = true, r = 90, g = 100, b = 125, align = 2)
                line(40f, hy - 6f, 555f, hy - 6f)
                var y = hy - 22f
                if (chunk.isEmpty()) text(297f, y, if (fr) "Aucune transaction sur la période." else "No transaction for this period.", 10f, align = 1)
                for (t in chunk) {
                    text(40f, y, dateFmt.format(Instant.ofEpochMilli(t.timestamp).atZone(zone)), 8f)
                    text(130f, y, ellipsize(t.label, 8.5f, 165f, bold = true), 8.5f, bold = true)
                    text(300f, y, ellipsize(Labels.category(t.category, lang), 8f, 80f), 8f, r = 90, g = 100, b = 125)
                    text(385f, y, ellipsize(accountNames[t.accountId] ?: "—", 8f, 70f), 8f, r = 90, g = 100, b = 125)
                    val (r, g, b) = if (t.isDebit) Triple(20, 28, 48) else Triple(10, 140, 90)
                    text(490f, y, money(t), 8.5f, bold = true, r = r, g = g, b = b, align = 2)
                    text(555f, y, MoneyFormat.format(t.balanceAfter, prefs.currency, lang), 8f, align = 2)
                    line(40f, y - 8f, 555f, y - 8f, 235, 238, 245, 0.4f)
                    y -= rowHeight
                }
                footer(this, lang, pageIndex + 1, chunks.size)
            }
        }
        return pdf.build(if (fr) "Relevé ESSABANK (simulation)" else "ESSABANK statement (simulation)")
    }

    /** Reçu PDF d'une opération (virement, paiement…). */
    fun receipt(s: BankState, tx: Transaction, zone: ZoneId): ByteArray {
        val prefs = s.profile.preferences
        val lang = prefs.language
        val fr = lang == AppLanguage.FR
        val dateFmt = DateTimeFormatter.ofPattern(if (fr) "EEEE d MMMM yyyy 'à' HH:mm" else "EEEE, MMMM d, yyyy 'at' HH:mm", if (fr) Locale.FRANCE else Locale.UK)
        val pdf = PdfDocumentBuilder()
        pdf.page {
            watermark("SIMULATION")
            header(this, if (fr) "Reçu de transaction" else "Transaction receipt", lang)
            rect(40f, 600f, 515f, 110f, 238, 243, 255)
            text(297f, 680f, kindLabel(tx.kind, lang), 11f, r = 90, g = 100, b = 125, align = 1)
            text(297f, 648f, MoneyFormat.format(tx.signed, prefs.currency, lang, signed = true), 28f, bold = true, align = 1,
                r = if (tx.isDebit) NAVY.first else 10, g = if (tx.isDebit) NAVY.second else 140, b = if (tx.isDebit) NAVY.third else 90)
            text(297f, 618f, dateFmt.format(Instant.ofEpochMilli(tx.timestamp).atZone(zone)), 10f, align = 1, r = 90, g = 100, b = 125)
            val account = s.account(tx.accountId)
            val lines = buildList {
                add((if (fr) "Libellé" else "Description") to tx.label)
                tx.counterparty?.let { add((if (fr) (if (tx.isDebit) "Destinataire" else "Émetteur") else (if (tx.isDebit) "Recipient" else "Sender")) to it) }
                add((if (fr) "Compte" else "Account") to ((account?.name ?: "—") + (account?.let { "  ·  " + it.reference } ?: "")))
                add((if (fr) "Catégorie" else "Category") to Labels.category(tx.category, lang))
                add((if (fr) "Statut" else "Status") to statusLabel(tx.status, lang))
                add((if (fr) "Solde après opération" else "Balance after") to MoneyFormat.format(tx.balanceAfter, prefs.currency, lang))
                if (tx.note.isNotBlank()) add((if (fr) "Description" else "Note") to tx.note)
                add((if (fr) "Référence" else "Reference") to tx.reference)
                tx.cardId?.let { id -> s.card(id)?.let { add((if (fr) "Carte" else "Card") to "${it.label} ${it.maskedNumber}") } }
            }
            var y = 570f
            for ((k, v) in lines) {
                text(40f, y, k, 9f, r = 90, g = 100, b = 125)
                text(555f, y, ellipsize(v, 10f, 330f, bold = true), 10f, bold = true, align = 2)
                line(40f, y - 9f, 555f, y - 9f, 230, 234, 242, 0.4f)
                y -= 28f
            }
            // Tampon « SIMULATION »
            val mention = simulationMention(lang)
            rect(40f, y - 40f, 515f, 44f, BLUE.first, BLUE.second, BLUE.third)
            text(297f, y - 24f, mention, 14f, bold = true, r = 255, g = 255, b = 255, align = 1)
            text(297f, y - 62f, if (fr) "Ce reçu est fictif. Aucune somme réelle n'a été débitée, transférée ou reçue."
                else "This receipt is fictional. No real money was debited, transferred or received.", 9f, align = 1, r = 90, g = 100, b = 125)
            footer(this, lang, 1, 1)
        }
        return pdf.build(if (fr) "Reçu ESSABANK (simulation)" else "ESSABANK receipt (simulation)")
    }

    fun kindLabel(kind: TxKind, lang: AppLanguage): String {
        val fr = lang == AppLanguage.FR
        return when (kind) {
            TxKind.CARD_PAYMENT -> if (fr) "Paiement par carte virtuelle" else "Virtual card payment"
            TxKind.PAYMENT -> if (fr) "Prélèvement simulé" else "Simulated direct debit"
            TxKind.INTERNAL_TRANSFER -> if (fr) "Virement entre vos comptes" else "Transfer between your accounts"
            TxKind.TRANSFER_OUT -> if (fr) "Virement simulé envoyé" else "Simulated transfer sent"
            TxKind.TRANSFER_IN -> if (fr) "Virement simulé reçu" else "Simulated transfer received"
            TxKind.INCOME -> if (fr) "Revenu virtuel" else "Virtual income"
            TxKind.SAVINGS_DEPOSIT -> if (fr) "Dépôt dans un coffre" else "Vault deposit"
            TxKind.SAVINGS_WITHDRAWAL -> if (fr) "Retrait d'un coffre" else "Vault withdrawal"
            TxKind.INVEST_BUY -> if (fr) "Achat virtuel" else "Virtual purchase"
            TxKind.INVEST_SELL -> if (fr) "Vente virtuelle" else "Virtual sale"
            TxKind.INTEREST -> if (fr) "Intérêts fictifs" else "Fictional interest"
            TxKind.OPENING -> if (fr) "Solde d'ouverture" else "Opening balance"
        }
    }

    fun statusLabel(status: TxStatus, lang: AppLanguage): String {
        val fr = lang == AppLanguage.FR
        return when (status) {
            TxStatus.COMPLETED -> if (fr) "Exécutée (simulation)" else "Completed (simulation)"
            TxStatus.PENDING -> if (fr) "En attente (simulation)" else "Pending (simulation)"
            TxStatus.DECLINED -> if (fr) "Refusée (simulation)" else "Declined (simulation)"
        }
    }

    private fun header(p: PdfPage, title: String, lang: AppLanguage) = with(p) {
        rect(0f, 762f, width, 80f, NAVY.first, NAVY.second, NAVY.third)
        rect(0f, 758f, width, 4f, BLUE.first, BLUE.second, BLUE.third)
        text(40f, 806f, "ESSABANK", 20f, bold = true, r = 255, g = 255, b = 255)
        text(40f, 788f, "Your Money. Your World.", 9f, r = 150, g = 175, b = 255)
        text(555f, 806f, title, 13f, bold = true, r = 255, g = 255, b = 255, align = 2)
        text(555f, 788f, simulationMention(lang), 9f, bold = true, r = 150, g = 175, b = 255, align = 2)
    }

    private fun footer(p: PdfPage, lang: AppLanguage, page: Int, total: Int) = with(p) {
        val fr = lang == AppLanguage.FR
        line(40f, 52f, 555f, 52f)
        text(40f, 38f, if (fr) "Document fictif généré par ESSABANK, application de simulation éditée par Superessence Studio."
            else "Fictional document generated by ESSABANK, a simulation app published by Superessence Studio.", 7.5f, r = 90, g = 100, b = 125)
        text(40f, 27f, if (fr) "ESSABANK n'est pas un établissement bancaire. Unités virtuelles sans valeur monétaire réelle."
            else "ESSABANK is not a bank. Virtual units have no real monetary value.", 7.5f, r = 90, g = 100, b = 125)
        text(555f, 38f, "$page / $total", 8f, bold = true, align = 2)
        text(555f, 27f, simulationMention(lang), 7.5f, bold = true, r = BLUE.first, g = BLUE.second, b = BLUE.third, align = 2)
    }
}
