package com.superessence.essabank.ui.screens

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ExperimentalLayoutApi
import androidx.compose.foundation.layout.FlowRow
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.rounded.ReceiptLong
import androidx.compose.material.icons.rounded.Add
import androidx.compose.material.icons.rounded.ContentCopy
import androidx.compose.material.icons.rounded.CheckCircle
import androidx.compose.material.icons.rounded.Delete
import androidx.compose.material.icons.rounded.Edit
import androidx.compose.material.icons.rounded.PictureAsPdf
import androidx.compose.material.icons.rounded.Savings
import androidx.compose.material.icons.rounded.SwapHoriz
import androidx.compose.material.icons.rounded.Visibility
import androidx.compose.material3.Icon
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalClipboardManager
import androidx.compose.ui.text.AnnotatedString
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.superessence.essabank.core.engine.EngineResult
import com.superessence.essabank.core.model.Account
import com.superessence.essabank.core.model.AccountType
import com.superessence.essabank.ui.AppViewModel
import com.superessence.essabank.ui.Nav
import com.superessence.essabank.ui.Routes
import com.superessence.essabank.ui.components.AccountGlyph
import com.superessence.essabank.ui.components.AnimatedBalance
import com.superessence.essabank.ui.components.BarChart
import com.superessence.essabank.ui.components.ColorDot
import com.superessence.essabank.ui.components.ConfirmDialog
import com.superessence.essabank.ui.components.DonutChart
import com.superessence.essabank.ui.components.EssaTextField
import com.superessence.essabank.ui.components.IconBadge
import com.superessence.essabank.ui.components.LegendDot
import com.superessence.essabank.ui.components.LineChart
import com.superessence.essabank.ui.components.ListRow
import com.superessence.essabank.ui.components.Panel
import com.superessence.essabank.ui.components.PrimaryButton
import com.superessence.essabank.ui.components.QuickAction
import com.superessence.essabank.ui.components.RoundIconButton
import com.superessence.essabank.ui.components.ScreenScaffold
import com.superessence.essabank.ui.components.SectionHeader
import com.superessence.essabank.ui.components.SimulationPill
import com.superessence.essabank.ui.components.StatTile
import com.superessence.essabank.ui.components.ToggleRow
import com.superessence.essabank.ui.components.fmt
import com.superessence.essabank.ui.components.pressable
import com.superessence.essabank.ui.i18n.L
import com.superessence.essabank.ui.i18n.LocalLang
import com.superessence.essabank.ui.i18n.tr
import com.superessence.essabank.ui.theme.Brand
import com.superessence.essabank.ui.theme.Essa
import com.superessence.essabank.ui.theme.EssaType
import com.superessence.essabank.core.money.sumOfMoney
import java.time.LocalDate

@Composable
fun AccountsScreen(vm: AppViewModel, nav: Nav) {
    val s = vm.state.collectAsStateWithLifecycle().value ?: return
    val c = Essa.colors
    val lang = LocalLang.current
    val accounts = s.accounts.sortedBy { it.sortOrder }
    val total = accounts.filter { it.includeInTotal }.sumOfMoney { it.balance }
    var selected by remember { mutableStateOf<Int?>(null) }
    val positive = accounts.filter { it.balance.isPositive && it.includeInTotal }
    ScreenScaffold(tr("Mes comptes", "My accounts"), nav::back, actions = {
        RoundIconButton(Icons.Rounded.Add, tr("Nouveau compte", "New account"), { nav.go(Routes.NEW_ACCOUNT) })
    }) {
        item {
            Panel {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    DonutChart(positive.map { it.balance.cents.toFloat() to Brand.color(it.colorIndex) }, 132.dp, selected = selected) {
                        Column(horizontalAlignment = Alignment.CenterHorizontally) {
                            Text(tr("Total", "Total"), style = EssaType.small, color = c.textMuted)
                            Text(fmt(selected?.let { positive.getOrNull(it)?.balance } ?: total, decimals = false), style = EssaType.bodyStrong, color = c.text)
                        }
                    }
                    Spacer(Modifier.width(16.dp))
                    Column(Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                        positive.forEachIndexed { i, a ->
                            Box(Modifier.clip(RoundedCornerShape(8.dp)).pressable { selected = if (selected == i) null else i }) {
                                LegendDot(Brand.color(a.colorIndex), a.name)
                            }
                        }
                    }
                }
                Spacer(Modifier.height(10.dp))
                SimulationPill(text = tr("SOLDES VIRTUELS · AUCUNE VALEUR RÉELLE", "VIRTUAL BALANCES · NO REAL VALUE"))
            }
        }
        items(accounts, key = { it.id }) { a ->
            Panel(padding = 8.dp, onClick = { nav.go(Routes.account(a.id)) }) {
                AccountRow(a, null)
                if (!a.includeInTotal) Text(tr("Exclu du total", "Excluded from total"), style = EssaType.small, color = c.textMuted, modifier = Modifier.padding(start = 64.dp, bottom = 6.dp))
            }
        }
        item {
            PrimaryButton(tr("Transférer entre mes comptes", "Move money between my accounts"), { nav.go(Routes.transfer("internal")) }, icon = Icons.Rounded.SwapHoriz)
        }
    }
}

@Composable
fun AccountDetailScreen(vm: AppViewModel, nav: Nav, id: String) {
    val s = vm.state.collectAsStateWithLifecycle().value ?: return
    val account = s.account(id)
    if (account == null) { androidx.compose.runtime.LaunchedEffect(Unit) { nav.back() }; return }
    val c = Essa.colors
    val lang = LocalLang.current
    val clipboard = LocalClipboardManager.current
    var confirmDelete by remember { mutableStateOf(false) }
    var sel by remember { mutableStateOf<Int?>(null) }
    val today = LocalDate.now(vm.zone)
    val series = remember(s, id) { vm.analytics.accountBalanceSeries(s, id, 60) }
    val monthly = remember(s, id) { vm.analytics.monthlyFlows(s, 6, id) }
    val flow = remember(s, id) { vm.analytics.flow(s, today.withDayOfMonth(1), today, id) }
    val txs = remember(s, id) { s.transactionsFor(id) }
    val saver = rememberFileSaver("application/pdf") { ok -> vm.toast(if (ok) tr(lang, "Relevé PDF enregistré.", "PDF statement saved.") else tr(lang, "Export annulé.", "Export cancelled."), !ok) }
    val col = Brand.color(account.colorIndex)

    ScreenScaffold(account.name, nav::back, actions = {
        RoundIconButton(Icons.Rounded.Edit, tr("Modifier", "Edit"), { nav.go(Routes.editAccount(id)) })
    }) {
        item {
            Panel {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    AccountGlyph(account.icon, account.colorIndex, 48.dp)
                    Spacer(Modifier.width(12.dp))
                    Column(Modifier.weight(1f)) {
                        Text(L.accountType(account.type, lang), style = EssaType.small, color = c.textMuted)
                        Row(verticalAlignment = Alignment.CenterVertically) {
                            Text(account.reference, style = EssaType.bodyStrong, color = c.textSecondary)
                            Spacer(Modifier.width(6.dp))
                            Icon(Icons.Rounded.ContentCopy, tr("Copier", "Copy"), tint = c.textMuted, modifier = Modifier.size(16.dp).pressable {
                                clipboard.setText(AnnotatedString(account.reference)); vm.toast(tr(lang, "Référence fictive copiée.", "Fictional reference copied."))
                            })
                        }
                    }
                    if (account.isPrimary) SimulationPill(text = tr("PRINCIPAL", "MAIN"))
                }
                Spacer(Modifier.height(16.dp))
                AnimatedBalance(account.balance, color = c.text, style = EssaType.amountLarge.copy(fontSize = 34.sp))
                if (account.overdraft.isPositive) Text(tr("Découvert virtuel autorisé : ", "Virtual overdraft allowed: ") + fmt(account.overdraft), style = EssaType.small, color = c.textMuted)
                if (account.interestBp > 0) Text(tr("Rémunération fictive : ", "Fictional interest: ") + "%.2f %%".format(account.interestBp / 100.0), style = EssaType.small, color = c.positive)
                Spacer(Modifier.height(12.dp))
                LineChart(series.map { it.amount.cents.toFloat() }, col, height = 120.dp, selected = sel, onSelect = { sel = it })
                Text(sel?.let { series.getOrNull(it) }?.let { L.date(it.date, lang) + " · " + fmt(it.amount) } ?: tr("Solde sur 60 jours", "Balance over 60 days"),
                    style = EssaType.small, color = c.textMuted)
            }
        }
        item {
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
                QuickAction(Icons.Rounded.SwapHoriz, tr("Virer", "Transfer"), { nav.go(Routes.transfer("internal", from = id)) }, Modifier.weight(1f), highlight = true)
                QuickAction(Icons.Rounded.Savings, tr("Alimenter", "Top up"), { nav.go(Routes.transfer("internal", to = id)) }, Modifier.weight(1f))
                QuickAction(Icons.AutoMirrored.Rounded.ReceiptLong, tr("Historique", "History"), { nav.go(Routes.history(account = id)) }, Modifier.weight(1f))
                QuickAction(Icons.Rounded.PictureAsPdf, tr("Relevé", "Statement"), {
                    val bytes = vm.statementPdf(txs.filter { !java.time.Instant.ofEpochMilli(it.timestamp).atZone(vm.zone).toLocalDate().isBefore(today.minusDays(90)) },
                        tr(lang, "90 derniers jours", "Last 90 days"))
                    if (bytes != null) saver("ESSABANK-releve-simulation-${account.name.replace(' ', '-')}.pdf", bytes)
                }, Modifier.weight(1f))
            }
        }
        item {
            Row(horizontalArrangement = Arrangement.spacedBy(12.dp)) {
                StatTile(tr("Entrées du mois", "In this month"), fmt(flow.income, signed = true), Modifier.weight(1f), valueColor = c.positive)
                StatTile(tr("Sorties du mois", "Out this month"), fmt(-flow.spending), Modifier.weight(1f))
            }
        }
        item {
            Panel {
                Text(tr("Flux mensuels", "Monthly flows"), style = EssaType.title, color = c.text)
                Spacer(Modifier.height(12.dp))
                BarChart(monthly.map { it.spending.cents.toFloat() }, monthly.map { L.monthShort(it.start, lang) }, c.accent,
                    secondary = monthly.map { it.income.cents.toFloat() })
                Spacer(Modifier.height(8.dp))
                Row(horizontalArrangement = Arrangement.spacedBy(16.dp)) {
                    LegendDot(c.accent, tr("Dépenses", "Spending")); LegendDot(c.positive, tr("Revenus", "Income"))
                }
            }
        }
        item {
            SectionHeader(tr("Dernières opérations", "Latest operations"), tr("Tout voir", "See all")) { nav.go(Routes.history(account = id)) }
        }
        item {
            Panel(padding = 8.dp) {
                val recent = txs.sortedWith(compareByDescending<com.superessence.essabank.core.model.Transaction> { it.timestamp }.thenByDescending { it.seq }).take(8)
                if (recent.isEmpty()) Text(tr("Aucune opération sur ce compte.", "No operation on this account."), style = EssaType.body, color = c.textMuted, modifier = Modifier.padding(12.dp))
                recent.forEach { TransactionRow(it, s, vm.zone, { nav.go(Routes.tx(it.id)) }) }
            }
        }
        item { SectionHeader(tr("Paramètres du compte", "Account settings")) }
        item {
            Panel(padding = 8.dp) {
                ToggleRow(tr("Inclure dans le solde total", "Include in total balance"), tr("Compte pris en compte dans le patrimoine affiché", "Counted in the displayed net worth"),
                    account.includeInTotal, Icons.Rounded.Visibility) { v -> vm.run { updateAccount(it, id, includeInTotal = v) } }
                ToggleRow(tr("Arrondi automatique", "Automatic round-up"), tr("Chaque paiement par carte est arrondi à l'unité supérieure ; la différence va dans votre premier coffre.", "Each card payment is rounded up; the difference goes to your first vault."),
                    account.roundUp, Icons.Rounded.Savings) { v -> vm.run { updateAccount(it, id, roundUp = v) } }
                ListRow(tr("Personnaliser", "Customize"), tr("Nom, couleur et icône", "Name, color and icon"), leading = { IconBadge(Icons.Rounded.Edit, c.accent, 38.dp) },
                    onClick = { nav.go(Routes.editAccount(id)) })
                if (!account.isPrimary) ListRow(tr("Supprimer le compte", "Delete account"), tr("Le solde doit être nul", "Balance must be zero"),
                    leading = { IconBadge(Icons.Rounded.Delete, c.negative, 38.dp) }, onClick = { confirmDelete = true }, titleColor = c.negative)
            }
        }
    }
    if (confirmDelete) {
        val deletedMsg = tr("Compte supprimé.", "Account deleted.")
        ConfirmDialog(tr("Supprimer ce compte ?", "Delete this account?"),
            tr("Ses cartes et virements programmés seront supprimés. Cette action est définitive.", "Its cards and scheduled transfers will be deleted. This cannot be undone."),
            tr("Supprimer", "Delete"), danger = true,
            onConfirm = { vm.run(success = deletedMsg, onOk = { nav.back() }) { deleteAccount(it, id) } }, onDismiss = { confirmDelete = false })
    }
}


@OptIn(ExperimentalLayoutApi::class)
@Composable
fun AccountEditorScreen(vm: AppViewModel, nav: Nav, id: String?) {
    val s = vm.state.collectAsStateWithLifecycle().value ?: return
    val existing: Account? = id?.let { s.account(it) }
    val c = Essa.colors
    val lang = LocalLang.current
    var type by remember { mutableStateOf(existing?.type ?: AccountType.PROJECT) }
    var name by remember { mutableStateOf(existing?.name ?: L.accountType(AccountType.PROJECT, lang)) }
    var nameEdited by remember { mutableStateOf(existing != null) }
    var color by remember { mutableIntStateOf(existing?.colorIndex ?: 2) }
    var icon by remember { mutableStateOf(existing?.icon ?: L.defaultIcon(AccountType.PROJECT)) }
    var busy by remember { mutableStateOf(false) }
    val createdMsg = tr("Compte virtuel créé.", "Virtual account created.")
    val savedMsg = tr("Compte mis à jour.", "Account updated.")

    ScreenScaffold(if (existing == null) tr("Nouveau compte", "New account") else tr("Personnaliser le compte", "Customize account"), nav::back,
        bottomBar = {
            PrimaryButton(if (existing == null) tr("Ouvrir le compte virtuel", "Open the virtual account") else tr("Enregistrer", "Save"), loading = busy, enabled = name.isNotBlank(), onClick = {
                busy = true
                if (existing == null) vm.run(createdMsg, onOk = { busy = false; nav.back() }) { createAccount(it, type, name, color, icon) }.invokeOnCompletion { busy = false }
                else vm.run(savedMsg, onOk = { nav.back() }) { updateAccount(it, existing.id, name = name, colorIndex = color, icon = icon) }.invokeOnCompletion { busy = false }
            })
        }) {
        item {
            Panel {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    AccountGlyph(icon, color, 56.dp)
                    Spacer(Modifier.width(14.dp))
                    Column {
                        Text(name.ifBlank { "—" }, style = EssaType.h3, color = c.text)
                        Text(L.accountType(type, lang), style = EssaType.small, color = c.textMuted)
                    }
                }
            }
        }
        if (existing == null) {
            item { SectionHeader(tr("Type de compte", "Account type")) }
            items(AccountType.entries.toList()) { t ->
                val selected = t == type
                Box(Modifier.clip(RoundedCornerShape(16.dp)).background(if (selected) c.accentSoft else Color.Transparent)) {
                    ListRow(L.accountType(t, lang), typeHint(t, lang), leading = { IconBadge(L.icon(L.defaultIcon(t)), Brand.color(t.ordinal), 40.dp) },
                        onClick = {
                            type = t; icon = L.defaultIcon(t); color = t.ordinal
                            if (!nameEdited) name = L.accountType(t, lang)
                        }, chevron = false,
                        trailing = { if (selected) Icon(Icons.Rounded.CheckCircle, null, tint = c.accent) })
                }
            }
        }
        item { SectionHeader(tr("Nom", "Name")) }
        item { EssaTextField(name, { name = it; nameEdited = true }, tr("Nom du compte", "Account name"), maxLength = 32) }
        item { SectionHeader(tr("Couleur", "Color")) }
        item {
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
                Brand.palette.forEachIndexed { i, col -> ColorDot(col, i == color) { color = i } }
            }
        }
        item { SectionHeader(tr("Icône", "Icon")) }
        item {
            FlowRow(horizontalArrangement = Arrangement.spacedBy(10.dp), verticalArrangement = Arrangement.spacedBy(10.dp)) {
                L.accountIcons.forEach { key ->
                    Box(Modifier.clip(RoundedCornerShape(14.dp)).background(if (key == icon) c.accentSoft else Color.Transparent).pressable { icon = key }.padding(4.dp)) {
                        IconBadge(L.icon(key), if (key == icon) Brand.color(color) else c.textMuted, 44.dp)
                    }
                }
            }
        }
    }
}

private fun typeHint(t: AccountType, lang: com.superessence.essabank.core.money.AppLanguage): String {
    val fr = lang == com.superessence.essabank.core.money.AppLanguage.FR
    return when (t) {
        AccountType.CURRENT -> if (fr) "Dépenses du quotidien" else "Everyday spending"
        AccountType.SAVINGS -> if (fr) "Rémunération fictive de 2,50 %" else "Fictional 2.50% interest"
        AccountType.PROJECT -> if (fr) "Mettez de côté pour un projet" else "Put money aside for a project"
        AccountType.VACATION -> if (fr) "Votre cagnotte de voyage" else "Your travel fund"
        AccountType.BUSINESS -> if (fr) "Activité indépendante simulée" else "Simulated freelance activity"
        AccountType.INVESTMENT -> if (fr) "Liquidités du marché de démonstration" else "Cash for the demo market"
    }
}
