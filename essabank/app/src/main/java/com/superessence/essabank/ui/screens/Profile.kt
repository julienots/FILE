package com.superessence.essabank.ui.screens

import android.graphics.Bitmap
import android.graphics.BitmapFactory
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.PickVisualMediaRequest
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ExperimentalLayoutApi
import androidx.compose.foundation.layout.FlowRow
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
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.rounded.Logout
import androidx.compose.material.icons.rounded.AccountBalance
import androidx.compose.material.icons.rounded.AutoAwesome
import androidx.compose.material.icons.rounded.Backup
import androidx.compose.material.icons.rounded.AccountCircle
import androidx.compose.material.icons.rounded.CheckCircle
import androidx.compose.material.icons.rounded.AttachMoney
import androidx.compose.material.icons.rounded.Dashboard
import androidx.compose.material.icons.rounded.Brightness4
import androidx.compose.material.icons.rounded.Delete
import androidx.compose.material.icons.rounded.DeleteForever
import androidx.compose.material.icons.rounded.Edit
import androidx.compose.material.icons.rounded.Email
import androidx.compose.material.icons.rounded.Fingerprint
import androidx.compose.material.icons.rounded.Info
import androidx.compose.material.icons.rounded.KeyboardArrowDown
import androidx.compose.material.icons.rounded.KeyboardArrowUp
import androidx.compose.material.icons.rounded.Language
import androidx.compose.material.icons.rounded.Lock
import androidx.compose.material.icons.rounded.Notifications
import androidx.compose.material.icons.rounded.Palette
import androidx.compose.material.icons.rounded.VpnKey
import androidx.compose.material.icons.rounded.Person
import androidx.compose.material.icons.rounded.PhotoCamera
import androidx.compose.material.icons.rounded.Place
import androidx.compose.material.icons.rounded.Refresh
import androidx.compose.material.icons.rounded.Restore
import androidx.compose.material.icons.rounded.Security
import androidx.compose.material.icons.rounded.Storage
import androidx.compose.material.icons.rounded.Timer
import androidx.compose.material.icons.rounded.Visibility
import androidx.compose.material.icons.rounded.Stars
import androidx.compose.material3.Icon
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
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
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.superessence.essabank.core.model.Accent
import com.superessence.essabank.core.model.HomeWidget
import com.superessence.essabank.core.model.ThemeMode
import com.superessence.essabank.core.model.Tier
import com.superessence.essabank.core.money.AppLanguage
import com.superessence.essabank.core.money.DisplayCurrency
import com.superessence.essabank.core.security.PinHasher
import com.superessence.essabank.data.PinCheck
import com.superessence.essabank.security.Biometric
import com.superessence.essabank.ui.AppViewModel
import com.superessence.essabank.ui.Nav
import com.superessence.essabank.ui.Routes
import com.superessence.essabank.ui.components.AVATAR_COUNT
import com.superessence.essabank.ui.components.Avatar
import com.superessence.essabank.ui.components.Chip
import com.superessence.essabank.ui.components.ColorDot
import com.superessence.essabank.ui.components.ConfirmDialog
import com.superessence.essabank.ui.components.DisclaimerFooter
import com.superessence.essabank.ui.components.EssaMark
import com.superessence.essabank.ui.components.EssaSheet
import com.superessence.essabank.ui.components.EssaTextField
import com.superessence.essabank.ui.components.EssaWordmark
import com.superessence.essabank.ui.components.IconBadge
import com.superessence.essabank.ui.components.KeyValue
import com.superessence.essabank.ui.components.ListRow
import com.superessence.essabank.ui.components.Panel
import com.superessence.essabank.ui.components.PrimaryButton
import com.superessence.essabank.ui.components.RoundIconButton
import com.superessence.essabank.ui.components.ScreenScaffold
import com.superessence.essabank.ui.components.SecondaryButton
import com.superessence.essabank.ui.components.SectionHeader
import com.superessence.essabank.ui.components.SimulationPill
import com.superessence.essabank.ui.components.ToggleRow
import com.superessence.essabank.ui.components.pressable
import com.superessence.essabank.ui.i18n.L
import com.superessence.essabank.ui.i18n.LocalLang
import com.superessence.essabank.ui.i18n.tr
import com.superessence.essabank.ui.theme.Brand
import com.superessence.essabank.ui.theme.Dimens
import com.superessence.essabank.ui.theme.Essa
import com.superessence.essabank.ui.theme.EssaType
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.io.File

// ================================================================ Onglet Profil

@Composable
fun ProfileScreen(vm: AppViewModel, nav: Nav) {
    val s = vm.state.collectAsStateWithLifecycle().value ?: return
    val c = Essa.colors
    val lang = LocalLang.current
    val p = s.profile
    val photo = rememberProfilePhoto(p.photoPath)
    LazyColumn(
        Modifier.fillMaxSize().background(c.background),
        contentPadding = PaddingValues(start = Dimens.screen, end = Dimens.screen, bottom = 120.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp),
    ) {
        item {
            Column(Modifier.fillMaxWidth().statusBarsPadding().padding(top = 18.dp), horizontalAlignment = Alignment.CenterHorizontally) {
                Box(Modifier.pressable { nav.go(Routes.EDIT_PROFILE) }) {
                    Avatar(p.initials, p.avatar, 92.dp, photo)
                    Box(Modifier.align(Alignment.BottomEnd).size(30.dp).clip(RoundedCornerShape(15.dp)).background(c.accent), contentAlignment = Alignment.Center) {
                        Icon(Icons.Rounded.Edit, null, tint = c.onAccent, modifier = Modifier.size(16.dp))
                    }
                }
                Spacer(Modifier.height(12.dp))
                Text(p.fullName, style = EssaType.h2, color = c.text)
                Text(tr("Client fictif n° ", "Fictional client no. ") + p.clientNumber, style = EssaType.small, color = c.textMuted)
                Spacer(Modifier.height(8.dp))
                TierBadge(p.tier) { nav.go(Routes.PLANS) }
            }
        }
        item {
            Panel {
                KeyValue(tr("E-mail fictif", "Fictional e-mail"), p.email.ifBlank { "—" })
                KeyValue(tr("Ville", "City"), p.city.ifBlank { "—" })
                KeyValue(tr("Client depuis", "Customer since"), L.date(p.createdAt, vm.zone, lang))
                KeyValue(tr("Comptes · cartes", "Accounts · cards"), "${s.accounts.size} · ${s.cards.size}")
                KeyValue(tr("Profil", "Profile"), if (p.isDemo) tr("Démonstration", "Demo") else tr("Personnel (fictif)", "Personal (fictional)"))
            }
        }
        item { SectionHeader(tr("Mon espace", "My space")) }
        item {
            Panel(padding = 8.dp) {
                ListRow(tr("Informations du profil", "Profile information"), tr("Nom, photo, coordonnées fictives", "Name, photo, fictional details"), leading = { IconBadge(Icons.Rounded.Person, c.accent, 38.dp) }, onClick = { nav.go(Routes.EDIT_PROFILE) })
                ListRow(tr("Mes comptes", "My accounts"), null, leading = { IconBadge(Icons.Rounded.AccountBalance, c.accent, 38.dp) }, onClick = { nav.go(Routes.ACCOUNTS) })
                ListRow(tr("Formules ESSA", "ESSA plans"), L.tierName(p.tier) + tr(" · gratuit", " · free"), leading = { IconBadge(Icons.Rounded.Stars, Brand.Gold, 38.dp) }, onClick = { nav.go(Routes.PLANS) })
                ListRow(tr("Notifications", "Notifications"), tr("${s.unreadNotifications} non lue(s)", "${s.unreadNotifications} unread"), leading = { IconBadge(Icons.Rounded.Notifications, c.accent, 38.dp) }, onClick = { nav.go(Routes.NOTIFICATIONS) })
            }
        }
        item { SectionHeader(tr("Réglages", "Settings")) }
        item {
            Panel(padding = 8.dp) {
                ListRow(tr("Préférences", "Preferences"), tr("Langue, devise, thème, notifications", "Language, currency, theme, notifications"), leading = { IconBadge(Icons.Rounded.Palette, c.accent, 38.dp) }, onClick = { nav.go(Routes.PREFERENCES) })
                ListRow(tr("Tableau de bord personnalisé", "Custom dashboard"), if (p.tier.customDashboard) tr("Choisir et ordonner les blocs", "Choose and order blocks") else tr("Inclus dans ESSA BLACK", "Included in ESSA BLACK"),
                    leading = { IconBadge(Icons.Rounded.Dashboard, c.accent, 38.dp) }, onClick = { nav.go(if (p.tier.customDashboard) Routes.DASHBOARD else Routes.PLANS) })
                ListRow(tr("Sécurité", "Security"), tr("Code PIN, biométrie, verrouillage", "PIN, biometrics, auto-lock"), leading = { IconBadge(Icons.Rounded.Security, c.accent, 38.dp) }, onClick = { nav.go(Routes.SECURITY) })
                ListRow(tr("Gestion des données", "Data management"), tr("Sauvegarde, restauration, réinitialisation", "Backup, restore, reset"), leading = { IconBadge(Icons.Rounded.Storage, c.accent, 38.dp) }, onClick = { nav.go(Routes.DATA) })
                ListRow(tr("À propos", "About"), "ESSABANK · Superessence Studio", leading = { IconBadge(Icons.Rounded.Info, c.accent, 38.dp) }, onClick = { nav.go(Routes.ABOUT) })
            }
        }
        item {
            SecondaryButton(tr("Verrouiller l'application", "Lock the app"), { vm.lockNow() }, Modifier.fillMaxWidth(), icon = Icons.AutoMirrored.Rounded.Logout)
        }
        item { DisclaimerFooter() }
    }
}

@Composable
private fun TierBadge(tier: Tier, onClick: () -> Unit) {
    val colors = when (tier) {
        Tier.STANDARD -> listOf(Brand.Electric, Brand.Cyan)
        Tier.PLUS -> listOf(Color(0xFF7B5CFF), Brand.Electric)
        Tier.BLACK -> listOf(Color(0xFF111114), Color(0xFF3A3326))
    }
    Row(
        Modifier.clip(RoundedCornerShape(12.dp)).background(Brush.horizontalGradient(colors)).pressable(onClick = onClick).padding(horizontal = 12.dp, vertical = 6.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        EssaMark(16.dp, tile = false)
        Spacer(Modifier.width(6.dp))
        Text(L.tierName(tier), style = EssaType.caption, color = if (tier == Tier.BLACK) Brand.Gold else Color.White)
    }
}

// ================================================================ Profil : édition

@OptIn(ExperimentalLayoutApi::class)
@Composable
fun EditProfileScreen(vm: AppViewModel, nav: Nav) {
    val s = vm.state.collectAsStateWithLifecycle().value ?: return
    val c = Essa.colors
    val context = LocalContext.current
    val scope = rememberCoroutineScope()
    val p = s.profile
    var first by remember { mutableStateOf(p.firstName) }
    var last by remember { mutableStateOf(p.lastName) }
    var email by remember { mutableStateOf(p.email) }
    var city by remember { mutableStateOf(p.city) }
    val photo = rememberProfilePhoto(p.photoPath)
    val saved = tr("Profil mis à jour.", "Profile updated.")
    val photoErr = tr("Image illisible.", "Unreadable image.")
    val picker = rememberLauncherForActivityResult(ActivityResultContracts.PickVisualMedia()) { uri ->
        if (uri != null) scope.launch {
            val path = withContext(Dispatchers.IO) {
                try {
                    val bytes = readUri(context, uri) ?: return@withContext null
                    val src = BitmapFactory.decodeByteArray(bytes, 0, bytes.size) ?: return@withContext null
                    val side = minOf(src.width, src.height)
                    val square = Bitmap.createBitmap(src, (src.width - side) / 2, (src.height - side) / 2, side, side)
                    val scaled = Bitmap.createScaledBitmap(square, 512, 512, true)
                    val file = File(context.filesDir, "profile_photo.jpg")
                    file.outputStream().use { scaled.compress(Bitmap.CompressFormat.JPEG, 88, it) }
                    file.absolutePath + "?v=" + System.currentTimeMillis()
                } catch (e: Exception) { null }
            }
            if (path == null) vm.toast(photoErr, true)
            else vm.run { updateProfile(it) { pr -> pr.copy(photoPath = path.substringBefore('?'), avatar = pr.avatar) } }
        }
    }
    ScreenScaffold(tr("Informations du profil", "Profile information"), nav::back, bottomBar = {
        PrimaryButton(tr("Enregistrer", "Save"), enabled = first.isNotBlank(), onClick = {
            vm.run(saved, onOk = { nav.back() }) { updateProfile(it) { pr -> pr.copy(firstName = first, lastName = last, email = email, city = city) } }
        })
    }) {
        item {
            Column(Modifier.fillMaxWidth(), horizontalAlignment = Alignment.CenterHorizontally) {
                Avatar(p.initials, p.avatar, 96.dp, photo)
                Spacer(Modifier.height(12.dp))
                Row(horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                    SecondaryButton(tr("Choisir une photo", "Choose a photo"), { picker.launch(PickVisualMediaRequest(ActivityResultContracts.PickVisualMedia.ImageOnly)) }, icon = Icons.Rounded.PhotoCamera)
                    if (p.photoPath != null) SecondaryButton(tr("Retirer", "Remove"), {
                        p.photoPath?.let { File(it).delete() }
                        vm.run { updateProfile(it) { pr -> pr.copy(photoPath = null) } }
                    }, icon = Icons.Rounded.Delete, danger = true)
                }
                Spacer(Modifier.height(6.dp))
                Text(tr("La photo reste sur ce téléphone, dans l'espace privé de l'application.", "The photo stays on this phone, in the app's private storage."), style = EssaType.small, color = c.textMuted, textAlign = TextAlign.Center)
            }
        }
        item { SectionHeader(tr("Ou un avatar", "Or an avatar")) }
        item {
            FlowRow(horizontalArrangement = Arrangement.spacedBy(10.dp), verticalArrangement = Arrangement.spacedBy(10.dp)) {
                (0 until AVATAR_COUNT).forEach { i ->
                    Box(Modifier.clip(RoundedCornerShape(30.dp)).border(if (p.avatar == i && p.photoPath == null) 3.dp else 0.dp, c.accent, RoundedCornerShape(30.dp))
                        .pressable { vm.run { updateProfile(it) { pr -> pr.copy(avatar = i, photoPath = null) } } }.padding(3.dp)) {
                        Avatar(p.initials, i, 52.dp)
                    }
                }
            }
        }
        item { SectionHeader(tr("Identité fictive", "Fictional identity")) }
        item { EssaTextField(first, { first = it }, tr("Prénom", "First name"), leading = Icons.Rounded.Person, maxLength = 32) }
        item { EssaTextField(last, { last = it }, tr("Nom", "Last name"), leading = Icons.Rounded.AccountCircle, maxLength = 32) }
        item { EssaTextField(email, { email = it }, tr("E-mail fictif (facultatif)", "Fictional e-mail (optional)"), leading = Icons.Rounded.Email, keyboard = KeyboardType.Email, maxLength = 80) }
        item { EssaTextField(city, { city = it }, tr("Ville (facultatif)", "City (optional)"), leading = Icons.Rounded.Place, maxLength = 48) }
        item { Text(tr("N'indiquez aucune donnée bancaire réelle. Le nom du titulaire de vos cartes fictives sera mis à jour.", "Do not enter any real banking data. The holder name on your fictional cards will be updated."), style = EssaType.small, color = c.textMuted) }
    }
}

// ================================================================ Préférences

@Composable
fun PreferencesScreen(vm: AppViewModel, nav: Nav) {
    val s = vm.state.collectAsStateWithLifecycle().value ?: return
    val c = Essa.colors
    val lang = LocalLang.current
    val prefs = s.profile.preferences
    val tier = s.profile.tier
    val n = prefs.notifications
    val needBlack = tr("Le thème Onyx est réservé à ESSA BLACK (gratuit).", "The Onyx theme is reserved to ESSA BLACK (free).")
    val needPlus = tr("La personnalisation des couleurs est incluse dans ESSA PLUS (gratuit).", "Color customization is included in ESSA PLUS (free).")
    fun update(t: (com.superessence.essabank.core.model.Preferences) -> com.superessence.essabank.core.model.Preferences) = vm.run { updatePreferences(it, t) }

    ScreenScaffold(tr("Préférences", "Preferences"), nav::back) {
        item { SectionHeader(tr("Langue", "Language")) }
        item {
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                Chip("Français", prefs.language == AppLanguage.FR, Icons.Rounded.Language) { update { it.copy(language = AppLanguage.FR) } }
                Chip("English", prefs.language == AppLanguage.EN, Icons.Rounded.Language) { update { it.copy(language = AppLanguage.EN) } }
            }
        }
        item { SectionHeader(tr("Devise d'affichage", "Display currency")) }
        item {
            Panel(padding = 8.dp) {
                DisplayCurrency.entries.filter { it != DisplayCurrency.EUR_V }.forEach { cur ->
                    ListRow(cur.code + " · " + cur.symbol, if (cur == DisplayCurrency.UV) tr("Euro virtuel de référence", "Reference virtual euro") else tr("1 € = ", "1 € = ") + cur.rate.toPlainString() + " " + cur.symbol + tr(" (taux fictif)", " (fictional rate)"),
                        leading = { IconBadge(Icons.Rounded.AttachMoney, if (cur == prefs.currency) c.accent else c.textMuted, 38.dp) },
                        trailing = { if (cur == prefs.currency) Icon(Icons.Rounded.CheckCircle, null, tint = c.accent) },
                        onClick = { update { it.copy(currency = cur) } }, chevron = false)
                }
                Text(tr("Conversion d'affichage uniquement : toutes les devises sont virtuelles, sans valeur réelle.", "Display conversion only: every currency is virtual, with no real value."),
                    style = EssaType.small, color = c.textMuted, modifier = Modifier.padding(8.dp))
            }
        }
        item { SectionHeader(tr("Thème", "Theme")) }
        item {
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                listOf(ThemeMode.SYSTEM to tr("Système", "System"), ThemeMode.LIGHT to tr("Clair", "Light"), ThemeMode.DARK to tr("Sombre", "Dark"), ThemeMode.ONYX to "Onyx").forEach { (m, label) ->
                    Chip(label, prefs.theme == m, if (m == ThemeMode.ONYX) Icons.Rounded.Stars else Icons.Rounded.Brightness4) {
                        if (m == ThemeMode.ONYX && !tier.advancedThemes) vm.toast(needBlack, true) else update { it.copy(theme = m) }
                    }
                }
            }
        }
        item { SectionHeader(tr("Couleur d'accent", "Accent color")) }
        item {
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(14.dp)) {
                Accent.entries.forEach { a ->
                    ColorDot(Brand.accent(a), prefs.accent == a) { if (a != Accent.ELECTRIC && !tier.customization) vm.toast(needPlus, true) else update { it.copy(accent = a) } }
                }
            }
        }
        item { SectionHeader(tr("Affichage", "Display")) }
        item {
            Panel(padding = 8.dp) {
                ToggleRow(tr("Masquer les montants", "Hide amounts"), tr("Les soldes sont remplacés par des points", "Balances are replaced with dots"), prefs.hideBalances, Icons.Rounded.Visibility) { v -> update { it.copy(hideBalances = v) } }
                ToggleRow(tr("Animations", "Animations"), tr("Décompte des soldes, graphiques animés", "Balance count-up, animated charts"), prefs.animations, Icons.Rounded.AutoAwesome) { v -> update { it.copy(animations = v) } }
            }
        }
        item { SectionHeader(tr("Notifications", "Notifications")) }
        item {
            Panel(padding = 8.dp) {
                ToggleRow(tr("Paiements et transactions", "Payments and transactions"), null, n.payments) { v -> update { it.copy(notifications = it.notifications.copy(payments = v)) } }
                ToggleRow(tr("Virements", "Transfers"), null, n.transfers) { v -> update { it.copy(notifications = it.notifications.copy(transfers = v)) } }
                ToggleRow(tr("Objectifs d'épargne", "Savings goals"), null, n.savings) { v -> update { it.copy(notifications = it.notifications.copy(savings = v)) } }
                ToggleRow(tr("Alertes de budget", "Budget alerts"), null, n.budget) { v -> update { it.copy(notifications = it.notifications.copy(budget = v)) } }
                ToggleRow(tr("Cartes", "Cards"), null, n.cards) { v -> update { it.copy(notifications = it.notifications.copy(cards = v)) } }
                ToggleRow(tr("Résumés mensuels", "Monthly summaries"), null, n.summaries) { v -> update { it.copy(notifications = it.notifications.copy(summaries = v)) } }
                ToggleRow(tr("Investissements fictifs", "Fictional investments"), null, n.investments) { v -> update { it.copy(notifications = it.notifications.copy(investments = v)) } }
                ToggleRow(tr("Sécurité", "Security"), null, n.security) { v -> update { it.copy(notifications = it.notifications.copy(security = v)) } }
            }
        }
    }
}

// ================================================================ Sécurité

@Composable
fun SecurityScreen(vm: AppViewModel, nav: Nav, onSecureChanged: (Boolean) -> Unit) {
    val c = Essa.colors
    val context = LocalContext.current
    val sec = vm.security
    var bio by remember { mutableStateOf(sec.biometricEnabled) }
    var secure by remember { mutableStateOf(sec.secureScreen) }
    var autoLock by remember { mutableIntStateOf(sec.autoLockSeconds) }
    var changePin by remember { mutableStateOf(false) }
    val bioAvailable = Biometric.isAvailable(context)
    val bioOn = tr("Biométrie activée.", "Biometrics enabled.")
    val bioPrompt = tr("Confirmez pour activer", "Confirm to enable")
    val cancel = tr("Annuler", "Cancel")
    ScreenScaffold(tr("Sécurité", "Security"), nav::back) {
        item {
            Panel {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    IconBadge(Icons.Rounded.Security, c.positive, 44.dp)
                    Spacer(Modifier.width(12.dp))
                    Text(tr("Vos données restent sur ce téléphone. Le code PIN est haché (PBKDF2) et n'est jamais stocké en clair. Aucune sauvegarde cloud automatique.",
                        "Your data stays on this phone. The PIN is hashed (PBKDF2) and never stored in clear. No automatic cloud backup."), style = EssaType.small, color = c.textSecondary)
                }
            }
        }
        item {
            Panel(padding = 8.dp) {
                ListRow(tr("Changer le code PIN", "Change PIN"), null, leading = { IconBadge(Icons.Rounded.VpnKey, c.accent, 38.dp) }, onClick = { changePin = true })
                ToggleRow(tr("Déverrouillage biométrique", "Biometric unlock"), if (bioAvailable) tr("Empreinte ou visage Android", "Android fingerprint or face") else tr("Non disponible sur cet appareil", "Not available on this device"),
                    bio && bioAvailable, Icons.Rounded.Fingerprint, enabled = bioAvailable) { v ->
                    if (!v) { sec.biometricEnabled = false; bio = false }
                    else Biometric.prompt(context, "ESSABANK", bioPrompt, cancel, onSuccess = { sec.biometricEnabled = true; bio = true; vm.toast(bioOn) })
                }
                ToggleRow(tr("Protection de l'écran", "Screen protection"), tr("Bloque les captures et masque l'aperçu dans les applications récentes", "Blocks screenshots and hides the preview in recent apps"), secure, Icons.Rounded.Security) { v ->
                    sec.secureScreen = v; secure = v; onSecureChanged(v)
                }
            }
        }
        item { SectionHeader(tr("Verrouillage automatique", "Auto-lock")) }
        item {
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                listOf(0 to tr("Immédiat", "Immediately"), 30 to "30 s", 60 to "1 min", 300 to "5 min").forEach { (v, l) ->
                    Chip(l, autoLock == v, Icons.Rounded.Timer) { sec.autoLockSeconds = v; autoLock = v }
                }
            }
        }
        item { Text(tr("L'application se verrouille quand elle passe en arrière-plan au-delà de ce délai.", "The app locks when it stays in the background longer than this delay."), style = EssaType.small, color = c.textMuted) }
        item { PrimaryButton(tr("Verrouiller maintenant", "Lock now"), { vm.lockNow() }, icon = Icons.Rounded.Lock) }
    }
    if (changePin) ChangePinSheet(vm) { changePin = false }
}

@Composable
private fun ChangePinSheet(vm: AppViewModel, onDismiss: () -> Unit) {
    val c = Essa.colors
    var current by remember { mutableStateOf("") }
    var new1 by remember { mutableStateOf("") }
    var new2 by remember { mutableStateOf("") }
    var error by remember { mutableStateOf<String?>(null) }
    val wrong = tr("Code actuel incorrect.", "Current PIN is wrong.")
    val locked = tr("Trop d'essais, patientez.", "Too many attempts, please wait.")
    val trivial = tr("Nouveau code trop simple (4 à 6 chiffres, évitez 1234…).", "New PIN too simple (4 to 6 digits, avoid 1234…).")
    val mismatch = tr("Les nouveaux codes ne correspondent pas.", "New PINs do not match.")
    val done = tr("Code PIN modifié.", "PIN changed.")
    val notice = tr("Le code PIN a été modifié.", "The PIN was changed.")
    EssaSheet(onDismiss) {
        Text(tr("Changer le code PIN", "Change PIN"), style = EssaType.h2, color = c.text)
        Spacer(Modifier.height(12.dp))
        EssaTextField(current, { if (it.all(Char::isDigit)) current = it }, tr("Code actuel", "Current PIN"), keyboard = KeyboardType.NumberPassword, password = true, maxLength = 6)
        Spacer(Modifier.height(8.dp))
        EssaTextField(new1, { if (it.all(Char::isDigit)) new1 = it }, tr("Nouveau code", "New PIN"), keyboard = KeyboardType.NumberPassword, password = true, maxLength = 6)
        Spacer(Modifier.height(8.dp))
        EssaTextField(new2, { if (it.all(Char::isDigit)) new2 = it }, tr("Confirmer le nouveau code", "Confirm new PIN"), keyboard = KeyboardType.NumberPassword, password = true, maxLength = 6)
        error?.let { Spacer(Modifier.height(8.dp)); Text(it, style = EssaType.bodyStrong, color = c.negative) }
        Spacer(Modifier.height(16.dp))
        PrimaryButton(tr("Modifier", "Change"), enabled = current.length >= 4 && new1.length >= 4 && new2.length >= 4, onClick = {
            when (vm.security.check(current)) {
                is PinCheck.Wrong -> error = wrong
                is PinCheck.Locked -> error = locked
                PinCheck.Ok -> when {
                    !PinHasher.isValidPin(new1) -> error = trivial
                    new1 != new2 -> error = mismatch
                    else -> { vm.security.setPin(new1); vm.toast(done); vm.run { securityNotice(it, notice) }; onDismiss() }
                }
            }
        })
    }
}

// ================================================================ Données

@Composable
fun DataScreen(vm: AppViewModel, nav: Nav) {
    val s = vm.state.collectAsStateWithLifecycle().value ?: return
    val c = Essa.colors
    val lang = LocalLang.current
    val context = LocalContext.current
    val scope = rememberCoroutineScope()
    var exportSheet by remember { mutableStateOf(false) }
    var importData by remember { mutableStateOf<ByteArray?>(null) }
    var confirm by remember { mutableStateOf<String?>(null) }
    val demoCount = s.transactions.count { it.isDemo }
    val saver = rememberFileSaver("application/octet-stream") { ok -> vm.toast(if (ok) tr(lang, "Sauvegarde chiffrée enregistrée.", "Encrypted backup saved.") else tr(lang, "Sauvegarde annulée.", "Backup cancelled."), !ok) }
    val opener = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
        if (uri != null) importData = readUri(context, uri)
    }
    val cleared = tr("Transactions de démonstration supprimées. Les soldes sont conservés.", "Demo transactions deleted. Balances are kept.")
    ScreenScaffold(tr("Gestion des données", "Data management"), nav::back) {
        item {
            Panel {
                KeyValue(tr("Transactions", "Transactions"), s.transactions.size.toString())
                KeyValue(tr("Dont démonstration", "Of which demo"), demoCount.toString())
                KeyValue(tr("Notifications", "Notifications"), s.notifications.size.toString())
                KeyValue(tr("Stockage", "Storage"), tr("Base SQLite locale", "Local SQLite database"))
            }
        }
        item { SectionHeader(tr("Sauvegarde et restauration", "Backup and restore")) }
        item {
            Panel(padding = 8.dp) {
                ListRow(tr("Exporter une sauvegarde chiffrée", "Export an encrypted backup"), tr("Fichier .essabk protégé par mot de passe (AES-256)", "Password-protected .essabk file (AES-256)"),
                    leading = { IconBadge(Icons.Rounded.Backup, c.accent, 38.dp) }, onClick = { exportSheet = true })
                ListRow(tr("Restaurer une sauvegarde", "Restore a backup"), tr("Remplace toutes les données actuelles", "Replaces all current data"),
                    leading = { IconBadge(Icons.Rounded.Restore, c.accent, 38.dp) }, onClick = { opener.launch(arrayOf("*/*")) })
            }
        }
        item { SectionHeader(tr("Démonstration", "Demo")) }
        item {
            Panel(padding = 8.dp) {
                ListRow(tr("Supprimer les données de démonstration", "Delete demo data"), tr("$demoCount transaction(s) fictive(s) préremplie(s)", "$demoCount prefilled fictional transaction(s)"),
                    leading = { IconBadge(Icons.Rounded.Delete, c.warning, 38.dp) }, onClick = { if (demoCount > 0) confirm = "demo" })
                ListRow(tr("Réinitialiser le profil de démonstration", "Reset the demo profile"), tr("Recharge les comptes et l'historique fictifs d'origine", "Reloads the original fictional accounts and history"),
                    leading = { IconBadge(Icons.Rounded.Refresh, c.warning, 38.dp) }, onClick = { confirm = "reset" })
                ListRow(tr("Tout effacer", "Erase everything"), tr("Supprime le profil, la base et le code PIN", "Deletes the profile, database and PIN"),
                    leading = { IconBadge(Icons.Rounded.DeleteForever, c.negative, 38.dp) }, onClick = { confirm = "wipe" }, titleColor = c.negative)
            }
        }
        item { Text(tr("Toutes les données sont virtuelles. Les sauvegardes ne contiennent aucune information bancaire réelle.", "All data is virtual. Backups contain no real banking information."), style = EssaType.small, color = c.textMuted) }
    }
    if (exportSheet) PasswordSheet(tr("Mot de passe de la sauvegarde", "Backup password"), tr("Au moins 6 caractères. Il sera demandé à la restauration et ne peut pas être récupéré.", "At least 6 characters. It will be required to restore and cannot be recovered."),
        tr("Exporter", "Export"), onDismiss = { exportSheet = false }) { pwd ->
        exportSheet = false
        scope.launch { val bytes = vm.exportBackup(pwd); saver("ESSABANK-sauvegarde-${java.time.LocalDate.now()}.essabk", bytes) }
    }
    importData?.let { data ->
        PasswordSheet(tr("Restaurer la sauvegarde", "Restore backup"), tr("Saisissez le mot de passe choisi lors de l'export. Les données actuelles seront remplacées.", "Enter the password chosen at export. Current data will be replaced."),
            tr("Restaurer", "Restore"), onDismiss = { importData = null }) { pwd ->
            scope.launch { if (vm.importBackup(data, pwd)) importData = null }
        }
    }
    when (confirm) {
        "demo" -> ConfirmDialog(tr("Supprimer les données de démonstration ?", "Delete demo data?"), tr("Les transactions préremplies seront effacées. Les soldes actuels sont conservés grâce à une écriture d'ouverture.", "Prefilled transactions will be erased. Current balances are kept with an opening entry."),
            tr("Supprimer", "Delete"), danger = true, onConfirm = { vm.run(cleared) { clearDemoTransactions(it) } }, onDismiss = { confirm = null })
        "reset" -> ConfirmDialog(tr("Réinitialiser la démonstration ?", "Reset the demo?"), tr("Toutes les données actuelles seront remplacées par le profil fictif d'origine. Le code PIN est conservé.", "All current data will be replaced by the original fictional profile. The PIN is kept."),
            tr("Réinitialiser", "Reset"), danger = true, onConfirm = { vm.resetDemo { nav.tab(Routes.HOME) } }, onDismiss = { confirm = null })
        "wipe" -> ConfirmDialog(tr("Tout effacer ?", "Erase everything?"), tr("Le profil, la base locale et le code PIN seront supprimés de ce téléphone. Action définitive.", "The profile, local database and PIN will be removed from this phone. This cannot be undone."),
            tr("Tout effacer", "Erase everything"), danger = true, onConfirm = { vm.deleteEverything { } }, onDismiss = { confirm = null })
    }
}

@Composable
private fun PasswordSheet(title: String, text: String, action: String, onDismiss: () -> Unit, onSubmit: (String) -> Unit) {
    val c = Essa.colors
    var pwd by remember { mutableStateOf("") }
    EssaSheet(onDismiss) {
        Text(title, style = EssaType.h2, color = c.text)
        Spacer(Modifier.height(4.dp))
        Text(text, style = EssaType.small, color = c.textMuted)
        Spacer(Modifier.height(12.dp))
        EssaTextField(pwd, { pwd = it }, tr("Mot de passe", "Password"), password = true, keyboard = KeyboardType.Password, maxLength = 64)
        Spacer(Modifier.height(16.dp))
        PrimaryButton(action, enabled = pwd.length >= 6, onClick = { onSubmit(pwd) })
    }
}

// ================================================================ Formules

@Composable
fun PlansScreen(vm: AppViewModel, nav: Nav) {
    val s = vm.state.collectAsStateWithLifecycle().value ?: return
    val c = Essa.colors
    val lang = LocalLang.current
    val current = s.profile.tier
    val plans = listOf(
        Triple(Tier.STANDARD, listOf(Brand.Electric, Brand.Cyan), listOf(
            tr("Comptes virtuels (jusqu'à 6)", "Virtual accounts (up to 6)"), tr("3 cartes simulées", "3 simulated cards"), tr("Historique complet", "Full history"),
            tr("Budgets et alertes simulées", "Budgets and simulated alerts"), tr("Épargne (3 objectifs)", "Savings (3 goals)"))),
        Triple(Tier.PLUS, listOf(Color(0xFF7B5CFF), Brand.Electric), listOf(
            tr("Tout ESSA STANDARD", "Everything in ESSA STANDARD"), tr("Jusqu'à 10 cartes, cartes premium", "Up to 10 cards, premium cards"), tr("Statistiques avancées", "Advanced insights"),
            tr("Personnalisation des couleurs", "Color customization"), tr("Objectifs d'épargne illimités", "Unlimited savings goals"))),
        Triple(Tier.BLACK, listOf(Color(0xFF0B0B0D), Color(0xFF3A3326)), listOf(
            tr("Tout ESSA PLUS", "Everything in ESSA PLUS"), tr("Interface exclusive Onyx", "Exclusive Onyx interface"), tr("Designs de cartes premium (Onyx, Or, Titane, Nébuleuse)", "Premium card designs (Onyx, Gold, Titanium, Nebula)"),
            tr("Thèmes avancés", "Advanced themes"), tr("Tableau de bord personnalisé", "Custom dashboard"))),
    )
    ScreenScaffold(tr("Formules ESSA", "ESSA plans"), nav::back) {
        item {
            Text(tr("Toutes les formules sont gratuites : aucun paiement n'est demandé, aucun moyen de paiement n'est enregistré. Changez de formule à tout moment.",
                "Every plan is free: no payment is requested and no payment method is stored. Switch plans at any time."), style = EssaType.body, color = c.textSecondary)
        }
        plans.forEach { (tier, colors, features) ->
            item(key = tier.name) {
                val active = tier == current
                Column(Modifier.fillMaxWidth().clip(RoundedCornerShape(24.dp)).background(Brush.linearGradient(colors)).padding(20.dp)) {
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        EssaMark(26.dp, tile = false)
                        Spacer(Modifier.width(8.dp))
                        Text(L.tierName(tier), style = EssaType.h2, color = if (tier == Tier.BLACK) Brand.Gold else Color.White, modifier = Modifier.weight(1f))
                        if (active) SimulationPill(text = tr("ACTIVE", "ACTIVE"))
                    }
                    Text(tr("0 € · gratuit", "0 € · free"), style = EssaType.bodyStrong, color = Color.White.copy(alpha = 0.75f))
                    Spacer(Modifier.height(12.dp))
                    features.forEach { f ->
                        Row(Modifier.padding(vertical = 3.dp), verticalAlignment = Alignment.CenterVertically) {
                            Icon(Icons.Rounded.CheckCircle, null, tint = if (tier == Tier.BLACK) Brand.Gold else Color.White, modifier = Modifier.size(18.dp))
                            Spacer(Modifier.width(8.dp))
                            Text(f, style = EssaType.body, color = Color.White)
                        }
                    }
                    Spacer(Modifier.height(14.dp))
                    val msg = tr("Formule ${L.tierName(tier)} activée gratuitement.", "${L.tierName(tier)} plan activated for free.")
                    Box(
                        Modifier.fillMaxWidth().height(48.dp).clip(RoundedCornerShape(14.dp)).background(Color.White.copy(alpha = if (active) 0.12f else 0.95f))
                            .pressable(enabled = !active) { vm.run(msg) { setTier(it, tier) } },
                        contentAlignment = Alignment.Center,
                    ) {
                        Text(if (active) tr("Formule actuelle", "Current plan") else tr("Activer gratuitement", "Activate for free"), style = EssaType.title,
                            color = if (active) Color.White else Color(0xFF0A1230))
                    }
                }
            }
        }
        item { Text(tr("En revenant à une formule inférieure, les options exclusives (designs premium, thème Onyx, tableau de bord) sont simplement désactivées : aucune donnée n'est perdue.",
            "When moving to a lower plan, exclusive options (premium designs, Onyx theme, dashboard) are simply turned off: no data is lost."), style = EssaType.small, color = c.textMuted) }
    }
}

// ================================================================ Tableau de bord personnalisé (ESSA BLACK)

@Composable
fun DashboardCustomScreen(vm: AppViewModel, nav: Nav) {
    val s = vm.state.collectAsStateWithLifecycle().value ?: return
    val c = Essa.colors
    val widgets = s.profile.preferences.homeWidgets
    fun label(w: HomeWidget, fr: Boolean) = when (w) {
        HomeWidget.BALANCE -> if (fr) "Solde principal" else "Main balance"
        HomeWidget.QUICK_ACTIONS -> if (fr) "Accès rapides" else "Quick actions"
        HomeWidget.INSIGHTS -> if (fr) "Conseil du mois" else "Monthly insight"
        HomeWidget.WEALTH -> if (fr) "Évolution du patrimoine" else "Net worth trend"
        HomeWidget.ACCOUNTS -> if (fr) "Comptes" else "Accounts"
        HomeWidget.CARDS -> if (fr) "Cartes" else "Cards"
        HomeWidget.BUDGET -> if (fr) "Budget mensuel" else "Monthly budget"
        HomeWidget.GOALS -> if (fr) "Coffres d'épargne" else "Savings vaults"
        HomeWidget.TRANSACTIONS -> if (fr) "Dernières transactions" else "Latest transactions"
    }
    val fr = LocalLang.current == AppLanguage.FR
    fun save(list: List<HomeWidget>) = vm.run { updatePreferences(it) { p -> p.copy(homeWidgets = list) } }
    ScreenScaffold(tr("Tableau de bord", "Dashboard"), nav::back) {
        item { Text(tr("Choisissez les blocs de l'accueil et leur ordre. Le solde principal reste toujours disponible.", "Choose the home blocks and their order. The main balance always stays available."), style = EssaType.body, color = c.textSecondary) }
        item { SectionHeader(tr("Blocs affichés", "Visible blocks")) }
        item {
            Panel(padding = 8.dp) {
                widgets.forEachIndexed { i, w ->
                    ListRow(label(w, fr), null, leading = { IconBadge(Icons.Rounded.Dashboard, c.accent, 36.dp) }, trailing = {
                        Row {
                            RoundIconButton(Icons.Rounded.KeyboardArrowUp, "↑", { if (i > 0) save(widgets.toMutableList().also { l -> l.add(i - 1, l.removeAt(i)) }) }, size = 34.dp)
                            Spacer(Modifier.width(6.dp))
                            RoundIconButton(Icons.Rounded.KeyboardArrowDown, "↓", { if (i < widgets.lastIndex) save(widgets.toMutableList().also { l -> l.add(i + 1, l.removeAt(i)) }) }, size = 34.dp)
                            if (w != HomeWidget.BALANCE) {
                                Spacer(Modifier.width(6.dp))
                                RoundIconButton(Icons.Rounded.Delete, "×", { save(widgets - w) }, tint = c.negative, size = 34.dp)
                            }
                        }
                    })
                }
            }
        }
        val hidden = HomeWidget.entries.filter { it !in widgets }
        if (hidden.isNotEmpty()) {
            item { SectionHeader(tr("Blocs masqués", "Hidden blocks")) }
            item {
                Panel(padding = 8.dp) {
                    hidden.forEach { w -> ListRow(label(w, fr), tr("Toucher pour afficher", "Tap to show"), leading = { IconBadge(Icons.Rounded.Dashboard, c.textMuted, 36.dp) }, onClick = { save(widgets + w) }) }
                }
            }
        }
        item { SecondaryButton(tr("Rétablir l'ordre par défaut", "Restore default order"), { save(HomeWidget.entries.toList()) }, Modifier.fillMaxWidth()) }
    }
}

// ================================================================ À propos

@Composable
fun AboutScreen(nav: Nav) {
    val c = Essa.colors
    ScreenScaffold(tr("À propos", "About"), nav::back) {
        item {
            Column(Modifier.fillMaxWidth().clip(RoundedCornerShape(26.dp)).background(Brush.linearGradient(listOf(Color(0xFF0B1650), Brand.Night))).padding(26.dp), horizontalAlignment = Alignment.CenterHorizontally) {
                EssaMark(76.dp)
                Spacer(Modifier.height(12.dp))
                EssaWordmark(Color.White, 24f)
                Text("Your Money. Your World.", style = EssaType.body, color = Color.White.copy(alpha = 0.7f))
                Spacer(Modifier.height(12.dp))
                Text("Version 1.0.0", style = EssaType.small, color = Color.White.copy(alpha = 0.6f))
                Text("A SUPERESSENCE STUDIO PRODUCTION", style = EssaType.caption, color = Brand.Cyan)
            }
        }
        item {
            Panel {
                Text(tr("Application de simulation", "Simulation app"), style = EssaType.title, color = c.text)
                Spacer(Modifier.height(6.dp))
                BulletLine(tr("ESSABANK est une banque entièrement fictive, destinée à la simulation et au divertissement.", "ESSABANK is an entirely fictional bank, made for simulation and entertainment."))
                BulletLine(tr("Elle n'est pas un établissement bancaire, ne détient aucun fonds et n'effectue aucun virement réel.", "It is not a bank, holds no funds and makes no real transfer."))
                BulletLine(tr("Les euros affichés sont virtuels : ils n'ont aucune valeur monétaire réelle.", "Displayed euros are virtual: they have no real monetary value."))
                BulletLine(tr("Les cartes, numéros, CVV et références sont inventés et inutilisables sur un réseau de paiement.", "Cards, numbers, CVVs and references are made up and unusable on any payment network."))
                BulletLine(tr("Le marché, les actifs et les crédits sont simulés localement : ce ne sont ni des conseils ni des offres.", "Market, assets and loans are simulated locally: they are neither advice nor offers."))
                BulletLine(tr("Aucune connexion Internet n'est utilisée : vos données restent sur votre téléphone.", "No Internet connection is used: your data stays on your phone."))
            }
        }
        item {
            Panel {
                Text(tr("Crédits", "Credits"), style = EssaType.title, color = c.text)
                Spacer(Modifier.height(6.dp))
                Text(tr("Conception et développement : Superessence Studio.\nPolices Inter et Manrope sous licence SIL Open Font License 1.1.\nIcônes Material Symbols (Apache 2.0).",
                    "Design and development: Superessence Studio.\nInter and Manrope fonts under the SIL Open Font License 1.1.\nMaterial Symbols icons (Apache 2.0)."), style = EssaType.body, color = c.textSecondary)
            }
        }
    }
}
