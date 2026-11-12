package com.superessence.essabank.ui.screens

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyRow
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.Add
import androidx.compose.material.icons.rounded.Autorenew
import androidx.compose.material.icons.rounded.CheckCircle
import androidx.compose.material.icons.rounded.Delete
import androidx.compose.material.icons.rounded.Edit
import androidx.compose.material.icons.rounded.EmojiEvents
import androidx.compose.material.icons.rounded.Remove
import androidx.compose.material.icons.rounded.Savings
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
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.superessence.essabank.core.model.AccountType
import com.superessence.essabank.core.model.Frequency
import com.superessence.essabank.core.model.GoalKind
import com.superessence.essabank.core.model.GoalMoveType
import com.superessence.essabank.core.model.SavingsGoal
import com.superessence.essabank.core.money.Money
import com.superessence.essabank.core.money.ParseResult
import com.superessence.essabank.core.money.sumOfMoney
import com.superessence.essabank.ui.AppViewModel
import com.superessence.essabank.ui.Nav
import com.superessence.essabank.ui.Routes
import com.superessence.essabank.ui.components.Chip
import com.superessence.essabank.ui.components.ColorDot
import com.superessence.essabank.ui.components.ConfirmDialog
import com.superessence.essabank.ui.components.EmptyState
import com.superessence.essabank.ui.components.EssaSheet
import com.superessence.essabank.ui.components.EssaTextField
import com.superessence.essabank.ui.components.GoalIllustration
import com.superessence.essabank.ui.components.IconBadge
import com.superessence.essabank.ui.components.KeyValue
import com.superessence.essabank.ui.components.ListRow
import com.superessence.essabank.ui.components.Panel
import com.superessence.essabank.ui.components.PrimaryButton
import com.superessence.essabank.ui.components.ProgressBar
import com.superessence.essabank.ui.components.ProgressRing
import com.superessence.essabank.ui.components.RoundIconButton
import com.superessence.essabank.ui.components.ScreenScaffold
import com.superessence.essabank.ui.components.SecondaryButton
import com.superessence.essabank.ui.components.SectionHeader
import com.superessence.essabank.ui.components.fmt
import com.superessence.essabank.ui.i18n.L
import com.superessence.essabank.ui.i18n.LocalLang
import com.superessence.essabank.ui.i18n.tr
import com.superessence.essabank.ui.theme.Brand
import com.superessence.essabank.ui.theme.Essa
import com.superessence.essabank.ui.theme.EssaType
import java.time.LocalDate
import java.time.temporal.ChronoUnit

@Composable
fun SavingsScreen(vm: AppViewModel, nav: Nav) {
    val s = vm.state.collectAsStateWithLifecycle().value ?: return
    val c = Essa.colors
    val lang = LocalLang.current
    val saved = s.goals.sumOfMoney { it.saved }
    val target = s.goals.sumOfMoney { it.target }
    val canCreate = s.goals.size < s.profile.tier.maxGoals
    val autoCount = s.goals.count { it.autoSave?.active == true }
    ScreenScaffold(tr("Épargne virtuelle", "Virtual savings"), nav::back, actions = {
        RoundIconButton(Icons.Rounded.Add, tr("Nouvel objectif", "New goal"), { if (canCreate) nav.go(Routes.NEW_GOAL) else nav.go(Routes.PLANS) })
    }) {
        item {
            Panel {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    ProgressRing(if (target.cents > 0) saved.cents.toFloat() / target.cents else 0f, c.positive, 108.dp) {
                        Text("${if (target.cents > 0) (saved.cents * 100 / target.cents) else 0} %", style = EssaType.h3, color = c.text)
                    }
                    Spacer(Modifier.width(18.dp))
                    Column {
                        Text(tr("Total dans les coffres", "Total in vaults"), style = EssaType.small, color = c.textMuted)
                        Text(fmt(saved), style = EssaType.h2, color = c.text)
                        Text(tr("sur ", "of ") + fmt(target, decimals = false) + tr(" d'objectifs", " in goals"), style = EssaType.small, color = c.textMuted)
                        Spacer(Modifier.height(6.dp))
                        Text(tr("$autoCount épargne(s) automatique(s) active(s)", "$autoCount active auto-save(s)"), style = EssaType.small, color = c.accent)
                    }
                }
            }
        }
        if (s.goals.isEmpty()) item {
            EmptyState(Icons.Rounded.Savings, tr("Aucun objectif", "No goal yet"), tr("Créez un coffre virtuel pour votre prochain projet.", "Create a virtual vault for your next project."), tr("Créer un objectif", "Create a goal")) { nav.go(Routes.NEW_GOAL) }
        }
        items(s.goals.sortedBy { it.sortOrder }, key = { it.id }) { g -> GoalCard(g, lang) { nav.go(Routes.goal(g.id)) } }
        if (!canCreate) item {
            Panel(onClick = { nav.go(Routes.PLANS) }) {
                Text(tr("La formule ESSA STANDARD est limitée à ${s.profile.tier.maxGoals} objectifs. ESSA PLUS (gratuit) les rend illimités.", "ESSA STANDARD is limited to ${s.profile.tier.maxGoals} goals. ESSA PLUS (free) makes them unlimited."),
                    style = EssaType.small, color = c.warning)
            }
        }
        item {
            Text(tr("Les coffres sont des sous-comptes virtuels : l'argent déposé quitte votre compte et reste disponible au retrait à tout moment. Aucune somme réelle n'est épargnée.",
                "Vaults are virtual sub-accounts: deposited money leaves your account and can be withdrawn at any time. No real money is saved."), style = EssaType.small, color = c.textMuted)
        }
    }
}

@Composable
private fun GoalCard(g: SavingsGoal, lang: com.superessence.essabank.core.money.AppLanguage, onClick: () -> Unit) {
    val c = Essa.colors
    val col = Brand.color(g.colorIndex)
    Panel(onClick = onClick, padding = 12.dp) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            GoalIllustration(g.kind, col, Modifier.size(76.dp), 30.dp)
            Spacer(Modifier.width(14.dp))
            Column(Modifier.weight(1f)) {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Text(g.name, style = EssaType.title, color = c.text, maxLines = 1, overflow = TextOverflow.Ellipsis, modifier = Modifier.weight(1f, fill = false))
                    if (g.isReached) { Spacer(Modifier.width(6.dp)); Icon(Icons.Rounded.EmojiEvents, null, tint = Brand.Warning, modifier = Modifier.size(18.dp)) }
                    if (g.autoSave?.active == true) { Spacer(Modifier.width(6.dp)); Icon(Icons.Rounded.Autorenew, null, tint = c.accent, modifier = Modifier.size(16.dp)) }
                }
                Text(L.goalKind(g.kind, lang) + (g.targetDate?.let { " · " + L.shortDate(LocalDate.ofEpochDay(it), lang) } ?: ""), style = EssaType.small, color = c.textMuted)
                Spacer(Modifier.height(8.dp))
                ProgressBar(g.progress, col, height = 7.dp)
                Spacer(Modifier.height(6.dp))
                Row {
                    Text(fmt(g.saved), style = EssaType.small, color = c.text, modifier = Modifier.weight(1f))
                    Text("${(g.progress * 100).toInt()} % · " + fmt(g.target, decimals = false), style = EssaType.small, color = c.textMuted)
                }
            }
        }
    }
}

@Composable
fun GoalDetailScreen(vm: AppViewModel, nav: Nav, id: String) {
    val s = vm.state.collectAsStateWithLifecycle().value ?: return
    val g = s.goal(id)
    if (g == null) { LaunchedEffect(Unit) { nav.back() }; return }
    val c = Essa.colors
    val lang = LocalLang.current
    val col = Brand.color(g.colorIndex)
    var move by remember { mutableStateOf<GoalMoveType?>(null) }
    var auto by remember { mutableStateOf(false) }
    var delete by remember { mutableStateOf(false) }
    val today = LocalDate.now(vm.zone)
    val monthsLeft = g.targetDate?.let { ChronoUnit.MONTHS.between(today, LocalDate.ofEpochDay(it)).coerceAtLeast(1) }
    val perMonth = monthsLeft?.let { Money(g.remaining.cents / it) }
    val moves = s.goalMovements.filter { it.goalId == id }.sortedByDescending { it.timestamp }

    ScreenScaffold(g.name, nav::back, actions = { RoundIconButton(Icons.Rounded.Edit, tr("Modifier", "Edit"), { nav.go(Routes.editGoal(id)) }) }) {
        item { GoalIllustration(g.kind, col, Modifier.fillMaxWidth().height(150.dp), 56.dp) }
        item {
            Panel {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    ProgressRing(g.progress, col, 96.dp) { Text("${(g.progress * 100).toInt()} %", style = EssaType.h3, color = c.text) }
                    Spacer(Modifier.width(16.dp))
                    Column {
                        Text(tr("Épargné", "Saved"), style = EssaType.small, color = c.textMuted)
                        Text(fmt(g.saved), style = EssaType.h2, color = c.text)
                        Text(tr("Objectif ", "Target ") + fmt(g.target), style = EssaType.small, color = c.textMuted)
                        if (g.isReached) Text(tr("Objectif atteint", "Goal reached"), style = EssaType.bodyStrong, color = c.positive)
                        else Text(tr("Reste ", "Remaining ") + fmt(g.remaining), style = EssaType.bodyStrong, color = c.textSecondary)
                    }
                }
                if (g.targetDate != null) {
                    Spacer(Modifier.height(12.dp))
                    KeyValue(tr("Date souhaitée", "Target date"), L.date(LocalDate.ofEpochDay(g.targetDate!!), lang))
                    if (!g.isReached && perMonth != null) KeyValue(tr("Effort suggéré", "Suggested effort"), fmt(perMonth) + tr(" / mois", " / month"))
                }
            }
        }
        item {
            Row(horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                PrimaryButton(tr("Déposer", "Deposit"), { move = GoalMoveType.DEPOSIT }, Modifier.weight(1f), icon = Icons.Rounded.Add)
                SecondaryButton(tr("Retirer", "Withdraw"), { move = GoalMoveType.WITHDRAWAL }, Modifier.weight(1f).height(56.dp), icon = Icons.Rounded.Remove, enabled = g.saved.isPositive)
            }
        }
        item {
            Panel(onClick = { auto = true }) {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    IconBadge(Icons.Rounded.Autorenew, c.accent, 42.dp)
                    Spacer(Modifier.width(12.dp))
                    Column(Modifier.weight(1f)) {
                        Text(tr("Épargne automatique simulée", "Simulated auto-save"), style = EssaType.title, color = c.text)
                        val a = g.autoSave
                        Text(if (a != null && a.active) fmt(a.amount) + " · " + L.frequency(a.frequency, lang) + tr(" · prochain versement le ", " · next on ") + L.shortDate(LocalDate.ofEpochDay(a.nextDate), lang)
                            else tr("Désactivée · touchez pour configurer", "Off · tap to set up"), style = EssaType.small, color = c.textMuted)
                    }
                }
            }
        }
        item { SectionHeader(tr("Historique des dépôts", "Deposit history")) }
        item {
            Panel(padding = 8.dp) {
                if (moves.isEmpty()) Text(tr("Aucun mouvement pour l'instant.", "No movement yet."), style = EssaType.body, color = c.textMuted, modifier = Modifier.padding(10.dp))
                moves.take(40).forEach { m ->
                    val dep = m.type == GoalMoveType.DEPOSIT
                    ListRow(
                        (if (dep) tr("Dépôt", "Deposit") else tr("Retrait", "Withdrawal")) + if (m.automatic) tr(" automatique", " (auto)") else "",
                        (s.account(m.accountId)?.name ?: "—") + " · " + L.dateTime(m.timestamp, vm.zone, lang),
                        leading = { IconBadge(if (dep) Icons.Rounded.Add else Icons.Rounded.Remove, if (dep) c.positive else c.textMuted, 38.dp) },
                        trailing = { Text(fmt(if (dep) m.amount else -m.amount, signed = true), style = EssaType.amount, color = if (dep) c.positive else c.text) },
                    )
                }
            }
        }
        item { SecondaryButton(tr("Supprimer l'objectif", "Delete goal"), { delete = true }, Modifier.fillMaxWidth(), icon = Icons.Rounded.Delete, danger = true) }
    }

    move?.let { type -> GoalMoveSheet(vm, s, g, type) { move = null } }
    if (auto) AutoSaveSheet(vm, s, g) { auto = false }
    if (delete) {
        val target = s.primaryAccount?.id ?: s.accounts.first().id
        val done = tr("Objectif supprimé, épargne reversée.", "Goal deleted, savings returned.")
        ConfirmDialog(tr("Supprimer cet objectif ?", "Delete this goal?"),
            tr("L'épargne du coffre (", "The vault balance (") + fmt(g.saved, hideable = false) + tr(") sera reversée sur ", ") will be returned to ") + (s.account(target)?.name ?: "") + ".",
            tr("Supprimer", "Delete"), danger = true, onConfirm = { vm.run(done, onOk = { nav.back() }) { deleteGoal(it, id, target) } }, onDismiss = { delete = false })
    }
}

@Composable
private fun GoalMoveSheet(vm: AppViewModel, s: com.superessence.essabank.core.model.BankState, g: SavingsGoal, type: GoalMoveType, onDismiss: () -> Unit) {
    val c = Essa.colors
    val deposit = type == GoalMoveType.DEPOSIT
    val accounts = s.accounts.filter { it.type != AccountType.INVESTMENT }
    var account by remember { mutableStateOf(s.primaryAccount?.id ?: accounts.first().id) }
    var text by remember { mutableStateOf("") }
    val parsed = Money.parse(text)
    val requestId = remember { java.util.UUID.randomUUID().toString() }
    val okMsg = if (deposit) tr("Dépôt effectué dans le coffre.", "Deposit added to the vault.") else tr("Retrait effectué.", "Withdrawal done.")
    EssaSheet(onDismiss) {
        Text(if (deposit) tr("Déposer dans « ${g.name} »", "Deposit into \"${g.name}\"") else tr("Retirer de « ${g.name} »", "Withdraw from \"${g.name}\""), style = EssaType.h2, color = c.text)
        Spacer(Modifier.height(4.dp))
        Text(if (deposit) tr("Disponible dans le coffre après dépôt à tout moment.", "Withdraw anytime after depositing.") else tr("Épargne du coffre : ", "Vault balance: ") + fmt(g.saved, hideable = false), style = EssaType.small, color = c.textMuted)
        Spacer(Modifier.height(12.dp))
        EssaTextField(text, { text = it }, tr("Montant (€)", "Amount (€)"), keyboard = KeyboardType.Decimal, maxLength = 12, isError = text.isNotEmpty() && parsed !is ParseResult.Valid)
        Spacer(Modifier.height(8.dp))
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            val quick = if (deposit) listOf(50, 100, 250, 500) else listOf(50, 100)
            quick.forEach { v -> Chip("$v", text == "$v") { text = "$v" } }
            if (deposit && !g.isReached) Chip(tr("Le reste", "Remaining"), false) { text = g.remaining.toBigDecimal().toPlainString().replace('.', ',') }
            if (!deposit && g.saved.isPositive) Chip(tr("Tout", "All"), false) { text = g.saved.toBigDecimal().toPlainString().replace('.', ',') }
        }
        Spacer(Modifier.height(12.dp))
        Text(if (deposit) tr("Depuis le compte", "From account") else tr("Vers le compte", "To account"), style = EssaType.title, color = c.text)
        accounts.forEach { a -> AccountRow(a, { account = a.id }, selected = a.id == account) }
        Spacer(Modifier.height(12.dp))
        PrimaryButton(if (deposit) tr("Déposer", "Deposit") else tr("Retirer", "Withdraw"), enabled = parsed is ParseResult.Valid, onClick = {
            val amount = (parsed as? ParseResult.Valid)?.money ?: return@PrimaryButton
            vm.run(okMsg, onOk = { onDismiss() }) {
                if (deposit) depositToGoal(it, g.id, account, amount, requestId) else withdrawFromGoal(it, g.id, account, amount, requestId)
            }
        })
    }
}

@Composable
private fun AutoSaveSheet(vm: AppViewModel, s: com.superessence.essabank.core.model.BankState, g: SavingsGoal, onDismiss: () -> Unit) {
    val c = Essa.colors
    val lang = LocalLang.current
    val current = g.autoSave
    var text by remember { mutableStateOf(current?.amount?.toBigDecimal()?.stripTrailingZeros()?.toPlainString()?.replace('.', ',') ?: "50") }
    var freq by remember { mutableStateOf(current?.frequency ?: Frequency.MONTHLY) }
    var account by remember { mutableStateOf(current?.fromAccountId ?: s.primaryAccount?.id ?: s.accounts.first().id) }
    val parsed = Money.parse(text)
    val today = LocalDate.now(vm.zone)
    val onMsg = tr("Épargne automatique activée.", "Auto-save enabled.")
    val offMsg = tr("Épargne automatique désactivée.", "Auto-save disabled.")
    EssaSheet(onDismiss) {
        Text(tr("Épargne automatique", "Automatic savings"), style = EssaType.h2, color = c.text)
        Spacer(Modifier.height(4.dp))
        Text(tr("Un versement virtuel est effectué automatiquement, le premier dès aujourd'hui.", "A virtual deposit is made automatically, the first one today."), style = EssaType.small, color = c.textMuted)
        Spacer(Modifier.height(12.dp))
        EssaTextField(text, { text = it }, tr("Montant par versement (€)", "Amount per deposit (€)"), keyboard = KeyboardType.Decimal, maxLength = 12)
        Spacer(Modifier.height(10.dp))
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            listOf(Frequency.WEEKLY, Frequency.MONTHLY).forEach { f -> Chip(L.frequency(f, lang), freq == f) { freq = f } }
        }
        Spacer(Modifier.height(10.dp))
        LazyRow(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            items(s.accounts.filter { it.type != AccountType.INVESTMENT }, key = { it.id }) { a -> Chip(a.name, a.id == account, L.icon(a.icon)) { account = a.id } }
        }
        Spacer(Modifier.height(16.dp))
        PrimaryButton(tr("Activer", "Enable"), enabled = parsed is ParseResult.Valid, onClick = {
            val amount = (parsed as? ParseResult.Valid)?.money ?: return@PrimaryButton
            vm.run(onMsg, onOk = { vm.processDue(); onDismiss() }) { setAutoSave(it, g.id, amount, freq, account, today) }
        })
        if (current != null) {
            Spacer(Modifier.height(8.dp))
            SecondaryButton(tr("Désactiver", "Disable"), { vm.run(offMsg, onOk = { onDismiss() }) { setAutoSave(it, g.id, null, freq, account, today) } }, Modifier.fillMaxWidth(), danger = true)
        }
    }
}

@Composable
fun GoalEditorScreen(vm: AppViewModel, nav: Nav, id: String?) {
    val s = vm.state.collectAsStateWithLifecycle().value ?: return
    val existing = id?.let { s.goal(it) }
    val c = Essa.colors
    val lang = LocalLang.current
    val today = LocalDate.now(vm.zone)
    var kind by remember { mutableStateOf(existing?.kind ?: GoalKind.VACATION) }
    var name by remember { mutableStateOf(existing?.name ?: L.goalKind(GoalKind.VACATION, lang)) }
    var edited by remember { mutableStateOf(existing != null) }
    var target by remember { mutableStateOf(existing?.target?.toBigDecimal()?.stripTrailingZeros()?.toPlainString()?.replace('.', ',') ?: "2000") }
    var color by remember { mutableIntStateOf(existing?.colorIndex ?: 3) }
    val dateOptions = listOf<Pair<LocalDate?, String>>(null to tr("Sans date", "No date"), today.plusMonths(3) to tr("3 mois", "3 months"),
        today.plusMonths(6) to tr("6 mois", "6 months"), today.plusYears(1) to tr("1 an", "1 year"), today.plusYears(2) to tr("2 ans", "2 years"), today.plusYears(5) to tr("5 ans", "5 years"))
    var dateIndex by remember { mutableIntStateOf(if (existing == null) 3 else if (existing.targetDate == null) 0 else -1) }
    val parsed = Money.parse(target)
    val created = tr("Objectif créé.", "Goal created.")
    val saved = tr("Objectif mis à jour.", "Goal updated.")
    ScreenScaffold(if (existing == null) tr("Nouvel objectif", "New goal") else tr("Modifier l'objectif", "Edit goal"), nav::back, bottomBar = {
        PrimaryButton(if (existing == null) tr("Créer le coffre", "Create vault") else tr("Enregistrer", "Save"), enabled = name.isNotBlank() && parsed is ParseResult.Valid, onClick = {
            val amount = (parsed as? ParseResult.Valid)?.money ?: return@PrimaryButton
            val date = dateOptions.getOrNull(dateIndex)?.first
            if (existing == null) vm.run(created, onOk = { v -> (v as? SavingsGoal)?.let { nav.replace(Routes.goal(it.id)) } }) { createGoal(it, name, kind, amount, date, color) }
            else vm.run(saved, onOk = { nav.back() }) {
                updateGoal(it, existing.id, name = name, target = amount, kind = kind, colorIndex = color,
                    targetDate = if (dateIndex > 0) date else null, clearDate = dateIndex == 0)
            }
        })
    }) {
        item { GoalIllustration(kind, Brand.color(color), Modifier.fillMaxWidth().height(130.dp), 48.dp) }
        item { SectionHeader(tr("Type d'objectif", "Goal type")) }
        item {
            LazyRow(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                items(GoalKind.entries.toList()) { k -> Chip(L.goalKind(k, lang), k == kind, L.goalIcon(k)) { kind = k; if (!edited) name = L.goalKind(k, lang) } }
            }
        }
        item { EssaTextField(name, { name = it; edited = true }, tr("Nom de l'objectif", "Goal name"), maxLength = 32) }
        item { EssaTextField(target, { target = it }, tr("Montant cible (€)", "Target amount (€)"), keyboard = KeyboardType.Decimal, maxLength = 12, isError = target.isNotEmpty() && parsed !is ParseResult.Valid) }
        item { SectionHeader(tr("Date souhaitée", "Target date")) }
        item {
            LazyRow(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                items(dateOptions.size) { i -> Chip(dateOptions[i].second, i == dateIndex) { dateIndex = i } }
            }
            if (dateIndex == -1 && existing?.targetDate != null) {
                Spacer(Modifier.height(6.dp))
                Text(tr("Date actuelle : ", "Current date: ") + L.date(LocalDate.ofEpochDay(existing.targetDate!!), lang), style = EssaType.small, color = c.textMuted)
            }
        }
        item { SectionHeader(tr("Couleur de l'illustration", "Illustration color")) }
        item {
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
                Brand.palette.forEachIndexed { i, col -> ColorDot(col, i == color) { color = i } }
            }
        }
    }
}
