package com.superessence.essabank.ui.components

import androidx.compose.animation.AnimatedContent
import androidx.compose.animation.core.Animatable
import androidx.compose.animation.core.FastOutSlowInEasing
import androidx.compose.animation.core.LinearEasing
import androidx.compose.animation.core.RepeatMode
import androidx.compose.animation.core.animateFloat
import androidx.compose.animation.core.infiniteRepeatable
import androidx.compose.animation.core.rememberInfiniteTransition
import androidx.compose.animation.core.tween
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.togetherWith
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.rounded.Backspace
import androidx.compose.material.icons.rounded.Fingerprint
import androidx.compose.material3.Icon
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableLongStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.runtime.staticCompositionLocalOf
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.ImageBitmap
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.graphics.vector.rememberVectorPainter
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.text.SpanStyle
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.buildAnnotatedString
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.withStyle
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.superessence.essabank.core.model.GoalKind
import com.superessence.essabank.core.model.Preferences
import com.superessence.essabank.core.money.Money
import com.superessence.essabank.core.money.MoneyFormat
import com.superessence.essabank.ui.i18n.L
import com.superessence.essabank.ui.theme.Brand
import com.superessence.essabank.ui.theme.Essa
import com.superessence.essabank.ui.theme.EssaFonts
import com.superessence.essabank.ui.theme.EssaType

/** Préférences d'affichage courantes (langue, devise, masquage des montants). */
val LocalPrefs = staticCompositionLocalOf { Preferences() }

/** Formate un montant selon les préférences ; masqué si l'utilisateur a choisi de cacher les soldes. */
@Composable
fun fmt(money: Money, signed: Boolean = false, hideable: Boolean = true, decimals: Boolean = true): String {
    val p = LocalPrefs.current
    return MoneyFormat.format(money, p.currency, p.language, signed = signed, masked = hideable && p.hideBalances, decimals = decimals)
}

/**
 * Solde animé : décompte fluide vers la nouvelle valeur à chaque mise à jour,
 * partie décimale en plus petit, masquage en fondu.
 */
@Composable
fun AnimatedBalance(
    money: Money,
    style: TextStyle = EssaType.hero,
    color: Color = Color.White,
    hidden: Boolean = LocalPrefs.current.hideBalances,
    animate: Boolean = LocalPrefs.current.animations,
    signed: Boolean = false,
) {
    val p = LocalPrefs.current
    val progress = remember { Animatable(1f) }
    var from by remember { mutableLongStateOf(0L) }
    var to by remember { mutableLongStateOf(0L) }
    LaunchedEffect(money.cents) {
        val currentShown = from + ((to - from) * progress.value).toLong()
        from = if (to == 0L && from == 0L && animate) 0L else currentShown
        to = money.cents
        if (animate) {
            progress.snapTo(0f)
            progress.animateTo(1f, tween(1100, easing = FastOutSlowInEasing))
        } else progress.snapTo(1f)
    }
    val shown = if (progress.value >= 1f) money.cents else from + ((to - from) * progress.value).toLong()
    AnimatedContent(targetState = hidden, transitionSpec = { fadeIn(tween(250)) togetherWith fadeOut(tween(250)) }, label = "mask") { isHidden ->
        if (isHidden) {
            Text(MoneyFormat.format(money, p.currency, p.language, masked = true), style = style, color = color)
        } else {
            val (intPart, dec) = MoneyFormat.split(Money(shown), p.currency, p.language)
            val sign = if (signed && shown > 0) "+" else ""
            val symbol = p.currency.symbol
            Text(
                buildAnnotatedString {
                    if (p.currency.symbolBefore) withStyle(SpanStyle(fontSize = style.fontSize * 0.62f)) { append(symbol) }
                    append(sign + intPart)
                    withStyle(SpanStyle(fontSize = style.fontSize * 0.6f, fontWeight = FontWeight.Bold)) { append(dec) }
                    if (!p.currency.symbolBefore) withStyle(SpanStyle(fontSize = style.fontSize * 0.5f, fontWeight = FontWeight.SemiBold)) { append(" " + symbol) }
                },
                style = style, color = color, maxLines = 1,
            )
        }
    }
}

/** Montant d'une transaction : vert pour un crédit. */
@Composable
fun TxAmount(signed: Money, style: TextStyle = EssaType.amount) {
    val c = Essa.colors
    Text(fmt(signed, signed = true), style = style, color = if (signed.isPositive) c.positive else c.text, maxLines = 1)
}

// ---------------------------------------------------------------- marque

/** Monogramme ESSABANK : fût et barres décalées, dégradé électrique. */
@Composable
fun EssaMark(size: Dp, modifier: Modifier = Modifier, tile: Boolean = true) {
    Canvas(modifier.size(size)) {
        val s = this.size.minDimension
        if (tile) {
            drawRoundRect(
                Brush.linearGradient(listOf(Color(0xFF0E1A55), Color(0xFF050818)), Offset.Zero, Offset(s, s)),
                cornerRadius = CornerRadius(s * 0.28f),
            )
            drawRoundRect(Color(0x33FFFFFF), cornerRadius = CornerRadius(s * 0.28f), style = Stroke(width = s * 0.02f))
        }
        val u = s / 108f
        val r = CornerRadius(4.5f * u)
        fun bar(x: Float, y: Float, w: Float, h: Float, brush: Brush) = drawRoundRect(brush, Offset(x * u, y * u), Size(w * u, h * u), r)
        val white = Brush.linearGradient(listOf(Color.White, Color.White))
        bar(36f, 33f, 9f, 42f, white)
        bar(36f, 33f, 36f, 9f, white)
        bar(36f, 66f, 36f, 9f, white)
        bar(50f, 49.5f, 22f, 9f, Brush.horizontalGradient(listOf(Brand.Electric, Brand.Cyan), 50f * u, 72f * u))
        bar(76f, 49.5f, 9f, 9f, Brush.linearGradient(listOf(Brand.Cyan, Brand.Cyan)))
    }
}

@Composable
fun EssaWordmark(color: Color = Color.White, size: Float = 20f, modifier: Modifier = Modifier) {
    Text(
        buildAnnotatedString {
            withStyle(SpanStyle(fontWeight = FontWeight.ExtraBold)) { append("ESSA") }
            withStyle(SpanStyle(fontWeight = FontWeight.Medium)) { append("BANK") }
        },
        style = TextStyle(fontFamily = EssaFonts.display, fontSize = size.sp, letterSpacing = (size * 0.12f).sp),
        color = color, modifier = modifier,
    )
}

// ---------------------------------------------------------------- skeleton loading

@Composable
fun shimmerBrush(): Brush {
    val c = Essa.colors
    val t = rememberInfiniteTransition(label = "shimmer")
    val x by t.animateFloat(0f, 1400f, infiniteRepeatable(tween(1300, easing = LinearEasing), RepeatMode.Restart), label = "x")
    val base = c.surfaceAlt
    val hi = if (c.isDark) c.elevated.copy(alpha = 0.9f) else Color.White
    return Brush.linearGradient(listOf(base, hi, base), Offset(x - 400f, 0f), Offset(x, 200f))
}

@Composable
fun SkeletonBlock(modifier: Modifier, radius: Dp = 14.dp) {
    Box(modifier.clip(RoundedCornerShape(radius)).background(shimmerBrush()))
}

/** Écran de chargement squelette (affiché pendant l'ouverture de la base locale). */
@Composable
fun SkeletonHome() {
    val c = Essa.colors
    Column(Modifier.fillMaxSize().background(c.background).padding(20.dp), verticalArrangement = Arrangement.spacedBy(14.dp)) {
        Spacer(Modifier.height(40.dp))
        Row(verticalAlignment = Alignment.CenterVertically) {
            SkeletonBlock(Modifier.size(44.dp), 22.dp); Spacer(Modifier.width(12.dp))
            Column { SkeletonBlock(Modifier.width(120.dp).height(12.dp)); Spacer(Modifier.height(8.dp)); SkeletonBlock(Modifier.width(80.dp).height(10.dp)) }
        }
        SkeletonBlock(Modifier.fillMaxWidth().height(190.dp), 26.dp)
        Row(horizontalArrangement = Arrangement.spacedBy(12.dp)) { repeat(4) { SkeletonBlock(Modifier.weight(1f).height(72.dp), 18.dp) } }
        SkeletonBlock(Modifier.fillMaxWidth().height(120.dp), 20.dp)
        repeat(4) { SkeletonBlock(Modifier.fillMaxWidth().height(56.dp), 16.dp) }
    }
}

// ---------------------------------------------------------------- clavier numérique

/**
 * Clavier numérique intégré (montants et code PIN).
 * [extra] : touche en bas à gauche (virgule pour un montant, empreinte pour le PIN, ou rien).
 */
@Composable
fun NumberPad(onDigit: (Char) -> Unit, onBackspace: () -> Unit, extra: PadExtra = PadExtra.NONE, onExtra: () -> Unit = {}, keyHeight: Dp = 60.dp) {
    val c = Essa.colors
    Column(Modifier.fillMaxWidth(), verticalArrangement = Arrangement.spacedBy(6.dp)) {
        listOf("123", "456", "789").forEach { row ->
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                row.forEach { d -> PadKey(Modifier.weight(1f).height(keyHeight), { onDigit(d) }) { Text(d.toString(), style = EssaType.h2, color = c.text) } }
            }
        }
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            when (extra) {
                PadExtra.DECIMAL -> PadKey(Modifier.weight(1f).height(keyHeight), { onDigit(',') }) { Text(",", style = EssaType.h2, color = c.text) }
                PadExtra.BIOMETRIC -> PadKey(Modifier.weight(1f).height(keyHeight), onExtra) { Icon(Icons.Rounded.Fingerprint, "Biométrie", tint = c.accent, modifier = Modifier.size(30.dp)) }
                PadExtra.NONE -> Spacer(Modifier.weight(1f))
            }
            PadKey(Modifier.weight(1f).height(keyHeight), { onDigit('0') }) { Text("0", style = EssaType.h2, color = c.text) }
            PadKey(Modifier.weight(1f).height(keyHeight), onBackspace) { Icon(Icons.AutoMirrored.Rounded.Backspace, "Effacer", tint = c.textSecondary) }
        }
    }
}

enum class PadExtra { NONE, DECIMAL, BIOMETRIC }

@Composable
private fun PadKey(modifier: Modifier, onClick: () -> Unit, content: @Composable () -> Unit) {
    Box(modifier.clip(RoundedCornerShape(16.dp)).pressable(onClick = onClick), contentAlignment = Alignment.Center) { content() }
}

/** Applique une touche du clavier à une saisie de montant (2 décimales max). */
fun applyAmountKey(current: String, key: Char?): String {
    if (key == null) return current.dropLast(1)
    if (key == ',') return if (current.contains(',')) current else (if (current.isEmpty()) "0," else "$current,")
    if (current == "0") return key.toString()
    val dec = current.substringAfter(',', "")
    if (current.contains(',') && dec.length >= 2) return current
    if (!current.contains(',') && current.length >= 8) return current
    return current + key
}

// ---------------------------------------------------------------- avatars et illustrations

private val avatarGradients = listOf(
    listOf(Color(0xFF2F6BFF), Color(0xFF3DD6FF)), listOf(Color(0xFF7B5CFF), Color(0xFFE64980)),
    listOf(Color(0xFF12B886), Color(0xFF00B8D9)), listOf(Color(0xFFFF8A3D), Color(0xFFF5A524)),
    listOf(Color(0xFF0B1440), Color(0xFF2F6BFF)), listOf(Color(0xFF5C6B8A), Color(0xFF9AA6C4)),
    listOf(Color(0xFFE64980), Color(0xFFFF8A3D)), listOf(Color(0xFF16204D), Color(0xFF7B5CFF)),
)
val AVATAR_COUNT = avatarGradients.size

@Composable
fun Avatar(initials: String, index: Int, size: Dp, photo: ImageBitmap? = null, modifier: Modifier = Modifier) {
    if (photo != null) {
        Image(photo, null, contentScale = ContentScale.Crop, modifier = modifier.size(size).clip(CircleShape).border(2.dp, Color.White.copy(alpha = 0.2f), CircleShape))
        return
    }
    val g = avatarGradients[((index % AVATAR_COUNT) + AVATAR_COUNT) % AVATAR_COUNT]
    Box(
        modifier.size(size).clip(CircleShape).background(Brush.linearGradient(g)).border(2.dp, Color.White.copy(alpha = 0.18f), CircleShape),
        contentAlignment = Alignment.Center,
    ) {
        // Motif géométrique discret
        Canvas(Modifier.fillMaxSize()) {
            drawCircle(Color.White.copy(alpha = 0.10f), radius = this.size.minDimension * 0.42f, center = Offset(this.size.width * 0.85f, this.size.height * 0.1f))
            drawCircle(Color.White.copy(alpha = 0.07f), radius = this.size.minDimension * 0.3f, center = Offset(this.size.width * 0.1f, this.size.height * 0.95f))
        }
        Text(initials.ifEmpty { "?" }, color = Color.White, style = TextStyle(fontFamily = EssaFonts.display, fontWeight = FontWeight.ExtraBold, fontSize = (size.value * 0.36f).sp))
    }
}

/** Illustration vectorielle d'un objectif d'épargne (dégradé, ondes et pictogramme). */
@Composable
fun GoalIllustration(kind: GoalKind, color: Color, modifier: Modifier = Modifier, iconSize: Dp = 34.dp) {
    val icon: ImageVector = L.goalIcon(kind)
    val painter = rememberVectorPainter(icon)
    Box(modifier.clip(RoundedCornerShape(18.dp)), contentAlignment = Alignment.Center) {
        Canvas(Modifier.fillMaxSize()) {
            drawRect(Brush.linearGradient(listOf(color, color.copy(alpha = 0.55f), Color(0xFF0A1230)), Offset.Zero, Offset(size.width, size.height * 1.4f)))
            val cx = size.width * 0.78f; val cy = size.height * 0.2f
            for (i in 1..4) drawCircle(Color.White.copy(alpha = 0.06f + 0.02f * (4 - i)), radius = size.minDimension * 0.22f * i, center = Offset(cx, cy), style = Stroke(width = 1.5.dp.toPx()))
            drawCircle(Color.White.copy(alpha = 0.10f), radius = size.minDimension * 0.5f, center = Offset(size.width * 0.05f, size.height * 1.05f))
        }
        Icon(painter, null, tint = Color.White, modifier = Modifier.size(iconSize))
    }
}

/** Anneau de progression animé. */
@Composable
fun ProgressRing(progress: Float, color: Color, size: Dp, stroke: Dp = 10.dp, content: @Composable () -> Unit = {}) {
    val c = Essa.colors
    val anim = remember { Animatable(0f) }
    LaunchedEffect(progress) { anim.animateTo(progress.coerceIn(0f, 1f), tween(1000, easing = FastOutSlowInEasing)) }
    Box(Modifier.size(size), contentAlignment = Alignment.Center) {
        Canvas(Modifier.fillMaxSize()) {
            val sw = stroke.toPx()
            val inset = sw / 2
            val arcSize = Size(this.size.width - sw, this.size.height - sw)
            drawArc(c.surfaceAlt, 0f, 360f, false, Offset(inset, inset), arcSize, style = Stroke(sw))
            drawArc(Brush.sweepGradient(listOf(color.copy(alpha = 0.6f), color, color.copy(alpha = 0.6f))), -90f, 360f * anim.value, false,
                Offset(inset, inset), arcSize, style = Stroke(sw, cap = androidx.compose.ui.graphics.StrokeCap.Round))
        }
        content()
    }
}

/** Cercle de couleur sélectionnable (palette). */
@Composable
fun ColorDot(color: Color, selected: Boolean, onClick: () -> Unit) {
    Box(
        Modifier.size(40.dp).clip(CircleShape).background(color)
            .border(if (selected) 3.dp else 0.dp, if (selected) Essa.colors.text else Color.Transparent, CircleShape)
            .pressable(onClick = onClick),
    )
}

/** Carte « illustration » d'un compte. */
@Composable
fun AccountGlyph(iconKey: String, colorIndex: Int, size: Dp = 44.dp) {
    IconBadge(L.icon(iconKey), Brand.color(colorIndex), size)
}

@Composable
fun RatioBox(ratio: Float, modifier: Modifier = Modifier, content: @Composable () -> Unit) {
    Box(modifier.fillMaxWidth().aspectRatio(ratio)) { content() }
}
