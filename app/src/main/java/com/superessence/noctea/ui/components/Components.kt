package com.superessence.noctea.ui.components

import androidx.compose.animation.core.LinearEasing
import androidx.compose.animation.core.RepeatMode
import androidx.compose.animation.core.animateFloat
import androidx.compose.animation.core.infiniteRepeatable
import androidx.compose.animation.core.rememberInfiniteTransition
import androidx.compose.animation.core.tween
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxScope
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyRow
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Favorite
import androidx.compose.material.icons.filled.PlayArrow
import androidx.compose.material.icons.outlined.FavoriteBorder
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.drawscope.DrawScope
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.em
import androidx.compose.ui.unit.sp
import com.superessence.noctea.ui.theme.Cormorant
import com.superessence.noctea.ui.theme.LocalPalette
import kotlin.math.PI
import kotlin.math.cos
import kotlin.math.sin
import kotlin.random.Random

// ------------------------------------------------------------------ fond nocturne
@Composable
fun NightBackground(modifier: Modifier = Modifier, stars: Int = 70, content: @Composable BoxScope.() -> Unit) {
    val p = LocalPalette.current
    val starList = remember(stars) {
        val r = Random(7)
        List(stars) { Triple(Offset(r.nextFloat(), r.nextFloat() * 0.75f), r.nextFloat() * 1.6f + 0.4f, r.nextFloat() * 6.28f) }
    }
    val t by rememberInfiniteTransition(label = "sky").animateFloat(
        0f, (2 * PI).toFloat(), infiniteRepeatable(tween(9000, easing = LinearEasing)), label = "tw",
    )
    val drift by rememberInfiniteTransition(label = "glow").animateFloat(
        0f, 1f, infiniteRepeatable(tween(14000), RepeatMode.Reverse), label = "drift",
    )
    Box(
        modifier
            .fillMaxSize()
            .background(Brush.verticalGradient(listOf(p.sky0, p.sky1, p.sky2))),
    ) {
        Canvas(Modifier.fillMaxSize()) {
            val w = size.width
            val h = size.height
            drawCircle(
                Brush.radialGradient(
                    listOf(p.glow.copy(alpha = 0.28f), Color.Transparent),
                    center = Offset(w * (0.2f + 0.15f * drift), h * 0.12f), radius = w * 0.9f,
                ),
                radius = w * 0.9f, center = Offset(w * (0.2f + 0.15f * drift), h * 0.12f),
            )
            drawCircle(
                Brush.radialGradient(
                    listOf(p.accent.copy(alpha = 0.10f), Color.Transparent),
                    center = Offset(w * 0.9f, h * (0.55f - 0.1f * drift)), radius = w * 0.7f,
                ),
                radius = w * 0.7f, center = Offset(w * 0.9f, h * (0.55f - 0.1f * drift)),
            )
            starList.forEach { (o, s, ph) ->
                val a = 0.25f + 0.55f * ((sin(t + ph) + 1f) / 2f)
                drawCircle(p.text.copy(alpha = a), radius = s.dp.toPx() * 0.7f, center = Offset(o.x * w, o.y * h))
            }
        }
        content()
    }
}

// ------------------------------------------------------------------ logo
fun DrawScope.drawCrescent(center: Offset, radius: Float, colors: List<Color>, cut: Float = 0.85f, starColor: Color? = Color(0xFFFFF8E7)) {
    val outer = Path().apply { addOval(androidx.compose.ui.geometry.Rect(center, radius)) }
    val cutCenter = Offset(center.x + radius * 0.46f, center.y - radius * 0.39f)
    val inner = Path().apply { addOval(androidx.compose.ui.geometry.Rect(cutCenter, radius * cut)) }
    val crescent = Path().apply { op(outer, inner, androidx.compose.ui.graphics.PathOperation.Difference) }
    drawPath(crescent, Brush.linearGradient(colors, start = center - Offset(radius, radius), end = center + Offset(radius, radius)))
    if (starColor != null) drawSparkle(Offset(center.x + radius * 0.83f, center.y - radius * 0.63f), radius * 0.31f, starColor)
}

fun DrawScope.drawSparkle(c: Offset, r: Float, color: Color) {
    val path = Path()
    for (k in 0 until 8) {
        val ang = -PI / 2 + k * PI / 4
        val rr = if (k % 2 == 0) r else r * 0.26f
        val x = c.x + rr * cos(ang).toFloat()
        val y = c.y + rr * sin(ang).toFloat()
        if (k == 0) path.moveTo(x, y) else path.lineTo(x, y)
    }
    path.close()
    drawPath(path, color)
}

@Composable
fun MoonMark(size: Dp, modifier: Modifier = Modifier, glow: Boolean = true) {
    val p = LocalPalette.current
    Canvas(modifier.size(size)) {
        val r = this.size.minDimension * 0.34f
        val c = Offset(this.size.width * 0.46f, this.size.height * 0.54f)
        if (glow) drawCircle(Brush.radialGradient(listOf(p.moon.copy(alpha = 0.35f), Color.Transparent), c, r * 2.1f), r * 2.1f, c)
        drawCrescent(c, r, listOf(Color(0xFFFFF4D6), p.moon, p.accent))
    }
}

@Composable
fun NocteaWordmark(modifier: Modifier = Modifier, fontSize: Int = 34, color: Color = LocalPalette.current.text) {
    Text(
        "NOCTÉA",
        modifier = modifier,
        fontFamily = Cormorant,
        fontWeight = FontWeight.Medium,
        fontSize = fontSize.sp,
        letterSpacing = 0.32.em,
        color = color,
    )
}

// ------------------------------------------------------------------ cartes
@Composable
fun GlassCard(modifier: Modifier = Modifier, onClick: (() -> Unit)? = null, padding: Dp = 18.dp, content: @Composable ColumnScope.() -> Unit) {
    val p = LocalPalette.current
    val shape = RoundedCornerShape(24.dp)
    Column(
        modifier
            .clip(shape)
            .background(Brush.verticalGradient(listOf(Color.White.copy(alpha = 0.09f), Color.White.copy(alpha = 0.04f))))
            .border(1.dp, p.cardBorder, shape)
            .then(if (onClick != null) Modifier.clickable(onClick = onClick) else Modifier)
            .padding(padding),
        content = content,
    )
}

@Composable
fun SectionTitle(title: String, modifier: Modifier = Modifier, action: String? = null, onAction: (() -> Unit)? = null) {
    val p = LocalPalette.current
    Row(modifier.fillMaxWidth().padding(horizontal = 20.dp, vertical = 6.dp), verticalAlignment = Alignment.CenterVertically) {
        Text(title, style = MaterialTheme.typography.headlineSmall, modifier = Modifier.weight(1f))
        if (action != null && onAction != null) {
            Text(action, style = MaterialTheme.typography.labelMedium, color = p.accent, modifier = Modifier.clickable(onClick = onAction).padding(6.dp))
        }
    }
}

data class CardData(
    val id: String,
    val title: String,
    val subtitle: String,
    val art: String,
    val durationMs: Long,
    val favorite: Boolean,
    val premium: Boolean = false,
)

@Composable
fun TrackCardLarge(d: CardData, onClick: () -> Unit, onFav: (() -> Unit)?, modifier: Modifier = Modifier, width: Dp = 230.dp) {
    val p = LocalPalette.current
    Column(modifier.width(width).clip(RoundedCornerShape(26.dp)).clickable(onClick = onClick)) {
        Box(Modifier.fillMaxWidth().aspectRatio(1.05f).clip(RoundedCornerShape(26.dp))) {
            ArtIllustration(d.art, Modifier.fillMaxSize())
            Box(
                Modifier.fillMaxSize().background(Brush.verticalGradient(listOf(Color.Transparent, Color.Black.copy(alpha = 0.55f)))),
            )
            DurationPill(d.durationMs, Modifier.align(Alignment.BottomStart).padding(12.dp))
            if (onFav != null) {
                IconButton(onClick = onFav, modifier = Modifier.align(Alignment.TopEnd)) {
                    Icon(
                        if (d.favorite) Icons.Filled.Favorite else Icons.Outlined.FavoriteBorder,
                        contentDescription = if (d.favorite) "Retirer des favoris" else "Ajouter aux favoris",
                        tint = if (d.favorite) p.moon else Color.White.copy(alpha = 0.85f),
                    )
                }
            }
            Box(
                Modifier.align(Alignment.BottomEnd).padding(12.dp).size(38.dp).clip(CircleShape).background(p.moon),
                contentAlignment = Alignment.Center,
            ) { Icon(Icons.Filled.PlayArrow, contentDescription = "Écouter", tint = Color(0xFF1B1530)) }
        }
        Spacer(Modifier.height(10.dp))
        Text(d.title, style = MaterialTheme.typography.titleMedium, maxLines = 2, overflow = TextOverflow.Ellipsis, modifier = Modifier.padding(horizontal = 4.dp))
        Text(d.subtitle, style = MaterialTheme.typography.bodySmall, maxLines = 1, overflow = TextOverflow.Ellipsis, modifier = Modifier.padding(horizontal = 4.dp))
    }
}

@Composable
fun TrackRow(d: CardData, onClick: () -> Unit, onFav: (() -> Unit)?, modifier: Modifier = Modifier, trailing: @Composable (() -> Unit)? = null) {
    val p = LocalPalette.current
    Row(
        modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(20.dp))
            .clickable(onClick = onClick)
            .padding(horizontal = 16.dp, vertical = 8.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Box(Modifier.size(64.dp).clip(RoundedCornerShape(16.dp))) { ArtIllustration(d.art, Modifier.fillMaxSize()) }
        Spacer(Modifier.width(14.dp))
        Column(Modifier.weight(1f)) {
            Text(d.title, style = MaterialTheme.typography.titleMedium, maxLines = 1, overflow = TextOverflow.Ellipsis)
            Text("${d.subtitle} · ${formatDuration(d.durationMs)}", style = MaterialTheme.typography.bodySmall, maxLines = 1, overflow = TextOverflow.Ellipsis)
        }
        if (trailing != null) trailing()
        if (onFav != null) {
            IconButton(onClick = onFav) {
                Icon(
                    if (d.favorite) Icons.Filled.Favorite else Icons.Outlined.FavoriteBorder,
                    contentDescription = "Favori",
                    tint = if (d.favorite) p.moon else p.textDim,
                )
            }
        }
    }
}

@Composable
fun DurationPill(ms: Long, modifier: Modifier = Modifier) {
    Text(
        formatDuration(ms),
        modifier = modifier.clip(RoundedCornerShape(50)).background(Color.Black.copy(alpha = 0.35f)).padding(horizontal = 10.dp, vertical = 4.dp),
        style = MaterialTheme.typography.labelMedium,
        color = Color.White,
    )
}

@Composable
fun <T> ChipRow(options: List<T>, selected: T, label: (T) -> String, onSelect: (T) -> Unit, modifier: Modifier = Modifier) {
    LazyRow(modifier, contentPadding = androidx.compose.foundation.layout.PaddingValues(horizontal = 20.dp), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        items(options) { o -> NightChip(label(o), o == selected) { onSelect(o) } }
    }
}

@Composable
fun NightChip(text: String, selected: Boolean, modifier: Modifier = Modifier, onClick: () -> Unit) {
    val p = LocalPalette.current
    val shape = RoundedCornerShape(50)
    Text(
        text,
        modifier = modifier
            .clip(shape)
            .background(if (selected) p.accent else Color.White.copy(alpha = 0.06f))
            .border(1.dp, if (selected) p.accent else p.cardBorder, shape)
            .clickable(onClick = onClick)
            .padding(horizontal = 16.dp, vertical = 9.dp),
        style = MaterialTheme.typography.labelLarge,
        color = if (selected) Color(0xFF15102C) else p.text,
    )
}

@Composable
fun PrimaryButton(text: String, modifier: Modifier = Modifier, enabled: Boolean = true, icon: @Composable (() -> Unit)? = null, onClick: () -> Unit) {
    val p = LocalPalette.current
    Row(
        modifier
            .clip(RoundedCornerShape(50))
            .background(
                if (enabled) Brush.horizontalGradient(listOf(p.moon, Color(0xFFF7C9A0), p.accent))
                else Brush.horizontalGradient(listOf(Color.Gray.copy(alpha = 0.3f), Color.Gray.copy(alpha = 0.3f))),
            )
            .clickable(enabled = enabled, onClick = onClick)
            .padding(horizontal = 26.dp, vertical = 16.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.Center,
    ) {
        if (icon != null) { icon(); Spacer(Modifier.width(8.dp)) }
        Text(text, style = MaterialTheme.typography.labelLarge, color = Color(0xFF1A1430), textAlign = TextAlign.Center)
    }
}

@Composable
fun SecondaryButton(text: String, modifier: Modifier = Modifier, onClick: () -> Unit) {
    val p = LocalPalette.current
    Text(
        text,
        modifier = modifier
            .clip(RoundedCornerShape(50))
            .border(1.dp, p.accent.copy(alpha = 0.6f), RoundedCornerShape(50))
            .clickable(onClick = onClick)
            .padding(horizontal = 22.dp, vertical = 13.dp),
        style = MaterialTheme.typography.labelLarge,
        color = p.accent,
        textAlign = TextAlign.Center,
    )
}

/** Animation de chargement : une lune qui respire, entourée d'étoiles en orbite. */
@Composable
fun MoonLoader(modifier: Modifier = Modifier, size: Dp = 96.dp) {
    val p = LocalPalette.current
    val tr = rememberInfiniteTransition(label = "loader")
    val breath by tr.animateFloat(0.9f, 1.08f, infiniteRepeatable(tween(1800), RepeatMode.Reverse), label = "b")
    val rot by tr.animateFloat(0f, 360f, infiniteRepeatable(tween(5200, easing = LinearEasing)), label = "r")
    Box(modifier.size(size), contentAlignment = Alignment.Center) {
        Canvas(Modifier.fillMaxSize().graphicsLayer { rotationZ = rot }) {
            val c = center
            for (i in 0 until 6) {
                val a = i * PI / 3
                val rr = this.size.minDimension * 0.46f
                drawCircle(p.accent.copy(alpha = 0.25f + i * 0.12f), radius = 2.2.dp.toPx() + i * 0.4f, center = Offset(c.x + rr * cos(a).toFloat(), c.y + rr * sin(a).toFloat()))
            }
        }
        MoonMark(size * 0.62f, Modifier.graphicsLayer { scaleX = breath; scaleY = breath })
    }
}

fun formatDuration(ms: Long): String {
    val totalMin = ((ms + 30_000) / 60_000).toInt()
    return if (totalMin >= 60) "${totalMin / 60} h ${"%02d".format(totalMin % 60)}" else "$totalMin min"
}

fun formatClock(ms: Long): String {
    val s = (ms / 1000).coerceAtLeast(0)
    return if (s >= 3600) "%d:%02d:%02d".format(s / 3600, (s % 3600) / 60, s % 60) else "%d:%02d".format(s / 60, s % 60)
}

@Suppress("unused")
private fun unusedSize() = Size.Zero
