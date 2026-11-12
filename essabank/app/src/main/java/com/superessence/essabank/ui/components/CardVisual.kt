package com.superessence.essabank.ui.components

import androidx.compose.animation.core.Animatable
import androidx.compose.animation.core.Spring
import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.core.spring
import androidx.compose.animation.core.tween
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.gestures.detectDragGestures
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
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.Block
import androidx.compose.material.icons.rounded.Lock
import androidx.compose.material3.Icon
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.shadow
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.superessence.essabank.core.model.Card
import com.superessence.essabank.core.model.CardDesign
import com.superessence.essabank.core.model.CardStatus
import com.superessence.essabank.ui.i18n.L
import com.superessence.essabank.ui.i18n.LocalLang
import com.superessence.essabank.ui.i18n.tr
import com.superessence.essabank.ui.theme.EssaFonts
import kotlinx.coroutines.launch

object CardStyles {
    fun colors(d: CardDesign): List<Color> = when (d) {
        CardDesign.MIDNIGHT -> listOf(Color(0xFF0A1236), Color(0xFF162A7A), Color(0xFF0A1236))
        CardDesign.ELECTRIC -> listOf(Color(0xFF1D46F0), Color(0xFF2F6BFF), Color(0xFF34C6F4))
        CardDesign.GLACIER -> listOf(Color(0xFF5E7FD8), Color(0xFF8EAAF2), Color(0xFFB8CBFF))
        CardDesign.AURORA -> listOf(Color(0xFF4B23D8), Color(0xFF6B3BFF), Color(0xFF00BFB3))
        CardDesign.GRAPHITE -> listOf(Color(0xFF22252C), Color(0xFF3A3F4A), Color(0xFF555B68))
        CardDesign.OCEAN -> listOf(Color(0xFF00305F), Color(0xFF005A9C), Color(0xFF0096C7))
        CardDesign.ONYX -> listOf(Color(0xFF000000), Color(0xFF151518), Color(0xFF2A2A2E))
        CardDesign.GOLD_LEAF -> listOf(Color(0xFF6E5121), Color(0xFFB8913F), Color(0xFFE8CB84))
        CardDesign.TITANIUM -> listOf(Color(0xFF3F4652), Color(0xFF7C8594), Color(0xFFB5BCC8))
        CardDesign.NEBULA -> listOf(Color(0xFF14082F), Color(0xFF5B21B6), Color(0xFFDB2777))
    }
    fun accent(d: CardDesign): Color = when (d) {
        CardDesign.ONYX, CardDesign.GOLD_LEAF -> Color(0xFFE6C67A)
        CardDesign.TITANIUM, CardDesign.GLACIER -> Color(0xFFFFFFFF)
        else -> Color(0xFF8FD8FF)
    }
}

/**
 * Carte bancaire FICTIVE. Inclinaison 3D au doigt ([interactive]) et retournement animé ([flipped])
 * pour afficher le verso (CVV fictif). Les numéros ne sont affichés en entier que si [showDetails].
 */
@Composable
fun BankCard(
    card: Card,
    modifier: Modifier = Modifier,
    showDetails: Boolean = false,
    flipped: Boolean = false,
    interactive: Boolean = false,
    elevation: Dp = 18.dp,
) {
    val rotation by animateFloatAsState(if (flipped) 180f else 0f, tween(650), label = "flip")
    val tiltX = remember { Animatable(0f) }
    val tiltY = remember { Animatable(0f) }
    val scope = rememberCoroutineScope()
    val gestures = if (interactive) Modifier.pointerInput(Unit) {
        detectDragGestures(
            onDragEnd = { scope.launch { tiltX.animateTo(0f, spring(Spring.DampingRatioMediumBouncy)) }; scope.launch { tiltY.animateTo(0f, spring(Spring.DampingRatioMediumBouncy)) } },
            onDragCancel = { scope.launch { tiltX.snapTo(0f) }; scope.launch { tiltY.snapTo(0f) } },
        ) { change, drag ->
            change.consume()
            scope.launch { tiltY.snapTo((tiltY.value + drag.x / 12f).coerceIn(-16f, 16f)) }
            scope.launch { tiltX.snapTo((tiltX.value - drag.y / 12f).coerceIn(-12f, 12f)) }
        }
    } else Modifier
    val shape = RoundedCornerShape(22.dp)
    Box(
        modifier
            .fillMaxWidth()
            .aspectRatio(1.586f)
            .then(gestures)
            .graphicsLayer {
                rotationY = rotation + tiltY.value
                rotationX = tiltX.value
                cameraDistance = 14f * density
            }
            .shadow(elevation, shape, ambientColor = Color(0x660A1230), spotColor = Color(0x880A1230))
            .clip(shape),
    ) {
        if (rotation <= 90f) CardFront(card, showDetails, tiltY.value) else Box(Modifier.fillMaxSize().graphicsLayer { rotationY = 180f }) { CardBack(card, showDetails) }
        if (card.status != CardStatus.ACTIVE) {
            Box(Modifier.fillMaxSize().background(Color(0x99050818)), contentAlignment = Alignment.Center) {
                Row(
                    Modifier.clip(RoundedCornerShape(14.dp)).background(Color(0xCC0A1230)).padding(horizontal = 14.dp, vertical = 8.dp),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Icon(if (card.status == CardStatus.LOCKED) Icons.Rounded.Lock else Icons.Rounded.Block, null, tint = Color.White, modifier = Modifier.size(18.dp))
                    Spacer(Modifier.width(8.dp))
                    Text(L.cardStatus(card.status, LocalLang.current), color = Color.White, style = TextStyle(fontFamily = EssaFonts.body, fontWeight = FontWeight.SemiBold, fontSize = 14.sp))
                }
            }
        }
    }
}

@Composable
private fun CardFront(card: Card, showDetails: Boolean, tilt: Float) {
    val colors = CardStyles.colors(card.design)
    val accent = CardStyles.accent(card.design)
    val lang = LocalLang.current
    Box(Modifier.fillMaxSize()) {
        Canvas(Modifier.fillMaxSize()) {
            drawRect(Brush.linearGradient(colors, Offset.Zero, Offset(size.width, size.height)))
            // Ondes décoratives
            for (i in 1..5) drawCircle(Color.White.copy(alpha = 0.035f * (6 - i)), size.height * 0.28f * i, Offset(size.width * 1.02f, size.height * 0.05f), style = Stroke(1.2.dp.toPx()))
            drawCircle(Color.White.copy(alpha = 0.05f), size.height * 0.75f, Offset(-size.width * 0.05f, size.height * 1.15f))
            // Reflet qui suit l'inclinaison
            val shine = size.width * (0.5f + tilt / 40f)
            drawRect(Brush.linearGradient(listOf(Color.Transparent, Color.White.copy(alpha = 0.10f), Color.Transparent), Offset(shine - size.width * 0.35f, 0f), Offset(shine + size.width * 0.15f, size.height)))
        }
        Column(Modifier.fillMaxSize().padding(20.dp)) {
            Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
                EssaWordmark(Color.White, 15f)
                Spacer(Modifier.weight(1f))
                Text(L.cardType(card.type, lang).uppercase(), color = Color.White.copy(alpha = 0.75f), style = cardLabel(9.5f))
            }
            Spacer(Modifier.weight(1f))
            Row(verticalAlignment = Alignment.CenterVertically) {
                CardChip(accent)
                Spacer(Modifier.width(12.dp))
                Contactless(Color.White.copy(alpha = 0.8f))
                Spacer(Modifier.weight(1f))
                Text(card.label, color = Color.White.copy(alpha = 0.85f), style = cardLabel(11f), maxLines = 1)
            }
            Spacer(Modifier.height(14.dp))
            Text(
                if (showDetails) card.formattedNumber else card.maskedNumber,
                color = Color.White, style = TextStyle(fontFamily = EssaFonts.body, fontWeight = FontWeight.SemiBold, fontSize = 19.sp, letterSpacing = 2.sp, fontFeatureSettings = "tnum"),
                maxLines = 1,
            )
            Spacer(Modifier.height(12.dp))
            Row(verticalAlignment = Alignment.Bottom) {
                Column(Modifier.weight(1f)) {
                    Text(tr("TITULAIRE", "CARDHOLDER"), color = Color.White.copy(alpha = 0.6f), style = cardLabel(8f))
                    Text(card.holderName, color = Color.White, style = cardLabel(12f).copy(fontWeight = FontWeight.SemiBold), maxLines = 1)
                }
                Column {
                    Text(tr("EXPIRE", "EXPIRES"), color = Color.White.copy(alpha = 0.6f), style = cardLabel(8f))
                    Text(card.expiry, color = Color.White, style = cardLabel(12f).copy(fontWeight = FontWeight.SemiBold))
                }
                Spacer(Modifier.width(14.dp))
                Column(horizontalAlignment = Alignment.End) {
                    EssaMark(28.dp, tile = false)
                    Text(tr("FICTIVE", "FICTIONAL"), color = Color.White.copy(alpha = 0.7f), style = cardLabel(7.5f))
                }
            }
        }
    }
}

@Composable
private fun CardBack(card: Card, showDetails: Boolean) {
    val colors = CardStyles.colors(card.design)
    Box(Modifier.fillMaxSize().background(Brush.linearGradient(colors.reversed()))) {
        Column(Modifier.fillMaxSize()) {
            Spacer(Modifier.height(22.dp))
            Box(Modifier.fillMaxWidth().height(40.dp).background(Color(0xE6050608)))
            Spacer(Modifier.height(18.dp))
            Row(Modifier.padding(horizontal = 20.dp), verticalAlignment = Alignment.CenterVertically) {
                Box(Modifier.weight(1f).height(36.dp).clip(RoundedCornerShape(6.dp)).background(Brush.horizontalGradient(listOf(Color(0xFFEDEFF5), Color(0xFFD5DAE6)))))
                Spacer(Modifier.width(10.dp))
                Column(horizontalAlignment = Alignment.End) {
                    Text("CVV", color = Color.White.copy(alpha = 0.7f), style = cardLabel(8f))
                    Box(Modifier.clip(RoundedCornerShape(6.dp)).background(Color.White).padding(horizontal = 10.dp, vertical = 5.dp)) {
                        Text(if (showDetails) card.cvv else "•••", color = Color(0xFF0A1230), style = TextStyle(fontFamily = EssaFonts.body, fontWeight = FontWeight.Bold, fontSize = 15.sp, letterSpacing = 2.sp))
                    }
                }
            }
            Spacer(Modifier.weight(1f))
            Text(
                tr("Carte fictive ESSABANK. Numéro, date et CVV inventés : inutilisable sur tout réseau de paiement réel.",
                    "Fictional ESSABANK card. Number, date and CVV are made up: unusable on any real payment network."),
                color = Color.White.copy(alpha = 0.75f), style = cardLabel(9f).copy(letterSpacing = 0.2.sp, lineHeight = 12.sp),
                modifier = Modifier.padding(horizontal = 20.dp),
            )
            Spacer(Modifier.height(10.dp))
            Row(Modifier.padding(horizontal = 20.dp).padding(bottom = 16.dp), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.SpaceBetween) {
                EssaWordmark(Color.White.copy(alpha = 0.85f), 12f)
                Spacer(Modifier.weight(1f))
                SimulationPillOnCard()
            }
        }
    }
}

@Composable
private fun SimulationPillOnCard() {
    Box(Modifier.clip(RoundedCornerShape(6.dp)).background(Color.White.copy(alpha = 0.16f)).padding(horizontal = 8.dp, vertical = 3.dp)) {
        Text("SIMULATION", color = Color.White, style = cardLabel(8.5f).copy(fontWeight = FontWeight.Bold))
    }
}

private fun cardLabel(size: Float) = TextStyle(fontFamily = EssaFonts.body, fontWeight = FontWeight.Medium, fontSize = size.sp, letterSpacing = 1.2.sp)

/** Puce dessinée (aucun visuel propriétaire). */
@Composable
private fun CardChip(accent: Color) {
    Canvas(Modifier.size(width = 40.dp, height = 30.dp)) {
        val r = CornerRadius(6.dp.toPx())
        drawRoundRect(Brush.linearGradient(listOf(accent.copy(alpha = 0.95f), accent.copy(alpha = 0.55f), accent.copy(alpha = 0.9f))), cornerRadius = r)
        val line = Color.Black.copy(alpha = 0.22f)
        val w = size.width; val h = size.height
        drawLine(line, Offset(w * 0.33f, 0f), Offset(w * 0.33f, h), 1.dp.toPx())
        drawLine(line, Offset(w * 0.67f, 0f), Offset(w * 0.67f, h), 1.dp.toPx())
        drawLine(line, Offset(0f, h * 0.5f), Offset(w * 0.33f, h * 0.5f), 1.dp.toPx())
        drawLine(line, Offset(w * 0.67f, h * 0.5f), Offset(w, h * 0.5f), 1.dp.toPx())
        drawRoundRect(line, Offset(w * 0.33f, h * 0.25f), Size(w * 0.34f, h * 0.5f), CornerRadius(3.dp.toPx()), style = Stroke(1.dp.toPx()))
    }
}

/** Symbole « sans contact » générique (arcs concentriques). */
@Composable
private fun Contactless(color: Color) {
    Canvas(Modifier.size(22.dp)) {
        val sw = 2.dp.toPx()
        for (i in 0..2) {
            val rad = size.minDimension * (0.22f + 0.17f * i)
            drawArc(color, -50f, 100f, false, Offset(size.width * 0.15f - rad, size.height / 2 - rad), Size(rad * 2, rad * 2), style = Stroke(sw, cap = StrokeCap.Round))
        }
    }
}

/** Version miniature (listes). */
@Composable
fun MiniCard(card: Card, width: Dp = 52.dp) {
    val colors = CardStyles.colors(card.design)
    Box(
        Modifier.size(width, width / 1.586f).clip(RoundedCornerShape(7.dp)).background(Brush.linearGradient(colors)),
        contentAlignment = Alignment.BottomEnd,
    ) {
        Text(card.last4, color = Color.White, style = cardLabel(8f).copy(fontWeight = FontWeight.Bold), modifier = Modifier.padding(4.dp))
    }
}
