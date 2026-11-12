package com.superessence.essabank.ui.screens

import androidx.compose.animation.AnimatedContent
import androidx.compose.animation.core.Animatable
import androidx.compose.animation.core.FastOutSlowInEasing
import androidx.compose.animation.core.Spring
import androidx.compose.animation.core.spring
import androidx.compose.animation.core.tween
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.slideInHorizontally
import androidx.compose.animation.slideOutHorizontally
import androidx.compose.animation.togetherWith
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.imePadding
import androidx.compose.foundation.layout.navigationBarsPadding
import androidx.compose.foundation.layout.offset
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.AutoAwesome
import androidx.compose.material.icons.rounded.Badge
import androidx.compose.material.icons.rounded.Lock
import androidx.compose.material.icons.rounded.Person
import androidx.compose.material.icons.rounded.Shield
import androidx.compose.material.icons.rounded.VerifiedUser
import androidx.compose.material3.Icon
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableLongStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.alpha
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.superessence.essabank.core.money.AppLanguage
import com.superessence.essabank.core.money.Money
import com.superessence.essabank.core.security.PinHasher
import com.superessence.essabank.data.PinCheck
import com.superessence.essabank.security.Biometric
import com.superessence.essabank.ui.AppViewModel
import com.superessence.essabank.ui.components.Avatar
import com.superessence.essabank.ui.components.Chip
import com.superessence.essabank.ui.components.ConfirmDialog
import com.superessence.essabank.ui.components.EssaMark
import com.superessence.essabank.ui.components.EssaTextField
import com.superessence.essabank.ui.components.EssaWordmark
import com.superessence.essabank.ui.components.NumberPad
import com.superessence.essabank.ui.components.PadExtra
import com.superessence.essabank.ui.components.PrimaryButton
import com.superessence.essabank.ui.components.SecondaryButton
import com.superessence.essabank.ui.components.ToggleRow
import com.superessence.essabank.ui.i18n.tr
import com.superessence.essabank.ui.theme.Brand
import com.superessence.essabank.ui.theme.Essa
import com.superessence.essabank.ui.theme.EssaFonts
import com.superessence.essabank.ui.theme.EssaType
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch

// ================================================================ Intro « Superessence Studio présente »

@Composable
fun IntroScreen(onDone: () -> Unit) {
    val studio = "SUPERESSENCE"
    val letters = remember { studio.map { Animatable(0f) } }
    val subtitle = remember { Animatable(0f) }
    val studioOut = remember { Animatable(1f) }
    val logo = remember { Animatable(0f) }
    val glow = remember { Animatable(0f) }
    LaunchedEffect(Unit) {
        letters.forEachIndexed { i, a -> launch { delay(i * 55L); a.animateTo(1f, tween(380, easing = FastOutSlowInEasing)) } }
        delay(studio.length * 55L + 250)
        subtitle.animateTo(1f, tween(420))
        delay(650)
        studioOut.animateTo(0f, tween(380))
        launch { glow.animateTo(1f, tween(1100)) }
        logo.animateTo(1f, spring(dampingRatio = Spring.DampingRatioLowBouncy, stiffness = Spring.StiffnessLow))
        delay(900)
        onDone()
    }
    Box(
        Modifier.fillMaxSize()
            .background(Brush.radialGradient(listOf(Color(0xFF101C5A), Brand.Night), radius = 1400f))
            .clickable(interactionSource = remember { MutableInteractionSource() }, indication = null) { onDone() },
        contentAlignment = Alignment.Center,
    ) {
        Column(Modifier.alpha(studioOut.value), horizontalAlignment = Alignment.CenterHorizontally) {
            Row {
                studio.forEachIndexed { i, ch ->
                    val a = letters[i].value
                    Text(
                        ch.toString(), color = Color.White.copy(alpha = a),
                        style = TextStyle(fontFamily = EssaFonts.display, fontWeight = FontWeight.ExtraBold, fontSize = 26.sp, letterSpacing = 3.sp),
                        modifier = Modifier.graphicsLayer { translationY = (1f - a) * 24f },
                    )
                }
            }
            Spacer(Modifier.height(10.dp))
            Text("STUDIO  ·  " + tr("PRÉSENTE", "PRESENTS"), color = Brand.Cyan.copy(alpha = subtitle.value),
                style = TextStyle(fontFamily = EssaFonts.body, fontWeight = FontWeight.SemiBold, fontSize = 12.sp, letterSpacing = 5.sp))
        }
        Column(horizontalAlignment = Alignment.CenterHorizontally, modifier = Modifier.alpha(logo.value.coerceIn(0f, 1f))) {
            Box(contentAlignment = Alignment.Center) {
                Canvas(Modifier.size(220.dp)) {
                    drawCircle(Brush.radialGradient(listOf(Brand.Electric.copy(alpha = 0.45f * glow.value), Color.Transparent)), radius = size.minDimension / 2)
                }
                EssaMark(104.dp, Modifier.graphicsLayer { scaleX = 0.6f + 0.4f * logo.value; scaleY = 0.6f + 0.4f * logo.value })
            }
            EssaWordmark(Color.White, 30f)
            Spacer(Modifier.height(8.dp))
            Text("Your Money. Your World.", color = Color.White.copy(alpha = 0.7f), style = EssaType.body.copy(letterSpacing = 1.sp))
        }
        Text(
            tr("Application de simulation · aucune valeur réelle", "Simulation app · no real value"),
            color = Color.White.copy(alpha = 0.45f), style = EssaType.small,
            modifier = Modifier.align(Alignment.BottomCenter).navigationBarsPadding().padding(bottom = 28.dp),
        )
    }
}

// ================================================================ Bienvenue et création du profil

@Composable
fun OnboardingScreen(vm: AppViewModel, onCreated: () -> Unit) {
    var step by remember { mutableIntStateOf(0) }
    var language by remember { mutableStateOf(AppLanguage.FR) }
    var loading by remember { mutableStateOf(false) }
    val fr = language == AppLanguage.FR
    fun t(f: String, e: String) = if (fr) f else e

    Box(Modifier.fillMaxSize().background(Brush.verticalGradient(listOf(Color(0xFF0B1650), Brand.Night, Brand.Night)))) {
        AnimatedContent(step, transitionSpec = {
            (slideInHorizontally(tween(380)) { it / 2 } + fadeIn(tween(380))) togetherWith (slideOutHorizontally(tween(300)) { -it / 3 } + fadeOut(tween(250)))
        }, label = "onboarding") { s ->
            if (s == 0) {
                Column(
                    Modifier.fillMaxSize().statusBarsPadding().navigationBarsPadding().verticalScroll(rememberScrollState()).padding(24.dp),
                ) {
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        EssaMark(40.dp); Spacer(Modifier.width(10.dp)); EssaWordmark(Color.White, 18f)
                        Spacer(Modifier.weight(1f))
                        Chip("FR", language == AppLanguage.FR) { language = AppLanguage.FR }
                        Spacer(Modifier.width(6.dp))
                        Chip("EN", language == AppLanguage.EN) { language = AppLanguage.EN }
                    }
                    Spacer(Modifier.height(36.dp))
                    WelcomeCardsArt()
                    Spacer(Modifier.height(30.dp))
                    Text(t("Votre banque numérique,\nen version simulation.", "Your digital bank,\nsimulation edition."), style = EssaType.h1.copy(fontSize = 30.sp, lineHeight = 36.sp), color = Color.White)
                    Spacer(Modifier.height(12.dp))
                    Text(t("Comptes, cartes, virements, épargne et investissements : tout fonctionne, mais rien n'est réel.",
                        "Accounts, cards, transfers, savings and investing: everything works, but nothing is real."), style = EssaType.body.copy(fontSize = 15.sp, lineHeight = 22.sp), color = Color.White.copy(alpha = 0.72f))
                    Spacer(Modifier.height(22.dp))
                    Promise(Icons.Rounded.Shield, t("Banque 100 % fictive", "100% fictional bank"), t("ESSABANK n'est pas un établissement bancaire et ne détient aucun fonds.", "ESSABANK is not a bank and holds no funds."))
                    Promise(Icons.Rounded.AutoAwesome, t("Monnaie virtuelle", "Virtual currency"), t("Les unités virtuelles (UV) n'ont aucune valeur monétaire.", "Virtual units (UV) have no monetary value."))
                    Promise(Icons.Rounded.VerifiedUser, t("Aucune donnée bancaire demandée", "No banking data requested"), t("Tout reste sur votre téléphone, protégé par un code PIN local.", "Everything stays on your phone, protected by a local PIN."))
                    Spacer(Modifier.height(28.dp))
                    PrimaryButton(t("Découvrir avec le profil de démonstration", "Explore with the demo profile"), loading = loading, onClick = {
                        loading = true
                        vm.startDemo(language) { loading = false; onCreated() }
                    })
                    Spacer(Modifier.height(10.dp))
                    SecondaryButton(t("Créer mon profil fictif", "Create my fictional profile"), { step = 1 }, Modifier.fillMaxWidth(), icon = Icons.Rounded.Person)
                    Spacer(Modifier.height(18.dp))
                    Text("A SUPERESSENCE STUDIO PRODUCTION", style = EssaType.caption.copy(letterSpacing = 2.sp), color = Color.White.copy(alpha = 0.4f), modifier = Modifier.fillMaxWidth(), textAlign = TextAlign.Center)
                }
            } else {
                CreateProfileForm(language, onBack = { step = 0 }) { first, last, capital ->
                    vm.createProfile(first, last, capital, language) { onCreated() }
                }
            }
        }
    }
}

@Composable
private fun Promise(icon: androidx.compose.ui.graphics.vector.ImageVector, title: String, text: String) {
    Row(Modifier.fillMaxWidth().padding(vertical = 7.dp), verticalAlignment = Alignment.Top) {
        Box(Modifier.size(38.dp).clip(RoundedCornerShape(12.dp)).background(Color.White.copy(alpha = 0.08f)), contentAlignment = Alignment.Center) {
            Icon(icon, null, tint = Brand.Cyan, modifier = Modifier.size(20.dp))
        }
        Spacer(Modifier.width(12.dp))
        Column {
            Text(title, style = EssaType.title, color = Color.White)
            Text(text, style = EssaType.small, color = Color.White.copy(alpha = 0.6f))
        }
    }
}

/** Illustration d'accueil : deux cartes fictives superposées en perspective. */
@Composable
private fun WelcomeCardsArt() {
    val float = remember { Animatable(0f) }
    LaunchedEffect(Unit) {
        while (true) { float.animateTo(1f, tween(2600)); float.animateTo(0f, tween(2600)) }
    }
    Box(Modifier.fillMaxWidth().height(190.dp), contentAlignment = Alignment.Center) {
        FakeCard(listOf(Color(0xFF4B23D8), Color(0xFF00BFB3)), Modifier.offset(x = 40.dp, y = (-18).dp).graphicsLayer { rotationZ = 8f; rotationY = -14f; cameraDistance = 12f * density; translationY = float.value * 8f })
        FakeCard(listOf(Color(0xFF1D46F0), Color(0xFF34C6F4)), Modifier.offset(x = (-24).dp, y = 14.dp).graphicsLayer { rotationZ = -6f; rotationY = 10f; cameraDistance = 12f * density; translationY = -float.value * 10f })
    }
}

@Composable
private fun FakeCard(colors: List<Color>, modifier: Modifier) {
    Box(modifier.size(250.dp, 158.dp).clip(RoundedCornerShape(20.dp)).background(Brush.linearGradient(colors)).border(1.dp, Color.White.copy(alpha = 0.18f), RoundedCornerShape(20.dp))) {
        Canvas(Modifier.fillMaxSize()) {
            for (i in 1..4) drawCircle(Color.White.copy(alpha = 0.06f), size.height * 0.3f * i, Offset(size.width, 0f), style = androidx.compose.ui.graphics.drawscope.Stroke(2f))
        }
        Column(Modifier.padding(18.dp)) {
            EssaWordmark(Color.White, 13f)
            Spacer(Modifier.weight(1f))
            Text("•••• •••• •••• 0042", color = Color.White, style = EssaType.mono.copy(fontSize = 14.sp))
            Spacer(Modifier.height(6.dp))
            Text("SIMULATION", color = Color.White.copy(alpha = 0.7f), style = EssaType.caption)
        }
    }
}

@Composable
private fun CreateProfileForm(language: AppLanguage, onBack: () -> Unit, onCreate: (String, String, Money) -> Unit) {
    val fr = language == AppLanguage.FR
    fun t(f: String, e: String) = if (fr) f else e
    var first by remember { mutableStateOf("") }
    var last by remember { mutableStateOf("") }
    val capitals = listOf(1_000, 5_000, 10_000, 25_000)
    var capital by remember { mutableIntStateOf(1) }
    var busy by remember { mutableStateOf(false) }
    val c = Essa.colors
    Column(
        Modifier.fillMaxSize().statusBarsPadding().navigationBarsPadding().imePadding().verticalScroll(rememberScrollState()).padding(24.dp),
    ) {
        Text(t("← Retour", "← Back"), color = Color.White.copy(alpha = 0.7f), style = EssaType.bodyStrong, modifier = Modifier.clip(RoundedCornerShape(8.dp)).clickable(onClick = onBack).padding(6.dp))
        Spacer(Modifier.height(20.dp))
        Avatar((first.take(1) + last.take(1)).uppercase(), 0, 72.dp)
        Spacer(Modifier.height(18.dp))
        Text(t("Créez votre profil fictif", "Create your fictional profile"), style = EssaType.h1, color = Color.White)
        Spacer(Modifier.height(6.dp))
        Text(t("Un nom d'emprunt suffit. Aucune pièce d'identité, aucun identifiant bancaire ne vous sera demandé.",
            "A pseudonym is fine. No ID document or banking credentials will ever be requested."), style = EssaType.body, color = Color.White.copy(alpha = 0.65f))
        Spacer(Modifier.height(22.dp))
        EssaTextField(first, { first = it }, t("Prénom", "First name"), leading = Icons.Rounded.Person, maxLength = 32)
        Spacer(Modifier.height(10.dp))
        EssaTextField(last, { last = it }, t("Nom (facultatif)", "Last name (optional)"), leading = Icons.Rounded.Badge, maxLength = 32)
        Spacer(Modifier.height(22.dp))
        Text(t("Capital virtuel de départ", "Virtual starting capital"), style = EssaType.title, color = Color.White)
        Spacer(Modifier.height(10.dp))
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            capitals.forEachIndexed { i, v ->
                Chip("%,d UV".format(v).replace(',', if (fr) ' ' else ','), capital == i) { capital = i }
            }
        }
        Spacer(Modifier.height(8.dp))
        Text(t("Unités virtuelles sans valeur réelle, crédités sur votre compte courant fictif.", "Virtual units with no real value, credited to your fictional current account."), style = EssaType.small, color = c.textMuted)
        Spacer(Modifier.height(30.dp))
        PrimaryButton(t("Continuer", "Continue"), enabled = first.isNotBlank(), loading = busy, onClick = {
            busy = true
            onCreate(first, last, Money.units(capitals[capital]))
        })
    }
}

// ================================================================ Code PIN

@Composable
fun PinSetupScreen(vm: AppViewModel, onDone: () -> Unit) {
    val context = LocalContext.current
    var first by remember { mutableStateOf<String?>(null) }
    var pin by remember { mutableStateOf("") }
    var error by remember { mutableStateOf<String?>(null) }
    var askBio by remember { mutableStateOf(false) }
    val shake = remember { Animatable(0f) }
    val scope = rememberCoroutineScope()
    val trivial = tr("Code trop simple (évitez 1234, 0000…).", "Code too simple (avoid 1234, 0000…).")
    val mismatch = tr("Les deux codes ne correspondent pas.", "The two codes do not match.")
    val bioSubtitle = tr("Confirmez pour activer", "Confirm to enable")
    val cancelText = tr("Annuler", "Cancel")
    val c = Essa.colors

    fun submit() {
        if (first == null) {
            if (!PinHasher.isValidPin(pin)) { error = trivial; scope.launch { shakeIt(shake) }; pin = ""; return }
            first = pin; pin = ""; error = null
        } else {
            if (pin != first) { error = mismatch; first = null; pin = ""; scope.launch { shakeIt(shake) }; return }
            vm.setPin(pin)
            if (Biometric.isAvailable(context)) askBio = true else onDone()
        }
    }

    if (askBio) {
        Column(Modifier.fillMaxSize().background(c.background).statusBarsPadding().navigationBarsPadding().padding(24.dp), verticalArrangement = Arrangement.Center) {
            Icon(Icons.Rounded.Shield, null, tint = c.accent, modifier = Modifier.size(56.dp))
            Spacer(Modifier.height(16.dp))
            Text(tr("Déverrouillage biométrique", "Biometric unlock"), style = EssaType.h1, color = c.text)
            Spacer(Modifier.height(8.dp))
            Text(tr("Utilisez l'empreinte ou le visage enregistrés sur ce téléphone pour ouvrir ESSABANK. Vos données biométriques ne quittent jamais Android.",
                "Use the fingerprint or face registered on this phone to open ESSABANK. Your biometric data never leaves Android."), style = EssaType.body, color = c.textSecondary)
            Spacer(Modifier.height(28.dp))
            PrimaryButton(tr("Activer la biométrie", "Enable biometrics"), onClick = {
                Biometric.prompt(context, "ESSABANK", bioSubtitle, cancelText,
                    onSuccess = { vm.security.biometricEnabled = true; onDone() }, onFailure = { })
            })
            Spacer(Modifier.height(10.dp))
            SecondaryButton(tr("Plus tard", "Later"), onDone, Modifier.fillMaxWidth())
        }
        return
    }

    PinPadLayout(
        title = if (first == null) tr("Choisissez votre code PIN", "Choose your PIN") else tr("Confirmez votre code PIN", "Confirm your PIN"),
        subtitle = tr("4 à 6 chiffres. Il protège l'accès à l'application sur ce téléphone.", "4 to 6 digits. It protects access to the app on this phone."),
        pin = pin, error = error, shake = shake.value, maxLength = 6,
        onDigit = { if (pin.length < 6) { pin += it; error = null } },
        onBackspace = { pin = pin.dropLast(1) },
        footer = {
            PrimaryButton(tr("Valider", "Confirm"), enabled = pin.length >= 4, onClick = { submit() })
        },
    )
}

private suspend fun shakeIt(a: Animatable<Float, androidx.compose.animation.core.AnimationVector1D>) {
    for (v in listOf(18f, -16f, 12f, -8f, 4f, 0f)) a.animateTo(v, tween(55))
}

@Composable
private fun PinPadLayout(
    title: String, subtitle: String, pin: String, error: String?, shake: Float, maxLength: Int,
    onDigit: (Char) -> Unit, onBackspace: () -> Unit, extra: PadExtra = PadExtra.NONE, onExtra: () -> Unit = {},
    header: @Composable () -> Unit = { EssaMark(56.dp) }, footer: @Composable () -> Unit = {},
) {
    val c = Essa.colors
    Column(
        Modifier.fillMaxSize().background(c.background).statusBarsPadding().navigationBarsPadding().padding(horizontal = 28.dp, vertical = 16.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
    ) {
        Spacer(Modifier.weight(0.6f))
        header()
        Spacer(Modifier.height(18.dp))
        Text(title, style = EssaType.h2, color = c.text, textAlign = TextAlign.Center)
        Spacer(Modifier.height(6.dp))
        Text(subtitle, style = EssaType.small, color = c.textMuted, textAlign = TextAlign.Center)
        Spacer(Modifier.height(26.dp))
        Row(Modifier.graphicsLayer { translationX = shake }, horizontalArrangement = Arrangement.spacedBy(14.dp)) {
            repeat(maxLength.coerceAtLeast(4)) { i ->
                val filled = i < pin.length
                val optional = i >= 4
                Box(
                    Modifier.size(16.dp).clip(CircleShape)
                        .background(if (filled) c.accent else Color.Transparent)
                        .border(2.dp, if (filled) c.accent else c.border.copy(alpha = if (optional) 0.5f else 1f), CircleShape),
                )
            }
        }
        Spacer(Modifier.height(14.dp))
        Text(error ?: " ", style = EssaType.small, color = c.negative, textAlign = TextAlign.Center, modifier = Modifier.height(32.dp))
        Spacer(Modifier.weight(1f))
        NumberPad(onDigit, onBackspace, extra, onExtra)
        Spacer(Modifier.height(14.dp))
        footer()
    }
}

@Composable
fun LockScreen(vm: AppViewModel) {
    val context = LocalContext.current
    val state by vm.state.collectAsStateWithLifecycle()
    val profile = state?.profile
    var pin by remember { mutableStateOf("") }
    var error by remember { mutableStateOf<String?>(null) }
    var lockedFor by remember { mutableLongStateOf(vm.security.lockRemaining()) }
    var forgot by remember { mutableStateOf(false) }
    val shake = remember { Animatable(0f) }
    val scope = rememberCoroutineScope()
    val bio = vm.security.biometricEnabled && Biometric.isAvailable(context)
    val wrongText = tr("Code incorrect", "Wrong PIN")
    val attemptsText = tr("tentative(s) restante(s)", "attempt(s) left")

    val bioSubtitle = tr("Déverrouiller votre espace de simulation", "Unlock your simulation space")
    val usePin = tr("Utiliser le code", "Use PIN")
    fun biometric() = Biometric.prompt(context, "ESSABANK", bioSubtitle, usePin, onSuccess = { vm.unlock() })

    LaunchedEffect(Unit) { if (bio && lockedFor == 0L) biometric() }
    LaunchedEffect(lockedFor > 0) {
        while (lockedFor > 0) { delay(1000); lockedFor = vm.security.lockRemaining() }
    }
    fun verify(code: String) {
        when (val r = vm.checkPin(code)) {
            PinCheck.Ok -> Unit
            is PinCheck.Wrong -> { error = "$wrongText · ${r.attemptsLeft} $attemptsText"; pin = ""; scope.launch { shakeIt(shake) } }
            is PinCheck.Locked -> { lockedFor = r.remainingMillis; pin = ""; error = null; scope.launch { shakeIt(shake) } }
        }
    }

    PinPadLayout(
        title = tr("Bonjour", "Hello") + (profile?.firstName?.let { " $it" } ?: ""),
        subtitle = if (lockedFor > 0) tr("Trop d'essais. Réessayez dans ${lockedFor / 1000 + 1} s.", "Too many attempts. Try again in ${lockedFor / 1000 + 1} s.")
            else tr("Saisissez votre code PIN", "Enter your PIN"),
        pin = pin, error = error, shake = shake.value, maxLength = 6,
        onDigit = {
            if (lockedFor == 0L && pin.length < 6) {
                pin += it; error = null
                // Vérification automatique à partir de 4 chiffres si la longueur correspond au dernier code valide
                if (pin.length == 6) verify(pin)
            }
        },
        onBackspace = { pin = pin.dropLast(1) },
        extra = if (bio) PadExtra.BIOMETRIC else PadExtra.NONE, onExtra = { biometric() },
        header = {
            if (profile != null) Avatar(profile.initials, profile.avatar, 64.dp) else Icon(Icons.Rounded.Lock, null, tint = Essa.colors.accent, modifier = Modifier.size(48.dp))
        },
        footer = {
            PrimaryButton(tr("Déverrouiller", "Unlock"), enabled = pin.length >= 4 && lockedFor == 0L, onClick = { verify(pin) })
            Spacer(Modifier.height(8.dp))
            Text(tr("Code oublié ?", "Forgot your PIN?"), style = EssaType.bodyStrong, color = Essa.colors.textMuted,
                modifier = Modifier.clip(RoundedCornerShape(8.dp)).clickable { forgot = true }.padding(8.dp))
        },
    )
    if (forgot) {
        ConfirmDialog(
            title = tr("Réinitialiser l'application ?", "Reset the app?"),
            text = tr("Le code PIN n'est stocké que sous forme chiffrée sur ce téléphone et ne peut pas être récupéré. Vous pouvez effacer toutes les données locales et recommencer (une sauvegarde chiffrée pourra être restaurée ensuite).",
                "The PIN is only stored hashed on this phone and cannot be recovered. You can erase all local data and start again (an encrypted backup can be restored afterwards)."),
            confirm = tr("Tout effacer", "Erase everything"), danger = true,
            onConfirm = { vm.deleteEverything { } }, onDismiss = { forgot = false },
        )
    }
}
