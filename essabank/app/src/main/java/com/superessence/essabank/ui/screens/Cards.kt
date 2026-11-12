package com.superessence.essabank.ui.screens

import androidx.compose.animation.core.Animatable
import androidx.compose.animation.core.Spring
import androidx.compose.animation.core.spring
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
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.LazyRow
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.pager.HorizontalPager
import androidx.compose.foundation.pager.rememberPagerState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.Add
import androidx.compose.material.icons.rounded.Block
import androidx.compose.material.icons.rounded.CheckCircle
import androidx.compose.material.icons.rounded.CreditCard
import androidx.compose.material.icons.rounded.Delete
import androidx.compose.material.icons.rounded.Edit
import androidx.compose.material.icons.rounded.Lock
import androidx.compose.material.icons.rounded.LockOpen
import androidx.compose.material.icons.rounded.Palette
import androidx.compose.material.icons.rounded.Public
import androidx.compose.material.icons.rounded.Settings
import androidx.compose.material.icons.rounded.ShoppingBag
import androidx.compose.material.icons.rounded.Visibility
import androidx.compose.material.icons.rounded.VisibilityOff
import androidx.compose.material.icons.rounded.Wifi
import androidx.compose.material.icons.rounded.WorkspacePremium
import androidx.compose.material3.Icon
import androidx.compose.material3.Slider
import androidx.compose.material3.SliderDefaults
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.superessence.essabank.core.engine.EngineResult
import com.superessence.essabank.core.model.BankState
import com.superessence.essabank.core.model.Card
import com.superessence.essabank.core.model.CardDesign
import com.superessence.essabank.core.model.CardStatus
import com.superessence.essabank.core.model.CardType
import com.superessence.essabank.core.model.Category
import com.superessence.essabank.core.model.Tier
import com.superessence.essabank.core.money.Money
import com.superessence.essabank.core.money.ParseResult
import com.superessence.essabank.ui.AppViewModel
import com.superessence.essabank.ui.Nav
import com.superessence.essabank.ui.Routes
import com.superessence.essabank.ui.components.BankCard
import com.superessence.essabank.ui.components.CardStyles
import com.superessence.essabank.ui.components.Chip
import com.superessence.essabank.ui.components.ConfirmDialog
import com.superessence.essabank.ui.components.EmptyState
import com.superessence.essabank.ui.components.EssaSheet
import com.superessence.essabank.ui.components.EssaTextField
import com.superessence.essabank.ui.components.IconBadge
import com.superessence.essabank.ui.components.ListRow
import com.superessence.essabank.ui.components.Panel
import com.superessence.essabank.ui.components.PrimaryButton
import com.superessence.essabank.ui.components.ProgressBar
import com.superessence.essabank.ui.components.QuickAction
import com.superessence.essabank.ui.components.RoundIconButton
import com.superessence.essabank.ui.components.ScreenScaffold
import com.superessence.essabank.ui.components.SectionHeader
import com.superessence.essabank.ui.components.SimulationPill
import com.superessence.essabank.ui.components.ToggleRow
import com.superessence.essabank.ui.components.fmt
import com.superessence.essabank.ui.components.pressable
import com.superessence.essabank.ui.i18n.L
import com.superessence.essabank.ui.i18n.LocalLang
import com.superessence.essabank.ui.i18n.tr
import com.superessence.essabank.ui.theme.Dimens
import com.superessence.essabank.ui.theme.Essa
import com.superessence.essabank.ui.theme.EssaType
import kotlinx.coroutines.launch
import kotlin.math.absoluteValue

@Composable
fun CardsScreen(vm: AppViewModel, nav: Nav) {
    val s = vm.state.collectAsStateWithLifecycle().value ?: return
    val c = Essa.colors
    val lang = LocalLang.current
    val cards = s.cards
    val pager = rememberPagerState { cards.size }
    var flipped by remember { mutableStateOf(false) }
    var paySheet by remember { mutableStateOf<String?>(null) }

    LazyColumn(
        Modifier.fillMaxSize().background(c.background),
        contentPadding = PaddingValues(bottom = 120.dp),
        verticalArrangement = Arrangement.spacedBy(14.dp),
    ) {
        item {
            Row(Modifier.fillMaxWidth().statusBarsPadding().padding(horizontal = Dimens.screen).padding(top = 14.dp), verticalAlignment = Alignment.CenterVertically) {
                Column(Modifier.weight(1f)) {
                    Text(tr("Mes cartes", "My cards"), style = EssaType.h1, color = c.text)
                    Text(tr("${cards.size} carte(s) fictive(s) · ${L.tierName(s.profile.tier)} : ${s.profile.tier.maxCards} max", "${cards.size} fictional card(s) · ${L.tierName(s.profile.tier)}: ${s.profile.tier.maxCards} max"),
                        style = EssaType.small, color = c.textMuted)
                }
                RoundIconButton(Icons.Rounded.Add, tr("Nouvelle carte", "New card"), { nav.go(Routes.NEW_CARD) }, tint = c.onAccent, background = c.accent)
            }
        }
        if (cards.isEmpty()) {
            item { EmptyState(Icons.Rounded.CreditCard, tr("Aucune carte", "No card"), tr("Créez une carte virtuelle fictive en quelques secondes.", "Create a fictional virtual card in seconds."), tr("Créer une carte", "Create a card")) { nav.go(Routes.NEW_CARD) } }
            return@LazyColumn
        }
        item {
            HorizontalPager(pager, contentPadding = PaddingValues(horizontal = 34.dp), pageSpacing = 14.dp, modifier = Modifier.fillMaxWidth()) { page ->
                val offset = ((pager.currentPage - page) + pager.currentPageOffsetFraction)
                val card = cards[page]
                Box(
                    Modifier.graphicsLayer {
                        val o = offset.coerceIn(-1f, 1f)
                        rotationY = o * -18f
                        scaleX = 1f - 0.08f * o.absoluteValue
                        scaleY = 1f - 0.08f * o.absoluteValue
                        alpha = 1f - 0.35f * o.absoluteValue
                        cameraDistance = 14f * density
                    }.padding(vertical = 10.dp),
                ) {
                    BankCard(card, flipped = flipped && page == pager.currentPage, interactive = page == pager.currentPage, showDetails = false)
                }
            }
        }
        val card = cards.getOrNull(pager.currentPage) ?: cards.first()
        item {
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.Center) {
                cards.indices.forEach { i ->
                    Box(Modifier.padding(3.dp).size(width = if (i == pager.currentPage) 18.dp else 6.dp, height = 6.dp).clip(RoundedCornerShape(3.dp))
                        .background(if (i == pager.currentPage) c.accent else c.border))
                }
            }
        }
        item {
            Row(Modifier.fillMaxWidth().padding(horizontal = Dimens.screen), horizontalArrangement = Arrangement.SpaceBetween) {
                QuickAction(if (flipped) Icons.Rounded.VisibilityOff else Icons.Rounded.Visibility, if (flipped) tr("Recto", "Front") else tr("Verso", "Back"), { flipped = !flipped }, Modifier.weight(1f))
                QuickAction(if (card.status == CardStatus.LOCKED) Icons.Rounded.LockOpen else Icons.Rounded.Lock,
                    if (card.status == CardStatus.LOCKED) tr("Déverrouiller", "Unlock") else tr("Verrouiller", "Lock"),
                    { vm.run { setCardStatus(it, card.id, if (card.status == CardStatus.LOCKED) CardStatus.ACTIVE else CardStatus.LOCKED) } }, Modifier.weight(1f))
                QuickAction(Icons.Rounded.ShoppingBag, tr("Payer", "Pay"), { paySheet = card.id }, Modifier.weight(1f), highlight = true)
                QuickAction(Icons.Rounded.Settings, tr("Gérer", "Manage"), { nav.go(Routes.card(card.id)) }, Modifier.weight(1f))
            }
        }
        item { Box(Modifier.padding(horizontal = Dimens.screen)) { CardLimitPanel(vm, s, card) } }
        item { Box(Modifier.padding(horizontal = Dimens.screen)) { SectionHeader(tr("Paiements simulés", "Simulated payments"), tr("Détails", "Details")) { nav.go(Routes.card(card.id)) } } }
        item {
            Box(Modifier.padding(horizontal = Dimens.screen)) {
                Panel(padding = 8.dp) {
                    val txs = s.transactions.filter { it.cardId == card.id }.sortedByDescending { it.timestamp }.take(6)
                    if (txs.isEmpty()) Text(tr("Aucun paiement avec cette carte. Essayez le simulateur de paiement.", "No payment with this card yet. Try the payment simulator."),
                        style = EssaType.body, color = c.textMuted, modifier = Modifier.padding(12.dp))
                    txs.forEach { TransactionRow(it, s, vm.zone, { nav.go(Routes.tx(it.id)) }) }
                }
            }
        }
    }
    paySheet?.let { id -> s.card(id)?.let { PaymentSimulatorSheet(vm, it) { paySheet = null } } }
}

@Composable
private fun CardLimitPanel(vm: AppViewModel, s: BankState, card: Card) {
    val c = Essa.colors
    val spent = remember(s, card.id) { vm.engine.cardSpentThisMonth(s, card.id) }
    val ratio = if (card.monthlyLimit.cents > 0) spent.cents.toFloat() / card.monthlyLimit.cents else 0f
    Panel {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Column(Modifier.weight(1f)) {
                Text(tr("Plafond mensuel virtuel", "Virtual monthly limit"), style = EssaType.small, color = c.textMuted)
                Text(fmt(spent) + " / " + fmt(card.monthlyLimit, decimals = false), style = EssaType.title, color = c.text)
            }
            Text(L.cardStatus(card.status, LocalLang.current), style = EssaType.bodyStrong, color = if (card.status == CardStatus.ACTIVE) c.positive else c.warning)
        }
        Spacer(Modifier.height(10.dp))
        ProgressBar(ratio, if (ratio >= 1f) c.negative else if (ratio > 0.8f) c.warning else c.accent)
        Spacer(Modifier.height(8.dp))
        Text(s.account(card.accountId)?.let { tr("Débitée sur : ", "Charged to: ") + it.name } ?: "", style = EssaType.small, color = c.textMuted)
    }
}

/** Simulateur de paiement chez un commerçant fictif : débite réellement le solde virtuel. */
@Composable
fun PaymentSimulatorSheet(vm: AppViewModel, card: Card, onDismiss: () -> Unit) {
    val c = Essa.colors
    val lang = LocalLang.current
    var merchant by remember { mutableStateOf("") }
    var amountText by remember { mutableStateOf("") }
    var category by remember { mutableStateOf(Category.SHOPPING) }
    var online by remember { mutableStateOf(false) }
    var busy by remember { mutableStateOf(false) }
    val requestId = remember { java.util.UUID.randomUUID().toString() }
    val parsed = Money.parse(amountText)
    val suggestions = listOf("Librairie Papyrus", "Café Horizon", "Marché Lumière", "Cinéma Odyssée", "Boutique Céleste")
    EssaSheet(onDismiss) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Text(tr("Simulateur de paiement", "Payment simulator"), style = EssaType.h2, color = c.text, modifier = Modifier.weight(1f))
            SimulationPill()
        }
        Spacer(Modifier.height(4.dp))
        Text(tr("Carte ${card.label} ${card.maskedNumber}. Aucun commerçant réel n'est contacté.", "Card ${card.label} ${card.maskedNumber}. No real merchant is contacted."), style = EssaType.small, color = c.textMuted)
        Spacer(Modifier.height(14.dp))
        EssaTextField(merchant, { merchant = it }, tr("Commerçant fictif", "Fictional merchant"), maxLength = 40)
        Spacer(Modifier.height(8.dp))
        LazyRow(horizontalArrangement = Arrangement.spacedBy(8.dp)) { items(suggestions) { m -> Chip(m, merchant == m) { merchant = m } } }
        Spacer(Modifier.height(10.dp))
        EssaTextField(amountText, { amountText = it }, tr("Montant (UV)", "Amount (UV)"), keyboard = KeyboardType.Decimal, maxLength = 12,
            isError = amountText.isNotEmpty() && parsed !is ParseResult.Valid,
            supporting = if (amountText.isNotEmpty() && parsed !is ParseResult.Valid) tr("Montant invalide", "Invalid amount") else null)
        Spacer(Modifier.height(10.dp))
        LazyRow(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            items(Category.spending.filter { it != Category.TRANSFERS }) { cat -> Chip(L.category(cat, lang), cat == category, L.categoryIcon(cat)) { category = cat } }
        }
        Spacer(Modifier.height(6.dp))
        ToggleRow(tr("Paiement en ligne", "Online payment"), null, online, Icons.Rounded.Public) { online = it }
        Spacer(Modifier.height(12.dp))
        val okMsg = tr("Paiement virtuel effectué.", "Virtual payment made.")
        PrimaryButton(tr("Payer ", "Pay ") + ((parsed as? ParseResult.Valid)?.let { fmt(it.money, hideable = false) } ?: ""), loading = busy,
            enabled = merchant.isNotBlank() && parsed is ParseResult.Valid, onClick = {
                val amount = (parsed as? ParseResult.Valid)?.money ?: return@PrimaryButton
                busy = true
                vm.run(okMsg, onOk = { onDismiss() }) { cardPayment(it, card.id, merchant, category, amount, online, requestId) }.invokeOnCompletion { busy = false }
            })
    }
}

@Composable
fun CardDetailScreen(vm: AppViewModel, nav: Nav, id: String) {
    val s = vm.state.collectAsStateWithLifecycle().value ?: return
    val card = s.card(id)
    if (card == null) { LaunchedEffect(Unit) { nav.back() }; return }
    val c = Essa.colors
    val lang = LocalLang.current
    var flipped by remember { mutableStateOf(false) }
    var reveal by remember { mutableStateOf(false) }
    var pay by remember { mutableStateOf(false) }
    var rename by remember { mutableStateOf(false) }
    var delete by remember { mutableStateOf(false) }
    var limit by remember(card.monthlyLimit) { mutableFloatStateOf(card.monthlyLimit.cents / 100f) }
    val premiumOk = s.profile.tier.premiumDesigns
    val maxLimit = if (card.type == CardType.PREMIUM) 20_000f else 10_000f
    val savedLimit = tr("Plafond virtuel mis à jour.", "Virtual limit updated.")
    val designLocked = tr("Design réservé à ESSA BLACK (gratuit).", "Design reserved to ESSA BLACK (free).")

    ScreenScaffold(card.label, nav::back) {
        item {
            Box(Modifier.padding(vertical = 8.dp)) { BankCard(card, flipped = flipped, showDetails = reveal, interactive = true) }
            Text(tr("Faites glisser la carte pour l'incliner · touchez « Verso » pour la retourner", "Drag the card to tilt it · tap \"Back\" to flip it"),
                style = EssaType.small, color = c.textMuted, textAlign = TextAlign.Center, modifier = Modifier.fillMaxWidth())
        }
        item {
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
                QuickAction(if (reveal) Icons.Rounded.VisibilityOff else Icons.Rounded.Visibility, if (reveal) tr("Masquer", "Hide") else tr("Détails", "Details"), { reveal = !reveal }, Modifier.weight(1f))
                QuickAction(Icons.Rounded.CreditCard, if (flipped) tr("Recto", "Front") else tr("Verso", "Back"), { flipped = !flipped }, Modifier.weight(1f))
                QuickAction(if (card.status == CardStatus.LOCKED) Icons.Rounded.LockOpen else Icons.Rounded.Lock,
                    if (card.status == CardStatus.LOCKED) tr("Déverrouiller", "Unlock") else tr("Verrouiller", "Lock"),
                    { vm.run { setCardStatus(it, id, if (card.status == CardStatus.LOCKED) CardStatus.ACTIVE else CardStatus.LOCKED) } }, Modifier.weight(1f))
                QuickAction(Icons.Rounded.ShoppingBag, tr("Payer", "Pay"), { pay = true }, Modifier.weight(1f), highlight = true)
            }
        }
        if (reveal) item {
            Panel {
                Text(tr("Données fictives de la carte", "Fictional card data"), style = EssaType.small, color = c.textMuted)
                Spacer(Modifier.height(6.dp))
                Text(card.formattedNumber, style = EssaType.mono, color = c.text)
                Text(tr("Expire ", "Expires ") + card.expiry + "  ·  CVV " + card.cvv, style = EssaType.bodyStrong, color = c.textSecondary)
                Spacer(Modifier.height(8.dp))
                Text(tr("Numéro commençant par 0 et à clé de contrôle invalide : il est refusé par tout réseau de paiement réel.",
                    "Number starting with 0 with an invalid check digit: it is rejected by any real payment network."), style = EssaType.small, color = c.textMuted)
            }
        }
        item { CardLimitPanel(vm, s, card) }
        item { SectionHeader(tr("Plafond mensuel virtuel", "Virtual monthly limit")) }
        item {
            Panel {
                Text(fmt(Money.units(limit.toLong()), hideable = false, decimals = false), style = EssaType.h2, color = c.text)
                Slider(limit, { limit = (it / 50f).toInt() * 50f }, valueRange = 50f..maxLimit, onValueChangeFinished = {
                    vm.run(savedLimit) { st -> updateCard(st, id, monthlyLimit = Money.units(limit.toLong())) }
                }, colors = SliderDefaults.colors(thumbColor = c.accent, activeTrackColor = c.accent, inactiveTrackColor = c.surfaceAlt))
                Row { Text(fmt(Money.units(50), hideable = false, decimals = false), style = EssaType.small, color = c.textMuted, modifier = Modifier.weight(1f)); Text(fmt(Money.units(maxLimit.toLong()), hideable = false, decimals = false), style = EssaType.small, color = c.textMuted) }
            }
        }
        item { SectionHeader(tr("Design", "Design")) }
        item {
            LazyRow(horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                items(CardDesign.entries.toList()) { d ->
                    val locked = d.premium && !premiumOk
                    Column(horizontalAlignment = Alignment.CenterHorizontally, modifier = Modifier.pressable {
                        if (locked) vm.toast(designLocked, true) else vm.run { updateCard(it, id, design = d) }
                    }) {
                        Box(
                            Modifier.size(78.dp, 50.dp).clip(RoundedCornerShape(10.dp)).background(Brush.linearGradient(CardStyles.colors(d)))
                                .border(if (card.design == d) 3.dp else 1.dp, if (card.design == d) c.accent else c.border, RoundedCornerShape(10.dp)),
                            contentAlignment = Alignment.Center,
                        ) { if (locked) Icon(Icons.Rounded.WorkspacePremium, null, tint = Color.White) }
                        Spacer(Modifier.height(4.dp))
                        Text(L.design(d, lang), style = EssaType.small, color = if (card.design == d) c.text else c.textMuted)
                    }
                }
            }
        }
        item { SectionHeader(tr("Paramètres", "Settings")) }
        item {
            Panel(padding = 8.dp) {
                ToggleRow(tr("Carte active", "Card enabled"), tr("Désactivez la carte pour bloquer tout paiement simulé", "Disable the card to block every simulated payment"),
                    card.status != CardStatus.INACTIVE, Icons.Rounded.CheckCircle) { on -> vm.run { setCardStatus(it, id, if (on) CardStatus.ACTIVE else CardStatus.INACTIVE) } }
                ToggleRow(tr("Paiements en ligne", "Online payments"), null, card.onlinePayments, Icons.Rounded.Public) { v -> vm.run { updateCard(it, id, onlinePayments = v) } }
                ToggleRow(tr("Sans contact simulé", "Simulated contactless"), null, card.contactless, Icons.Rounded.Wifi) { v -> vm.run { updateCard(it, id, contactless = v) } }
                ListRow(tr("Renommer la carte", "Rename card"), card.label, leading = { IconBadge(Icons.Rounded.Edit, c.accent, 38.dp) }, onClick = { rename = true })
                ListRow(L.cardType(card.type, lang), L.cardTypeHint(card.type, lang) + (card.usesLeft?.let { tr(" Utilisations restantes : $it.", " Uses left: $it.") } ?: ""),
                    leading = { IconBadge(Icons.Rounded.Palette, c.accent, 38.dp) })
                ListRow(tr("Supprimer la carte", "Delete card"), null, leading = { IconBadge(Icons.Rounded.Delete, c.negative, 38.dp) }, onClick = { delete = true }, titleColor = c.negative)
            }
        }
        item { SectionHeader(tr("Historique des paiements simulés", "Simulated payment history")) }
        item {
            Panel(padding = 8.dp) {
                val txs = s.transactions.filter { it.cardId == id }.sortedByDescending { it.timestamp }
                if (txs.isEmpty()) Text(tr("Aucun paiement pour l'instant.", "No payment yet."), style = EssaType.body, color = c.textMuted, modifier = Modifier.padding(12.dp))
                txs.take(30).forEach { TransactionRow(it, s, vm.zone, { nav.go(Routes.tx(it.id)) }) }
            }
        }
    }
    if (pay) PaymentSimulatorSheet(vm, card) { pay = false }
    if (rename) RenameSheet(tr("Renommer la carte", "Rename card"), card.label, onDismiss = { rename = false }) { name -> vm.run(onOk = { rename = false }) { updateCard(it, id, label = name) } }
    if (delete) {
        val deleted = tr("Carte supprimée.", "Card deleted.")
        ConfirmDialog(tr("Supprimer cette carte ?", "Delete this card?"), tr("La carte fictive sera définitivement supprimée. L'historique de ses paiements est conservé.", "The fictional card will be permanently deleted. Its payment history is kept."),
            tr("Supprimer", "Delete"), danger = true, onConfirm = { vm.run(deleted, onOk = { nav.back() }) { deleteCard(it, id) } }, onDismiss = { delete = false })
    }
}

@Composable
fun RenameSheet(title: String, initial: String, onDismiss: () -> Unit, onSave: (String) -> Unit) {
    val c = Essa.colors
    var text by remember { mutableStateOf(initial) }
    EssaSheet(onDismiss) {
        Text(title, style = EssaType.h2, color = c.text)
        Spacer(Modifier.height(14.dp))
        EssaTextField(text, { text = it }, tr("Nom", "Name"), maxLength = 32)
        Spacer(Modifier.height(16.dp))
        PrimaryButton(tr("Enregistrer", "Save"), enabled = text.isNotBlank(), onClick = { onSave(text) })
    }
}

@Composable
fun NewCardScreen(vm: AppViewModel, nav: Nav) {
    val s = vm.state.collectAsStateWithLifecycle().value ?: return
    val c = Essa.colors
    val lang = LocalLang.current
    val tier = s.profile.tier
    var type by remember { mutableStateOf(CardType.VIRTUAL) }
    var account by remember { mutableStateOf(s.primaryAccount?.id ?: s.accounts.first().id) }
    var label by remember { mutableStateOf(L.cardType(CardType.VIRTUAL, lang)) }
    var labelEdited by remember { mutableStateOf(false) }
    var design by remember { mutableStateOf(CardDesign.ELECTRIC) }
    var limit by remember { mutableIntStateOf(1500) }
    var busy by remember { mutableStateOf(false) }
    val preview = Card("preview", account, type, label.ifBlank { "—" }, s.profile.fullName.uppercase(), "0000000000000000", 12, 2030, "000", design,
        monthlyLimit = Money.units(limit), createdAt = 0)
    val appear = remember { Animatable(0.85f) }
    LaunchedEffect(design, type) { appear.snapTo(0.94f); appear.animateTo(1f, spring(Spring.DampingRatioMediumBouncy, Spring.StiffnessLow)) }
    val created = tr("Carte virtuelle créée.", "Virtual card created.")
    val needPlus = tr("La carte premium nécessite ESSA PLUS (gratuit).", "Premium card requires ESSA PLUS (free).")
    val limitReached = s.cards.size >= tier.maxCards

    ScreenScaffold(tr("Nouvelle carte fictive", "New fictional card"), nav::back, bottomBar = {
        if (limitReached) PrimaryButton(tr("Changer de formule (gratuit)", "Change plan (free)"), { nav.go(Routes.PLANS) }, icon = Icons.Rounded.WorkspacePremium)
        else PrimaryButton(tr("Créer la carte", "Create card"), loading = busy, enabled = label.isNotBlank(), onClick = {
            busy = true
            vm.run(created, onOk = { v -> (v as? Card)?.let { nav.replace(Routes.card(it.id)) } }) { createCard(it, account, type, label, design, Money.units(limit)) }
                .invokeOnCompletion { busy = false }
        })
    }) {
        item { Box(Modifier.graphicsLayer { scaleX = appear.value; scaleY = appear.value }.padding(vertical = 6.dp)) { BankCard(preview, interactive = true) } }
        if (limitReached) item {
            Panel { Text(tr("Vous avez atteint le nombre maximal de cartes de la formule ${L.tierName(tier)} (${tier.maxCards}).", "You reached the maximum number of cards of the ${L.tierName(tier)} plan (${tier.maxCards})."), style = EssaType.body, color = c.warning) }
        }
        item { SectionHeader(tr("Type de carte", "Card type")) }
        items(CardType.entries.toList()) { t ->
            val locked = t == CardType.PREMIUM && tier == Tier.STANDARD
            Box(Modifier.clip(RoundedCornerShape(16.dp)).background(if (t == type) c.accentSoft else Color.Transparent)) {
                ListRow(L.cardType(t, lang), L.cardTypeHint(t, lang), leading = { IconBadge(if (locked) Icons.Rounded.WorkspacePremium else Icons.Rounded.CreditCard, if (t == type) c.accent else c.textMuted, 40.dp) },
                    onClick = {
                        if (locked) vm.toast(needPlus, true)
                        else { type = t; if (!labelEdited) label = L.cardType(t, lang); if (t == CardType.PREMIUM) limit = maxOf(limit, 5000) }
                    }, chevron = false, trailing = { if (t == type) Icon(Icons.Rounded.CheckCircle, null, tint = c.accent) })
            }
        }
        item { SectionHeader(tr("Compte débité", "Charged account")) }
        items(s.accounts.filter { it.type != com.superessence.essabank.core.model.AccountType.INVESTMENT }, key = { it.id }) { a ->
            AccountRow(a, { account = a.id }, selected = a.id == account)
        }
        item { SectionHeader(tr("Nom et design", "Name and design")) }
        item { EssaTextField(label, { label = it; labelEdited = true }, tr("Nom de la carte", "Card name"), maxLength = 32) }
        item {
            LazyRow(horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                items(CardDesign.entries.toList()) { d ->
                    val locked = d.premium && !tier.premiumDesigns
                    Box(
                        Modifier.size(72.dp, 46.dp).clip(RoundedCornerShape(10.dp)).background(Brush.linearGradient(CardStyles.colors(d)))
                            .border(if (design == d) 3.dp else 1.dp, if (design == d) c.accent else c.border, RoundedCornerShape(10.dp))
                            .pressable { if (!locked) design = d },
                        contentAlignment = Alignment.Center,
                    ) { if (locked) Icon(Icons.Rounded.WorkspacePremium, null, tint = Color.White.copy(alpha = 0.8f)) }
                }
            }
        }
        item { SectionHeader(tr("Plafond mensuel virtuel", "Virtual monthly limit")) }
        item {
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                listOf(300, 800, 1500, 3000, 5000).forEach { v -> Chip(fmt(Money.units(v), hideable = false, decimals = false), limit == v) { limit = v } }
            }
        }
        item {
            Text(tr("Numéro, date d'expiration et CVV seront générés de façon fictive et ne fonctionnent sur aucun réseau réel.",
                "Number, expiry date and CVV will be generated fictitiously and work on no real network."), style = EssaType.small, color = c.textMuted)
        }
    }
}
