package com.superessence.essabank.ui.screens

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.LazyRow
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.AccountBalance
import androidx.compose.material.icons.rounded.Close
import androidx.compose.material.icons.rounded.PictureAsPdf
import androidx.compose.material.icons.rounded.Search
import androidx.compose.material.icons.automirrored.rounded.Sort
import androidx.compose.material3.Icon
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.superessence.essabank.core.model.Category
import com.superessence.essabank.core.model.Transaction
import com.superessence.essabank.core.model.TxKind
import com.superessence.essabank.core.money.sumOfMoney
import com.superessence.essabank.core.pdf.Reports
import com.superessence.essabank.ui.AppViewModel
import com.superessence.essabank.ui.Nav
import com.superessence.essabank.ui.Routes
import com.superessence.essabank.ui.components.Chip
import com.superessence.essabank.ui.components.EmptyState
import com.superessence.essabank.ui.components.EssaSheet
import com.superessence.essabank.ui.components.EssaTextField
import com.superessence.essabank.ui.components.IconBadge
import com.superessence.essabank.ui.components.KeyValue
import com.superessence.essabank.ui.components.Panel
import com.superessence.essabank.ui.components.PrimaryButton
import com.superessence.essabank.ui.components.RoundIconButton
import com.superessence.essabank.ui.components.SecondaryButton
import com.superessence.essabank.ui.components.SectionHeader
import com.superessence.essabank.ui.components.SimulationPill
import com.superessence.essabank.ui.components.TopBar
import com.superessence.essabank.ui.components.fmt
import com.superessence.essabank.ui.i18n.L
import com.superessence.essabank.ui.i18n.LocalLang
import com.superessence.essabank.ui.i18n.tr
import com.superessence.essabank.ui.theme.Brand
import com.superessence.essabank.ui.theme.Dimens
import com.superessence.essabank.ui.theme.Essa
import com.superessence.essabank.ui.theme.EssaType
import java.text.Normalizer
import java.time.Instant
import java.time.LocalDate

private enum class TypeFilter { ALL, SPENDING, INCOME, TRANSFERS }
private enum class SortMode { DATE_DESC, DATE_ASC, AMOUNT_DESC, AMOUNT_ASC }

private fun normalize(s: String) = Normalizer.normalize(s.lowercase(), Normalizer.Form.NFD).replace(Regex("\\p{M}"), "")

@Composable
fun HistoryScreen(vm: AppViewModel, nav: Nav, accountFilter: String?, categoryFilter: String?) {
    val s = vm.state.collectAsStateWithLifecycle().value ?: return
    val c = Essa.colors
    val lang = LocalLang.current
    var query by remember { mutableStateOf("") }
    var type by remember { mutableStateOf(TypeFilter.ALL) }
    var account by remember { mutableStateOf(accountFilter) }
    var category by remember { mutableStateOf(categoryFilter?.let { runCatching { Category.valueOf(it) }.getOrNull() }) }
    var period by remember { mutableIntStateOf(3) }
    var sort by remember { mutableStateOf(SortMode.DATE_DESC) }
    var sortSheet by remember { mutableStateOf(false) }
    val today = LocalDate.now(vm.zone)
    val types = listOf(TypeFilter.ALL to tr("Tout", "All"), TypeFilter.SPENDING to tr("Dépenses", "Spending"), TypeFilter.INCOME to tr("Revenus", "Income"), TypeFilter.TRANSFERS to tr("Transferts", "Transfers"))
    val periods = listOf(7L to tr("7 jours", "7 days"), 30L to tr("30 jours", "30 days"), 90L to tr("90 jours", "90 days"), null to tr("Tout", "All"))
    val saver = rememberFileSaver("application/pdf") { ok -> vm.toast(if (ok) tr(lang, "Export PDF enregistré.", "PDF export saved.") else tr(lang, "Export annulé.", "Export cancelled."), !ok) }

    val filtered = remember(s, query, type, account, category, period, sort) {
        val q = normalize(query.trim())
        val minDay = periods[period].first?.let { today.minusDays(it - 1) }
        s.transactions.asSequence()
            .filter { account == null || it.accountId == account }
            .filter { category == null || it.category == category }
            .filter {
                when (type) {
                    TypeFilter.ALL -> true
                    TypeFilter.SPENDING -> it.kind.isSpending && it.isDebit
                    TypeFilter.INCOME -> it.kind.isIncome && !it.isDebit
                    TypeFilter.TRANSFERS -> it.category == Category.TRANSFERS
                }
            }
            .filter { minDay == null || !Instant.ofEpochMilli(it.timestamp).atZone(vm.zone).toLocalDate().isBefore(minDay) }
            .filter { q.isEmpty() || normalize(it.label).contains(q) || normalize(it.counterparty ?: "").contains(q) || normalize(it.note).contains(q) ||
                normalize(L.category(it.category, lang)).contains(q) || it.reference.lowercase().contains(q) || it.amount.toString().contains(q.replace(',', '.')) }
            .toList()
    }
    val sorted = when (sort) {
        SortMode.DATE_DESC -> filtered.sortedWith(compareByDescending<Transaction> { it.timestamp }.thenByDescending { it.seq })
        SortMode.DATE_ASC -> filtered.sortedWith(compareBy<Transaction> { it.timestamp }.thenBy { it.seq })
        SortMode.AMOUNT_DESC -> filtered.sortedByDescending { it.amount.cents }
        SortMode.AMOUNT_ASC -> filtered.sortedBy { it.amount.cents }
    }
    val totalIn = filtered.filter { !it.isDebit }.sumOfMoney { it.amount }
    val totalOut = filtered.filter { it.isDebit }.sumOfMoney { it.amount }

    Column(Modifier.fillMaxSize().background(c.background)) {
        TopBar(tr("Historique", "History"), nav::back) {
            RoundIconButton(Icons.AutoMirrored.Rounded.Sort, tr("Trier", "Sort"), { sortSheet = true })
            Spacer(Modifier.width(8.dp))
            RoundIconButton(Icons.Rounded.PictureAsPdf, tr("Exporter en PDF", "Export as PDF"), {
                val label = account?.let { s.account(it)?.name } ?: periods[period].second
                vm.statementPdf(sorted, label)?.let { saver("ESSABANK-historique-simulation.pdf", it) }
            })
            Spacer(Modifier.width(8.dp))
        }
        LazyColumn(
            Modifier.fillMaxSize(), contentPadding = PaddingValues(start = Dimens.screen, end = Dimens.screen, bottom = 40.dp),
            verticalArrangement = Arrangement.spacedBy(8.dp),
        ) {
            item {
                EssaTextField(query, { query = it }, tr("Rechercher (libellé, montant, référence…)", "Search (label, amount, reference…)"), leading = Icons.Rounded.Search, maxLength = 40)
            }
            item {
                LazyRow(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    items(types) { (t, l) -> Chip(l, type == t) { type = t } }
                    item { Spacer(Modifier.width(4.dp)) }
                    items(periods.size) { i -> Chip(periods[i].second, period == i) { period = i } }
                }
            }
            item {
                LazyRow(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    item { Chip(tr("Tous les comptes", "All accounts"), account == null, Icons.Rounded.AccountBalance) { account = null } }
                    items(s.accounts, key = { it.id }) { a -> Chip(a.name, account == a.id, L.icon(a.icon)) { account = if (account == a.id) null else a.id } }
                }
            }
            item {
                LazyRow(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    item { Chip(tr("Toutes catégories", "All categories"), category == null) { category = null } }
                    items(Category.entries.toList()) { cat -> Chip(L.category(cat, lang), category == cat, L.categoryIcon(cat)) { category = if (category == cat) null else cat } }
                }
            }
            item {
                Row(Modifier.fillMaxWidth().padding(top = 4.dp), verticalAlignment = Alignment.CenterVertically) {
                    Text(tr("${filtered.size} opération(s)", "${filtered.size} operation(s)"), style = EssaType.small, color = c.textMuted, modifier = Modifier.weight(1f))
                    Text(fmt(totalIn, signed = true), style = EssaType.small, color = c.positive)
                    Spacer(Modifier.width(10.dp))
                    Text(fmt(-totalOut), style = EssaType.small, color = c.textSecondary)
                }
            }
            if (sorted.isEmpty()) {
                item {
                    EmptyState(Icons.Rounded.Search, tr("Aucun résultat", "No result"), tr("Modifiez la recherche ou les filtres.", "Change the search or filters."),
                        tr("Réinitialiser les filtres", "Reset filters")) { query = ""; type = TypeFilter.ALL; account = null; category = null; period = 3 }
                }
            } else if (sort == SortMode.DATE_DESC || sort == SortMode.DATE_ASC) {
                val groups = groupByDay(sorted, vm.zone).let { if (sort == SortMode.DATE_ASC) it.reversed() else it }
                for ((day, txs) in groups.take(120)) {
                    item(key = "h$day") {
                        val net = txs.sumOfMoney { it.signed }
                        DayHeader(day, today, fmt(net, signed = true))
                    }
                    item(key = "d$day") {
                        Panel(padding = 6.dp) { (if (sort == SortMode.DATE_ASC) txs.reversed() else txs).forEach { tx -> TransactionRow(tx, s, vm.zone, { nav.go(Routes.tx(tx.id)) }, showAccount = account == null) } }
                    }
                }
            } else {
                item { Panel(padding = 6.dp) { sorted.take(300).forEach { tx -> TransactionRow(tx, s, vm.zone, { nav.go(Routes.tx(tx.id)) }, showAccount = account == null) } } }
            }
        }
    }
    if (sortSheet) {
        EssaSheet({ sortSheet = false }) {
            Text(tr("Trier par", "Sort by"), style = EssaType.h2, color = c.text)
            Spacer(Modifier.height(12.dp))
            listOf(
                SortMode.DATE_DESC to tr("Date (récentes d'abord)", "Date (newest first)"), SortMode.DATE_ASC to tr("Date (anciennes d'abord)", "Date (oldest first)"),
                SortMode.AMOUNT_DESC to tr("Montant décroissant", "Amount (highest first)"), SortMode.AMOUNT_ASC to tr("Montant croissant", "Amount (lowest first)"),
            ).forEach { (m, l) ->
                Box(Modifier.fillMaxWidth().padding(vertical = 4.dp)) { Chip(l, sort == m) { sort = m; sortSheet = false } }
            }
        }
    }
}

@Composable
fun TransactionScreen(vm: AppViewModel, nav: Nav, id: String) {
    val s = vm.state.collectAsStateWithLifecycle().value ?: return
    val tx = s.transactions.firstOrNull { it.id == id }
    if (tx == null) { LaunchedEffect(Unit) { nav.back() }; return }
    val c = Essa.colors
    val lang = LocalLang.current
    val account = s.account(tx.accountId)
    val card = tx.cardId?.let { s.card(it) }
    val saver = rememberFileSaver("application/pdf") { ok -> vm.toast(if (ok) tr(lang, "Reçu PDF enregistré.", "PDF receipt saved.") else tr(lang, "Export annulé.", "Export cancelled."), !ok) }
    val other = tx.transferId?.let { tid -> s.transactions.firstOrNull { it.transferId == tid && it.id != tx.id } }
    com.superessence.essabank.ui.components.ScreenScaffold(tr("Détail de l'opération", "Operation details"), nav::back) {
        item {
            Panel {
                Column(Modifier.fillMaxWidth(), horizontalAlignment = Alignment.CenterHorizontally) {
                    IconBadge(L.categoryIcon(tx.category), L.categoryColor(tx.category), 64.dp)
                    Spacer(Modifier.height(12.dp))
                    Text(tx.label, style = EssaType.h3, color = c.text, textAlign = TextAlign.Center)
                    Text(Reports.kindLabel(tx.kind, lang), style = EssaType.small, color = c.textMuted)
                    Spacer(Modifier.height(10.dp))
                    Text(fmt(tx.signed, signed = true, hideable = false), style = EssaType.amountLarge.copy(fontSize = 36.sp), color = if (tx.isDebit) c.text else c.positive)
                    Spacer(Modifier.height(8.dp))
                    Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                        SimulationPill(text = statusShort(tx.status, lang).uppercase())
                        if (tx.isDebit && tx.kind.isSpending) SimulationPill(text = tr("DÉPENSE", "SPENDING"))
                        if (tx.isDemo) SimulationPill(text = tr("DÉMONSTRATION", "DEMO"))
                    }
                }
            }
        }
        item {
            Panel {
                KeyValue(tr("Date", "Date"), L.date(tx.timestamp, vm.zone, lang))
                KeyValue(tr("Heure", "Time"), L.time(tx.timestamp, vm.zone))
                KeyValue(tr("Catégorie", "Category"), L.category(tx.category, lang))
                KeyValue(tr("Compte", "Account"), account?.name ?: tr("Compte supprimé", "Deleted account"))
                tx.counterparty?.let { KeyValue(if (tx.isDebit) tr("Destinataire", "Recipient") else tr("Émetteur", "Sender"), it) }
                card?.let { KeyValue(tr("Carte", "Card"), it.label + " " + it.maskedNumber) }
                KeyValue(tr("Statut simulé", "Simulated status"), Reports.statusLabel(tx.status, lang))
                KeyValue(tr("Solde après opération", "Balance after"), fmt(tx.balanceAfter, hideable = false))
                if (tx.note.isNotBlank()) KeyValue(tr("Description", "Note"), tx.note)
                KeyValue(tr("Référence", "Reference"), tx.reference)
            }
        }
        if (other != null) item {
            Panel(onClick = { nav.go(Routes.tx(other.id)) }) {
                Text(tr("Écriture liée", "Linked entry"), style = EssaType.small, color = c.textMuted)
                Text((s.account(other.accountId)?.name ?: "—") + " · " + fmt(other.signed, signed = true), style = EssaType.bodyStrong, color = c.text)
            }
        }
        item {
            Box(Modifier.fillMaxWidth().clip(RoundedCornerShape(14.dp)).background(Brand.Electric).padding(12.dp), contentAlignment = Alignment.Center) {
                Text(Reports.simulationMention(lang), style = EssaType.caption.copy(fontSize = 12.sp), color = Color.White)
            }
        }
        item {
            PrimaryButton(tr("Exporter le reçu PDF", "Export PDF receipt"), { vm.receiptPdf(tx)?.let { saver("ESSABANK-recu-simulation-${tx.reference}.pdf", it) } }, icon = Icons.Rounded.PictureAsPdf)
        }
        if (account != null) item {
            SecondaryButton(tr("Voir le compte", "View account"), { nav.go(Routes.account(account.id)) }, Modifier.fillMaxWidth(), icon = Icons.Rounded.AccountBalance)
        }
        if (tx.kind == TxKind.TRANSFER_OUT && tx.counterparty != null) item {
            val ben = s.beneficiaries.firstOrNull { it.name == tx.counterparty }
            if (ben != null) SecondaryButton(tr("Renouveler ce virement", "Repeat this transfer"), { nav.go(Routes.transfer("beneficiary", to = ben.id)) }, Modifier.fillMaxWidth())
        }
    }
}
