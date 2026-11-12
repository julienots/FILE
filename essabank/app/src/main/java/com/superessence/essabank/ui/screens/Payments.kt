package com.superessence.essabank.ui.screens

import androidx.activity.compose.BackHandler
import androidx.compose.animation.AnimatedContent
import androidx.compose.animation.core.Animatable
import androidx.compose.animation.core.FastOutSlowInEasing
import androidx.compose.animation.core.tween
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.slideInHorizontally
import androidx.compose.animation.slideOutHorizontally
import androidx.compose.animation.togetherWith
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.imePadding
import androidx.compose.foundation.layout.navigationBarsPadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.LazyRow
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.rounded.ReceiptLong
import androidx.compose.material.icons.rounded.AccountBalance
import androidx.compose.material.icons.rounded.Add
import androidx.compose.material.icons.automirrored.rounded.CallReceived
import androidx.compose.material.icons.rounded.Delete
import androidx.compose.material.icons.rounded.Event
import androidx.compose.material.icons.rounded.Group
import androidx.compose.material.icons.rounded.PersonAdd
import androidx.compose.material.icons.rounded.Repeat
import androidx.compose.material.icons.rounded.Schedule
import androidx.compose.material.icons.automirrored.rounded.Send
import androidx.compose.material.icons.rounded.Star
import androidx.compose.material.icons.rounded.StarBorder
import androidx.compose.material.icons.rounded.SwapHoriz
import androidx.compose.material3.Icon
import androidx.compose.material3.Switch
import androidx.compose.material3.SwitchDefaults
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.PathEffect
import androidx.compose.ui.graphics.PathMeasure
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.superessence.essabank.core.engine.EngineResult
import com.superessence.essabank.core.model.AccountType
import com.superessence.essabank.core.model.Beneficiary
import com.superessence.essabank.core.model.Frequency
import com.superessence.essabank.core.model.Transaction
import com.superessence.essabank.core.model.TxKind
import com.superessence.essabank.core.money.Money
import com.superessence.essabank.core.money.MoneyFormat
import com.superessence.essabank.core.money.ParseResult
import com.superessence.essabank.core.pdf.Reports
import com.superessence.essabank.ui.AppViewModel
import com.superessence.essabank.ui.Nav
import com.superessence.essabank.ui.Routes
import com.superessence.essabank.ui.components.Avatar
import com.superessence.essabank.ui.components.Chip
import com.superessence.essabank.ui.components.ConfirmDialog
import com.superessence.essabank.ui.components.EmptyState
import com.superessence.essabank.ui.components.EssaSheet
import com.superessence.essabank.ui.components.EssaTextField
import com.superessence.essabank.ui.components.IconBadge
import com.superessence.essabank.ui.components.KeyValue
import com.superessence.essabank.ui.components.ListRow
import com.superessence.essabank.ui.components.LocalPrefs
import com.superessence.essabank.ui.components.NumberPad
import com.superessence.essabank.ui.components.PadExtra
import com.superessence.essabank.ui.components.Panel
import com.superessence.essabank.ui.components.PrimaryButton
import com.superessence.essabank.ui.components.RoundIconButton
import com.superessence.essabank.ui.components.ScreenScaffold
import com.superessence.essabank.ui.components.SecondaryButton
import com.superessence.essabank.ui.components.SectionHeader
import com.superessence.essabank.ui.components.SimulationPill
import com.superessence.essabank.ui.components.TopBar
import com.superessence.essabank.ui.components.AccountGlyph
import com.superessence.essabank.ui.components.applyAmountKey
import com.superessence.essabank.ui.components.fmt
import com.superessence.essabank.ui.components.pressable
import com.superessence.essabank.ui.i18n.L
import com.superessence.essabank.ui.i18n.LocalLang
import com.superessence.essabank.ui.i18n.tr
import com.superessence.essabank.ui.theme.Brand
import com.superessence.essabank.ui.theme.Dimens
import com.superessence.essabank.ui.theme.Essa
import com.superessence.essabank.ui.theme.EssaType
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import java.time.LocalDate

// ================================================================ Onglet Paiements

@Composable
fun PaymentsScreen(vm: AppViewModel, nav: Nav) {
    val s = vm.state.collectAsStateWithLifecycle().value ?: return
    val c = Essa.colors
    val lang = LocalLang.current
    val transfers = remember(s) {
        s.transactions.filter { it.kind == TxKind.TRANSFER_OUT || it.kind == TxKind.TRANSFER_IN || (it.kind == TxKind.INTERNAL_TRANSFER && it.isDebit) }
            .sortedByDescending { it.timestamp }.take(8)
    }
    val favorites = s.beneficiaries.sortedByDescending { it.favorite }
    LazyColumn(
        Modifier.fillMaxSize().background(c.background),
        contentPadding = PaddingValues(start = Dimens.screen, end = Dimens.screen, bottom = 120.dp),
        verticalArrangement = Arrangement.spacedBy(14.dp),
    ) {
        item {
            Column(Modifier.statusBarsPadding().padding(top = 14.dp)) {
                Text(tr("Paiements", "Payments"), style = EssaType.h1, color = c.text)
                Text(tr("Virements simulés en euros virtuels · aucun transfert réel", "Simulated transfers in virtual euros · no real transfer"), style = EssaType.small, color = c.textMuted)
            }
        }
        item {
            Row(horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                PaymentTile(Icons.AutoMirrored.Rounded.Send, tr("Envoyer", "Send"), tr("À un bénéficiaire", "To a beneficiary"), Modifier.weight(1f), true) { nav.go(Routes.transfer("beneficiary")) }
                PaymentTile(Icons.Rounded.SwapHoriz, tr("Entre comptes", "Between accounts"), tr("Instantané", "Instant"), Modifier.weight(1f)) { nav.go(Routes.transfer("internal")) }
                PaymentTile(Icons.AutoMirrored.Rounded.CallReceived, tr("Recevoir", "Receive"), tr("Simuler", "Simulate"), Modifier.weight(1f)) { nav.go(Routes.transfer("receive")) }
            }
        }
        item { SectionHeader(tr("Bénéficiaires", "Beneficiaries"), tr("Gérer", "Manage")) { nav.go(Routes.BENEFICIARIES) } }
        item {
            LazyRow(horizontalArrangement = Arrangement.spacedBy(14.dp)) {
                item {
                    Column(horizontalAlignment = Alignment.CenterHorizontally, modifier = Modifier.width(66.dp).pressable { nav.go(Routes.BENEFICIARIES) }) {
                        Box(Modifier.size(56.dp).clip(CircleShape).border(1.5.dp, c.border, CircleShape), contentAlignment = Alignment.Center) { Icon(Icons.Rounded.PersonAdd, null, tint = c.accent) }
                        Spacer(Modifier.height(6.dp))
                        Text(tr("Ajouter", "Add"), style = EssaType.small, color = c.textSecondary)
                    }
                }
                items(favorites, key = { it.id }) { b ->
                    Column(horizontalAlignment = Alignment.CenterHorizontally, modifier = Modifier.width(66.dp).pressable { nav.go(Routes.transfer("beneficiary", to = b.id)) }) {
                        Avatar(initials(b.name), b.colorIndex, 56.dp)
                        Spacer(Modifier.height(6.dp))
                        Text(b.name.substringBefore(' '), style = EssaType.small, color = c.textSecondary, maxLines = 1, overflow = TextOverflow.Ellipsis)
                    }
                }
            }
        }
        item { SectionHeader(tr("Virements programmés", "Scheduled transfers"), tr("Tout voir", "See all")) { nav.go(Routes.SCHEDULED) } }
        item {
            Panel(padding = 8.dp) {
                val list = s.scheduled.filter { it.active }.sortedBy { it.nextDate }.take(3)
                if (list.isEmpty()) ListRow(tr("Programmer un virement", "Schedule a transfer"), tr("Ponctuel, hebdomadaire ou mensuel", "One-off, weekly or monthly"),
                    leading = { IconBadge(Icons.Rounded.Schedule, c.accent, 40.dp) }, onClick = { nav.go(Routes.transfer("internal")) })
                list.forEach { sch -> ScheduledRow(s, sch, lang) { nav.go(Routes.SCHEDULED) } }
            }
        }
        item { SectionHeader(tr("Derniers virements", "Latest transfers"), tr("Historique", "History")) { nav.go(Routes.history(category = "TRANSFERS")) } }
        item {
            Panel(padding = 8.dp) {
                if (transfers.isEmpty()) Text(tr("Aucun virement pour l'instant.", "No transfer yet."), style = EssaType.body, color = c.textMuted, modifier = Modifier.padding(12.dp))
                transfers.forEach { TransactionRow(it, s, vm.zone, { nav.go(Routes.tx(it.id)) }) }
            }
        }
    }
}

fun initials(name: String) = name.split(' ').filter { it.isNotBlank() }.take(2).joinToString("") { it.take(1).uppercase() }

@Composable
private fun PaymentTile(icon: androidx.compose.ui.graphics.vector.ImageVector, title: String, subtitle: String, modifier: Modifier, primary: Boolean = false, onClick: () -> Unit) {
    val c = Essa.colors
    Column(
        modifier.clip(RoundedCornerShape(22.dp)).background(if (primary) c.hero else androidx.compose.ui.graphics.Brush.linearGradient(listOf(c.surface, c.surface)))
            .border(1.dp, if (primary) Color.Transparent else c.border, RoundedCornerShape(22.dp)).pressable(onClick = onClick).padding(14.dp),
    ) {
        Box(Modifier.size(38.dp).clip(RoundedCornerShape(12.dp)).background(if (primary) Color.White.copy(alpha = 0.16f) else c.accentSoft), contentAlignment = Alignment.Center) {
            Icon(icon, null, tint = if (primary) Color.White else c.accent, modifier = Modifier.size(20.dp))
        }
        Spacer(Modifier.height(18.dp))
        Text(title, style = EssaType.bodyStrong, color = if (primary) Color.White else c.text, maxLines = 1)
        Text(subtitle, style = EssaType.small, color = if (primary) Color.White.copy(alpha = 0.7f) else c.textMuted, maxLines = 1)
    }
}

@Composable
private fun ScheduledRow(s: com.superessence.essabank.core.model.BankState, sch: com.superessence.essabank.core.model.ScheduledTransfer, lang: com.superessence.essabank.core.money.AppLanguage, onClick: () -> Unit) {
    val c = Essa.colors
    val dest = sch.toAccountId?.let { s.account(it)?.name } ?: sch.beneficiaryId?.let { s.beneficiary(it)?.name } ?: "—"
    ListRow(
        sch.label, dest + " · " + L.frequency(sch.frequency, lang) + " · " + L.shortDate(LocalDate.ofEpochDay(sch.nextDate), lang),
        leading = { IconBadge(if (sch.frequency == Frequency.ONCE) Icons.Rounded.Event else Icons.Rounded.Repeat, if (sch.active) c.accent else c.textMuted, 40.dp) },
        trailing = { Text(fmt(sch.amount), style = EssaType.amount, color = c.text) }, onClick = onClick, chevron = false,
    )
}

// ================================================================ Parcours de virement

private enum class Step { AMOUNT, DETAILS, REVIEW, PROCESSING, DONE }

/**
 * Virement simulé en trois étapes (montant, détails, vérification), suivi d'une animation
 * de transfert et d'un reçu marqué « SIMULATION — AUCUN TRANSFERT RÉEL ».
 * [mode] : « internal » (entre comptes), « beneficiary » (vers un bénéficiaire fictif), « receive » (réception simulée).
 */
@Composable
fun TransferScreen(vm: AppViewModel, nav: Nav, mode: String, initialTo: String?, initialFrom: String?) {
    val s = vm.state.collectAsStateWithLifecycle().value ?: return
    val c = Essa.colors
    val lang = LocalLang.current
    val receive = mode == "receive"
    val toBeneficiary = mode == "beneficiary"
    val accounts = s.accounts.sortedBy { it.sortOrder }
    var step by remember { mutableStateOf(Step.AMOUNT) }
    var amountText by remember { mutableStateOf("") }
    var from by remember { mutableStateOf(initialFrom?.takeIf { s.account(it) != null } ?: s.primaryAccount?.id ?: accounts.first().id) }
    var toAccount by remember {
        mutableStateOf(if (!toBeneficiary && !receive) initialTo?.takeIf { s.account(it) != null } ?: accounts.firstOrNull { it.id != (initialFrom ?: s.primaryAccount?.id) }?.id else null)
    }
    var beneficiary by remember { mutableStateOf(if (toBeneficiary) initialTo?.takeIf { s.beneficiary(it) != null } else null) }
    var sender by remember { mutableStateOf("") }
    var motif by remember { mutableStateOf("") }
    var note by remember { mutableStateOf("") }
    var scheduled by remember { mutableStateOf(false) }
    var dateOffset by remember { mutableIntStateOf(1) }
    var frequency by remember { mutableStateOf(Frequency.ONCE) }
    var result by remember { mutableStateOf<Transaction?>(null) }
    var error by remember { mutableStateOf<String?>(null) }
    var addBen by remember { mutableStateOf(false) }
    val requestId = remember { java.util.UUID.randomUUID().toString() }
    val scope = rememberCoroutineScope()

    val parsed = Money.parse(amountText.replace(',', '.'))
    val amount = (parsed as? ParseResult.Valid)?.money
    // Pour une réception, le compte crédité est « from » (compte de l'utilisateur).
    val fromAcc = s.account(from)
    val today = LocalDate.now(vm.zone)
    val dates = listOf(
        today to tr("Aujourd'hui", "Today"), today.plusDays(1) to tr("Demain", "Tomorrow"),
        today.plusDays(7) to tr("Dans 7 jours", "In 7 days"), today.withDayOfMonth(1).plusMonths(1) to tr("1er du mois", "1st of month"),
    )
    val execDate = dates[dateOffset.coerceIn(0, dates.lastIndex)].first

    val title = when {
        receive -> tr("Recevoir (simulation)", "Receive (simulation)")
        toBeneficiary -> tr("Envoyer de l'argent virtuel", "Send virtual money")
        else -> tr("Virement entre mes comptes", "Transfer between my accounts")
    }
    val destName = when {
        receive -> fromAcc?.name ?: ""
        toBeneficiary -> beneficiary?.let { s.beneficiary(it)?.name } ?: ""
        else -> toAccount?.let { s.account(it)?.name } ?: ""
    }
    val amountError = when {
        amount == null -> null
        !receive && !scheduled && fromAcc != null && amount > fromAcc.available -> tr("Solde virtuel insuffisant (disponible : ", "Insufficient virtual balance (available: ") + fmt(fromAcc.available, hideable = false) + ")"
        else -> null
    }

    BackHandler(enabled = step == Step.DETAILS || step == Step.REVIEW) { step = if (step == Step.REVIEW) Step.DETAILS else Step.AMOUNT }
    BackHandler(enabled = step == Step.PROCESSING) { }

    fun confirm() {
        val a = amount ?: return
        error = null
        step = Step.PROCESSING
        scope.launchTransfer(vm, onResult = { ok, tx ->
            if (ok) { result = tx; step = Step.DONE } else { step = Step.REVIEW }
        }) {
            when {
                scheduled -> vm.exec {
                    scheduleTransfer(it, from, if (toBeneficiary) null else toAccount, if (toBeneficiary) beneficiary else null, a, motif, note, execDate, frequency, requestId = requestId)
                }.let { r -> r.isOk to null }
                receive -> vm.exec { receiveSimulated(it, from, sender, a, motif, requestId) }.let { r -> r.isOk to r.valueOrNull() }
                toBeneficiary -> vm.exec { sendToBeneficiary(it, from, beneficiary!!, a, motif, note, requestId) }.let { r -> r.isOk to r.valueOrNull() }
                else -> vm.exec { transferInternal(it, from, toAccount!!, a, motif, note, requestId) }.let { r -> r.isOk to r.valueOrNull()?.firstOrNull() }
            }
        }
    }

    Column(Modifier.fillMaxSize().background(c.background)) {
        if (step != Step.PROCESSING && step != Step.DONE) {
            TopBar(title, onBack = {
                when (step) {
                    Step.AMOUNT -> { nav.back() }
                    Step.DETAILS -> { step = Step.AMOUNT }
                    else -> { step = Step.DETAILS }
                }
            }) { SimulationPill(Modifier.padding(end = 12.dp)) }
            StepIndicator(step.ordinal.coerceAtMost(2))
        }
        AnimatedContent(step, transitionSpec = {
            (slideInHorizontally(tween(320)) { it / 3 } + fadeIn(tween(320))) togetherWith (slideOutHorizontally(tween(260)) { -it / 4 } + fadeOut(tween(200)))
        }, label = "transfer", modifier = Modifier.weight(1f)) { st ->
            when (st) {
                Step.AMOUNT -> Column(Modifier.fillMaxSize().navigationBarsPadding().padding(horizontal = Dimens.screen)) {
                    Spacer(Modifier.weight(0.4f))
                    Text(if (receive) tr("Montant à recevoir", "Amount to receive") else tr("Montant à envoyer", "Amount to send"), style = EssaType.small, color = c.textMuted, modifier = Modifier.fillMaxWidth(), textAlign = TextAlign.Center)
                    Spacer(Modifier.height(4.dp))
                    Text((amountText.ifEmpty { "0" }) + " €",
                        style = EssaType.hero.copy(fontSize = 46.sp), color = if (amountError != null) c.negative else c.text, textAlign = TextAlign.Center, modifier = Modifier.fillMaxWidth(), maxLines = 1)
                    Text(amountError ?: (fromAcc?.let { (if (receive) tr("Crédité sur ", "Credited to ") else tr("Disponible sur ", "Available on ")) + it.name + " : " + fmt(it.available, hideable = false) } ?: ""),
                        style = EssaType.small, color = if (amountError != null) c.negative else c.textMuted, textAlign = TextAlign.Center, modifier = Modifier.fillMaxWidth())
                    Spacer(Modifier.height(14.dp))
                    LazyRow(horizontalArrangement = Arrangement.spacedBy(8.dp), contentPadding = PaddingValues(horizontal = 4.dp)) {
                        items(accounts.filter { receive || it.type != AccountType.INVESTMENT || !toBeneficiary }, key = { it.id }) { a ->
                            Chip(a.name, a.id == from, com.superessence.essabank.ui.i18n.L.icon(a.icon)) { from = a.id; if (toAccount == a.id) toAccount = accounts.firstOrNull { it.id != a.id }?.id }
                        }
                    }
                    Spacer(Modifier.height(8.dp))
                    Row(horizontalArrangement = Arrangement.spacedBy(8.dp), modifier = Modifier.fillMaxWidth()) {
                        listOf(20, 50, 100, 250).forEach { v -> Box(Modifier.weight(1f)) { Chip("+$v", false) { amountText = addQuick(amountText, v) } } }
                    }
                    Spacer(Modifier.weight(0.6f))
                    NumberPad({ k -> amountText = applyAmountKey(amountText, k) }, { amountText = applyAmountKey(amountText, null) }, PadExtra.DECIMAL, keyHeight = 56.dp)
                    Spacer(Modifier.height(12.dp))
                    PrimaryButton(tr("Continuer", "Continue"), enabled = amount != null && amountError == null, onClick = { step = Step.DETAILS })
                    Spacer(Modifier.height(12.dp))
                }
                Step.DETAILS -> Column(Modifier.fillMaxSize().imePadding().navigationBarsPadding()) {
                    Column(Modifier.weight(1f).verticalScroll(rememberScrollState()).padding(horizontal = Dimens.screen), verticalArrangement = Arrangement.spacedBy(10.dp)) {
                        Spacer(Modifier.height(4.dp))
                        when {
                            receive -> {
                                SectionHeader(tr("Émetteur fictif", "Fictional sender"))
                                EssaTextField(sender, { sender = it }, tr("Nom de l'émetteur", "Sender name"), maxLength = 40)
                                LazyRow(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                                    items(s.beneficiaries.map { it.name } + listOf("Studio Fictif SAS")) { n -> Chip(n, sender == n) { sender = n } }
                                }
                            }
                            toBeneficiary -> {
                                Row(verticalAlignment = Alignment.CenterVertically) {
                                    Box(Modifier.weight(1f)) { SectionHeader(tr("Bénéficiaire fictif", "Fictional beneficiary")) }
                                    Text(tr("+ Nouveau", "+ New"), style = EssaType.bodyStrong, color = c.accent, modifier = Modifier.clip(RoundedCornerShape(8.dp)).pressable { addBen = true }.padding(6.dp))
                                }
                                if (s.beneficiaries.isEmpty()) Text(tr("Ajoutez d'abord un bénéficiaire fictif.", "Add a fictional beneficiary first."), style = EssaType.body, color = c.textMuted)
                                s.beneficiaries.sortedByDescending { it.favorite }.forEach { b ->
                                    BeneficiaryRow(b, selected = b.id == beneficiary) { beneficiary = b.id }
                                }
                            }
                            else -> {
                                SectionHeader(tr("Compte destinataire", "Destination account"))
                                accounts.filter { it.id != from }.forEach { a -> AccountRow(a, { toAccount = a.id }, selected = a.id == toAccount) }
                            }
                        }
                        SectionHeader(tr("Motif et description", "Reference and note"))
                        EssaTextField(motif, { motif = it }, tr("Motif (facultatif)", "Reference (optional)"), maxLength = 40)
                        EssaTextField(note, { note = it }, tr("Description (facultatif)", "Note (optional)"), singleLine = false, maxLength = 200)
                        if (!receive) {
                            Panel(padding = 12.dp) {
                                Row(verticalAlignment = Alignment.CenterVertically) {
                                    IconBadge(Icons.Rounded.Schedule, c.accent, 38.dp)
                                    Spacer(Modifier.width(12.dp))
                                    Column(Modifier.weight(1f)) {
                                        Text(tr("Programmer le virement", "Schedule the transfer"), style = EssaType.title, color = c.text)
                                        Text(tr("Exécution automatique à la date choisie", "Executed automatically on the chosen date"), style = EssaType.small, color = c.textMuted)
                                    }
                                    Switch(scheduled, { scheduled = it }, colors = SwitchDefaults.colors(checkedTrackColor = c.accent))
                                }
                                if (scheduled) {
                                    Spacer(Modifier.height(10.dp))
                                    LazyRow(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                                        items(dates.size) { i -> Chip(dates[i].second + " · " + L.shortDate(dates[i].first, lang), dateOffset == i) { dateOffset = i } }
                                    }
                                    Spacer(Modifier.height(8.dp))
                                    Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                                        Frequency.entries.forEach { f -> Chip(L.frequency(f, lang), frequency == f) { frequency = f } }
                                    }
                                }
                            }
                        }
                        Spacer(Modifier.height(10.dp))
                    }
                    val ready = when { receive -> sender.isNotBlank(); toBeneficiary -> beneficiary != null; else -> toAccount != null }
                    Box(Modifier.padding(horizontal = Dimens.screen, vertical = 12.dp)) {
                        PrimaryButton(tr("Vérifier", "Review"), enabled = ready, onClick = { step = Step.REVIEW })
                    }
                }
                Step.REVIEW -> Column(Modifier.fillMaxSize().navigationBarsPadding().padding(horizontal = Dimens.screen)) {
                    Column(Modifier.weight(1f).verticalScroll(rememberScrollState())) {
                        Spacer(Modifier.height(8.dp))
                        Panel {
                            Text(tr("Vous allez ", "You are about to ") + when {
                                receive -> tr("simuler la réception de", "simulate receiving")
                                scheduled -> tr("programmer", "schedule")
                                else -> tr("envoyer", "send")
                            }, style = EssaType.small, color = c.textMuted, modifier = Modifier.fillMaxWidth(), textAlign = TextAlign.Center)
                            Text(amount?.let { fmt(it, hideable = false) } ?: "", style = EssaType.amountLarge.copy(fontSize = 38.sp), color = c.text, modifier = Modifier.fillMaxWidth(), textAlign = TextAlign.Center)
                            Spacer(Modifier.height(16.dp))
                            TransferPath(
                                if (receive) sender.ifBlank { "—" } else fromAcc?.name ?: "—",
                                if (receive) fromAcc?.name ?: "" else destName,
                            )
                            Spacer(Modifier.height(14.dp))
                            if (!receive) KeyValue(tr("Compte débité", "Debited account"), fromAcc?.name ?: "—")
                            KeyValue(if (receive) tr("Émetteur", "Sender") else tr("Destinataire", "Recipient"), if (receive) sender else destName)
                            if (toBeneficiary) beneficiary?.let { s.beneficiary(it) }?.let { KeyValue(tr("Référence fictive", "Fictional reference"), it.reference) }
                            if (receive) KeyValue(tr("Compte crédité", "Credited account"), fromAcc?.name ?: "—")
                            KeyValue(tr("Motif", "Reference"), motif.ifBlank { "—" })
                            if (note.isNotBlank()) KeyValue(tr("Description", "Note"), note)
                            KeyValue(tr("Exécution", "Execution"), if (scheduled) L.date(execDate, lang) + " · " + L.frequency(frequency, lang) else tr("Immédiate", "Immediate"))
                            KeyValue(tr("Frais", "Fees"), fmt(Money.ZERO, hideable = false))
                            if (!receive && !scheduled && fromAcc != null && amount != null)
                                KeyValue(tr("Solde après opération", "Balance after"), fmt(fromAcc.balance - amount, hideable = false))
                        }
                        Spacer(Modifier.height(12.dp))
                        Text(tr("Opération 100 % virtuelle : aucune somme réelle n'est débitée ni transmise à un établissement financier.",
                            "100% virtual operation: no real money is debited or sent to any financial institution."), style = EssaType.small, color = c.textMuted, textAlign = TextAlign.Center, modifier = Modifier.fillMaxWidth())
                        error?.let { Text(it, style = EssaType.bodyStrong, color = c.negative) }
                    }
                    PrimaryButton(tr("Confirmer la simulation", "Confirm simulation"), onClick = { confirm() })
                    Spacer(Modifier.height(12.dp))
                }
                Step.PROCESSING -> ProcessingView(if (receive) sender else fromAcc?.name ?: "", if (receive) fromAcc?.name ?: "" else destName)
                Step.DONE -> SuccessView(vm, nav, result, amount ?: Money.ZERO, scheduled, execDate, destName, receive)
            }
        }
    }
    if (addBen) AddBeneficiarySheet(vm, onDismiss = { addBen = false }) { b -> beneficiary = b.id; addBen = false }
}

private fun addQuick(current: String, v: Int): String {
    val m = (Money.parse(current.ifEmpty { "0" }.replace(',', '.')) as? ParseResult.Valid)?.money ?: Money.ZERO
    val total = m + Money.units(v)
    val plain = total.toBigDecimal().stripTrailingZeros().toPlainString()
    return plain.replace('.', ',')
}

private fun kotlinx.coroutines.CoroutineScope.launchTransfer(vm: AppViewModel, onResult: (Boolean, Transaction?) -> Unit, block: suspend () -> Pair<Boolean, Transaction?>) {
    launch {
        val started = System.currentTimeMillis()
        val (ok, tx) = block()
        // L'animation de transfert dure au moins 1,4 s pour rester lisible.
        val remaining = 1400 - (System.currentTimeMillis() - started)
        if (ok && remaining > 0) delay(remaining)
        onResult(ok, tx)
    }
}

@Composable
private fun StepIndicator(index: Int) {
    val c = Essa.colors
    Row(Modifier.fillMaxWidth().padding(horizontal = Dimens.screen, vertical = 6.dp), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
        repeat(3) { i -> Box(Modifier.weight(1f).height(4.dp).clip(RoundedCornerShape(2.dp)).background(if (i <= index) c.accent else c.border)) }
    }
}

@Composable
private fun TransferPath(from: String, to: String) {
    val c = Essa.colors
    Row(verticalAlignment = Alignment.CenterVertically) {
        Column(Modifier.weight(1f), horizontalAlignment = Alignment.CenterHorizontally) {
            Avatar(initials(from), 0, 44.dp); Spacer(Modifier.height(6.dp))
            Text(from, style = EssaType.small, color = c.textSecondary, maxLines = 1, overflow = TextOverflow.Ellipsis)
        }
        Canvas(Modifier.weight(1f).height(20.dp)) {
            drawLine(c.accent.copy(alpha = 0.6f), Offset(0f, size.height / 2), Offset(size.width, size.height / 2), 2.dp.toPx(), pathEffect = PathEffect.dashPathEffect(floatArrayOf(8f, 8f)))
            drawCircle(c.accent, 4.dp.toPx(), Offset(size.width, size.height / 2))
        }
        Column(Modifier.weight(1f), horizontalAlignment = Alignment.CenterHorizontally) {
            Avatar(initials(to), 4, 44.dp); Spacer(Modifier.height(6.dp))
            Text(to, style = EssaType.small, color = c.textSecondary, maxLines = 1, overflow = TextOverflow.Ellipsis)
        }
    }
}

/** Animation de transfert : un euro virtuel voyage d'un avatar à l'autre le long d'un arc. */
@Composable
private fun ProcessingView(from: String, to: String) {
    val c = Essa.colors
    val progress = remember { Animatable(0f) }
    LaunchedEffect(Unit) {
        while (true) { progress.snapTo(0f); progress.animateTo(1f, tween(1200, easing = FastOutSlowInEasing)) }
    }
    Column(Modifier.fillMaxSize().statusBarsPadding().padding(Dimens.screen), horizontalAlignment = Alignment.CenterHorizontally, verticalArrangement = Arrangement.Center) {
        Box(Modifier.fillMaxWidth().height(180.dp)) {
            Canvas(Modifier.fillMaxSize()) {
                val start = Offset(size.width * 0.15f, size.height * 0.7f)
                val end = Offset(size.width * 0.85f, size.height * 0.7f)
                val path = Path().apply { moveTo(start.x, start.y); quadraticTo(size.width / 2, -size.height * 0.15f, end.x, end.y) }
                drawPath(path, c.border, style = Stroke(2.dp.toPx(), pathEffect = PathEffect.dashPathEffect(floatArrayOf(10f, 10f))))
                val measure = PathMeasure().apply { setPath(path, false) }
                val p = measure.getPosition(measure.length * progress.value)
                drawCircle(c.accent.copy(alpha = 0.25f), 18.dp.toPx(), p)
                drawCircle(c.accent, 9.dp.toPx(), p)
                drawCircle(c.surfaceAlt, 26.dp.toPx(), start); drawCircle(c.accent.copy(alpha = 0.5f), 26.dp.toPx(), start, style = Stroke(2.dp.toPx()))
                drawCircle(c.surfaceAlt, 26.dp.toPx(), end); drawCircle(c.positive.copy(alpha = 0.6f), 26.dp.toPx(), end, style = Stroke(2.dp.toPx()))
            }
        }
        Spacer(Modifier.height(20.dp))
        Text(tr("Traitement de la simulation…", "Processing the simulation…"), style = EssaType.h2, color = c.text)
        Spacer(Modifier.height(6.dp))
        Text("$from  →  $to", style = EssaType.body, color = c.textMuted, textAlign = TextAlign.Center)
    }
}

@Composable
private fun SuccessView(vm: AppViewModel, nav: Nav, tx: Transaction?, amount: Money, scheduled: Boolean, date: LocalDate, dest: String, receive: Boolean) {
    val c = Essa.colors
    val lang = LocalLang.current
    val check = remember { Animatable(0f) }
    val ring = remember { Animatable(0f) }
    LaunchedEffect(Unit) { ring.animateTo(1f, tween(500)); check.animateTo(1f, tween(450, easing = FastOutSlowInEasing)) }
    val saver = rememberFileSaver("application/pdf") { ok -> vm.toast(if (ok) tr(lang, "Reçu PDF enregistré.", "PDF receipt saved.") else tr(lang, "Export annulé.", "Export cancelled."), !ok) }
    Column(Modifier.fillMaxSize().statusBarsPadding().navigationBarsPadding().padding(Dimens.screen), horizontalAlignment = Alignment.CenterHorizontally) {
        Spacer(Modifier.weight(0.5f))
        Canvas(Modifier.size(110.dp)) {
            drawCircle(c.positive.copy(alpha = 0.15f * ring.value), size.minDimension / 2 * ring.value)
            drawCircle(c.positive, size.minDimension / 2.9f * ring.value)
            val p = Path().apply {
                moveTo(size.width * 0.34f, size.height * 0.52f); lineTo(size.width * 0.46f, size.height * 0.63f); lineTo(size.width * 0.67f, size.height * 0.40f)
            }
            val m = PathMeasure().apply { setPath(p, false) }
            val seg = Path()
            m.getSegment(0f, m.length * check.value, seg, true)
            drawPath(seg, Color.White, style = Stroke(5.dp.toPx(), cap = androidx.compose.ui.graphics.StrokeCap.Round, join = androidx.compose.ui.graphics.StrokeJoin.Round))
        }
        Spacer(Modifier.height(18.dp))
        Text(when {
            scheduled -> tr("Virement programmé", "Transfer scheduled")
            receive -> tr("Virement simulé reçu", "Simulated transfer received")
            else -> tr("Virement simulé effectué", "Simulated transfer completed")
        }, style = EssaType.h1, color = c.text, textAlign = TextAlign.Center)
        Spacer(Modifier.height(6.dp))
        Text(fmt(amount, hideable = false) + (if (receive) tr(" crédités sur ", " credited to ") else " → ") + dest +
            (if (scheduled) tr(" le ", " on ") + L.date(date, lang) else ""), style = EssaType.body, color = c.textSecondary, textAlign = TextAlign.Center)
        Spacer(Modifier.height(20.dp))
        Box(Modifier.clip(RoundedCornerShape(12.dp)).background(Brand.Electric).padding(horizontal = 14.dp, vertical = 8.dp)) {
            Text(Reports.simulationMention(lang), style = EssaType.caption.copy(fontSize = 12.sp), color = Color.White)
        }
        if (tx != null) {
            Spacer(Modifier.height(10.dp))
            Text(tr("Référence ", "Reference ") + tx.reference, style = EssaType.small, color = c.textMuted)
        }
        Spacer(Modifier.weight(1f))
        if (tx != null) {
            Row(horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                SecondaryButton(tr("Voir le reçu", "View receipt"), { nav.replace(Routes.tx(tx.id)) }, Modifier.weight(1f), icon = Icons.AutoMirrored.Rounded.ReceiptLong)
                SecondaryButton(tr("Reçu PDF", "PDF receipt"), { vm.receiptPdf(tx)?.let { saver("ESSABANK-recu-simulation-${tx.reference}.pdf", it) } }, Modifier.weight(1f))
            }
            Spacer(Modifier.height(10.dp))
        }
        PrimaryButton(tr("Terminé", "Done"), { nav.back() })
    }
}

// ================================================================ Bénéficiaires

@Composable
fun BeneficiaryRow(b: Beneficiary, selected: Boolean = false, onClick: () -> Unit) {
    val c = Essa.colors
    Box(Modifier.fillMaxWidth().clip(RoundedCornerShape(16.dp)).background(if (selected) c.accentSoft else Color.Transparent).padding(horizontal = 6.dp)) {
        ListRow(b.name, b.reference + (if (b.note.isNotBlank()) " · " + b.note else ""), leading = { Avatar(initials(b.name), b.colorIndex, 42.dp) },
            trailing = { if (b.favorite) Icon(Icons.Rounded.Star, null, tint = Brand.Warning, modifier = Modifier.size(18.dp)) }, onClick = onClick, chevron = false)
    }
}

@Composable
fun AddBeneficiarySheet(vm: AppViewModel, onDismiss: () -> Unit, onAdded: (Beneficiary) -> Unit) {
    val c = Essa.colors
    var name by remember { mutableStateOf("") }
    var ref by remember { mutableStateOf("") }
    var note by remember { mutableStateOf("") }
    val added = tr("Bénéficiaire fictif ajouté.", "Fictional beneficiary added.")
    EssaSheet(onDismiss) {
        Text(tr("Nouveau bénéficiaire fictif", "New fictional beneficiary"), style = EssaType.h2, color = c.text)
        Spacer(Modifier.height(4.dp))
        Text(tr("N'utilisez jamais de vraies coordonnées bancaires : une référence fictive ESSA-SIM est générée automatiquement.",
            "Never use real bank details: a fictional ESSA-SIM reference is generated automatically."), style = EssaType.small, color = c.textMuted)
        Spacer(Modifier.height(12.dp))
        EssaTextField(name, { name = it }, tr("Nom", "Name"), maxLength = 40)
        Spacer(Modifier.height(8.dp))
        EssaTextField(ref, { ref = it.uppercase() }, tr("Référence fictive (facultatif)", "Fictional reference (optional)"), maxLength = 22,
            supporting = tr("Format ESSA-SIM-0000-0000-00", "Format ESSA-SIM-0000-0000-00"))
        Spacer(Modifier.height(8.dp))
        EssaTextField(note, { note = it }, tr("Note (facultatif)", "Note (optional)"), maxLength = 60)
        Spacer(Modifier.height(16.dp))
        PrimaryButton(tr("Ajouter", "Add"), enabled = name.isNotBlank(), onClick = {
            vm.run(added, onOk = { v -> (v as? Beneficiary)?.let(onAdded) }) { addBeneficiary(it, name, ref.ifBlank { null }, note, (name.hashCode() and 7)) }
        })
    }
}

@Composable
fun BeneficiariesScreen(vm: AppViewModel, nav: Nav) {
    val s = vm.state.collectAsStateWithLifecycle().value ?: return
    val c = Essa.colors
    var add by remember { mutableStateOf(false) }
    var open by remember { mutableStateOf<String?>(null) }
    var delete by remember { mutableStateOf<String?>(null) }
    ScreenScaffold(tr("Bénéficiaires fictifs", "Fictional beneficiaries"), nav::back, actions = {
        RoundIconButton(Icons.Rounded.PersonAdd, tr("Ajouter", "Add"), { add = true })
    }) {
        if (s.beneficiaries.isEmpty()) item { EmptyState(Icons.Rounded.Group, tr("Aucun bénéficiaire", "No beneficiary"), tr("Ajoutez des proches fictifs pour leur envoyer de l'argent virtuel.", "Add fictional contacts to send them virtual money."), tr("Ajouter", "Add")) { add = true } }
        else item {
            Panel(padding = 8.dp) {
                s.beneficiaries.sortedWith(compareByDescending<Beneficiary> { it.favorite }.thenBy { it.name }).forEach { b -> BeneficiaryRow(b) { open = b.id } }
            }
        }
    }
    if (add) AddBeneficiarySheet(vm, { add = false }) { add = false }
    open?.let { id -> s.beneficiary(id)?.let { b ->
        var note by remember(id) { mutableStateOf(b.note) }
        val sentCount = s.transactions.count { it.counterparty == b.name && it.kind == TxKind.TRANSFER_OUT }
        EssaSheet({ open = null }) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Avatar(initials(b.name), b.colorIndex, 56.dp)
                Spacer(Modifier.width(12.dp))
                Column(Modifier.weight(1f)) {
                    Text(b.name, style = EssaType.h2, color = c.text)
                    Text(b.reference + " · " + b.bankLabel, style = EssaType.small, color = c.textMuted)
                }
                RoundIconButton(if (b.favorite) Icons.Rounded.Star else Icons.Rounded.StarBorder, tr("Favori", "Favorite"), {
                    vm.run { updateBeneficiary(it, id, favorite = !b.favorite) }
                }, tint = if (b.favorite) Brand.Warning else c.textMuted)
            }
            Spacer(Modifier.height(10.dp))
            Text(tr("$sentCount virement(s) simulé(s) envoyé(s)", "$sentCount simulated transfer(s) sent"), style = EssaType.small, color = c.textMuted)
            Spacer(Modifier.height(12.dp))
            EssaTextField(note, { note = it }, tr("Note", "Note"), maxLength = 60)
            Spacer(Modifier.height(14.dp))
            PrimaryButton(tr("Envoyer de l'argent virtuel", "Send virtual money"), { open = null; nav.go(Routes.transfer("beneficiary", to = id)) }, icon = Icons.AutoMirrored.Rounded.Send)
            Spacer(Modifier.height(10.dp))
            Row(horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                SecondaryButton(tr("Enregistrer la note", "Save note"), { vm.run(onOk = { open = null }) { updateBeneficiary(it, id, note = note) } }, Modifier.weight(1f))
                SecondaryButton(tr("Supprimer", "Delete"), { delete = id; open = null }, Modifier.weight(1f), icon = Icons.Rounded.Delete, danger = true)
            }
        }
    } }
    delete?.let { id ->
        val deleted = tr("Bénéficiaire supprimé.", "Beneficiary deleted.")
        ConfirmDialog(tr("Supprimer ce bénéficiaire ?", "Delete this beneficiary?"), tr("Ses virements programmés seront annulés. L'historique est conservé.", "Its scheduled transfers will be cancelled. History is kept."),
            tr("Supprimer", "Delete"), danger = true, onConfirm = { vm.run(deleted) { deleteBeneficiary(it, id) } }, onDismiss = { delete = null })
    }
}

// ================================================================ Virements programmés

@Composable
fun ScheduledScreen(vm: AppViewModel, nav: Nav) {
    val s = vm.state.collectAsStateWithLifecycle().value ?: return
    val c = Essa.colors
    val lang = LocalLang.current
    var cancel by remember { mutableStateOf<String?>(null) }
    ScreenScaffold(tr("Virements programmés", "Scheduled transfers"), nav::back, actions = {
        RoundIconButton(Icons.Rounded.Add, tr("Nouveau", "New"), { nav.go(Routes.transfer("internal")) })
    }) {
        if (s.scheduled.isEmpty()) item { EmptyState(Icons.Rounded.Schedule, tr("Aucune programmation", "Nothing scheduled"), tr("Programmez un virement simulé ponctuel ou récurrent.", "Schedule a one-off or recurring simulated transfer."), tr("Programmer", "Schedule")) { nav.go(Routes.transfer("internal")) } }
        items(s.scheduled.sortedWith(compareByDescending<com.superessence.essabank.core.model.ScheduledTransfer> { it.active }.thenBy { it.nextDate }), key = { it.id }) { sch ->
            val from = s.account(sch.fromAccountId)?.name ?: "—"
            val to = sch.toAccountId?.let { s.account(it)?.name } ?: sch.beneficiaryId?.let { s.beneficiary(it)?.name } ?: "—"
            Panel {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    IconBadge(if (sch.frequency == Frequency.ONCE) Icons.Rounded.Event else Icons.Rounded.Repeat, if (sch.active) c.accent else c.textMuted, 42.dp)
                    Spacer(Modifier.width(12.dp))
                    Column(Modifier.weight(1f)) {
                        Text(sch.label, style = EssaType.title, color = c.text, maxLines = 1, overflow = TextOverflow.Ellipsis)
                        Text("$from → $to", style = EssaType.small, color = c.textMuted, maxLines = 1, overflow = TextOverflow.Ellipsis)
                    }
                    Text(fmt(sch.amount), style = EssaType.amount, color = c.text)
                }
                Spacer(Modifier.height(10.dp))
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Column(Modifier.weight(1f)) {
                        Text(L.frequency(sch.frequency, lang) + " · " + if (sch.active) tr("prochaine exécution le ", "next run on ") + L.date(LocalDate.ofEpochDay(sch.nextDate), lang) else tr("terminé ou suspendu", "finished or paused"),
                            style = EssaType.small, color = c.textSecondary)
                        Text(tr("${sch.executions} exécution(s) · ${sch.failures} échec(s)", "${sch.executions} run(s) · ${sch.failures} failure(s)"), style = EssaType.small, color = c.textMuted)
                    }
                    Switch(sch.active, { v -> vm.run { setScheduledActive(it, sch.id, v) } }, colors = SwitchDefaults.colors(checkedTrackColor = c.accent))
                    Spacer(Modifier.width(6.dp))
                    RoundIconButton(Icons.Rounded.Delete, tr("Supprimer", "Delete"), { cancel = sch.id }, tint = c.negative, size = 38.dp)
                }
            }
        }
        item {
            Text(tr("Les virements programmés sont exécutés automatiquement à l'ouverture de l'application, dès leur date d'échéance. Si le solde virtuel est insuffisant, l'échéance est notifiée et reportée à la suivante.",
                "Scheduled transfers run automatically when the app opens, as soon as they are due. If the virtual balance is insufficient, you are notified and the run moves to the next date."),
                style = EssaType.small, color = c.textMuted)
        }
    }
    cancel?.let { id ->
        val done = tr("Programmation supprimée.", "Schedule deleted.")
        ConfirmDialog(tr("Supprimer la programmation ?", "Delete this schedule?"), tr("Les prochaines exécutions n'auront pas lieu.", "Future runs will not happen."), tr("Supprimer", "Delete"), danger = true,
            onConfirm = { vm.run(done) { cancelScheduled(it, id) } }, onDismiss = { cancel = null })
    }
}
