package com.superessence.essabank.ui.components

import androidx.compose.animation.core.Animatable
import androidx.compose.animation.core.FastOutSlowInEasing
import androidx.compose.animation.core.tween
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.gestures.detectDragGestures
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.size
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.PathEffect
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.StrokeJoin
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.graphics.drawscope.clipRect
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import com.superessence.essabank.ui.theme.Essa
import com.superessence.essabank.ui.theme.EssaType

/**
 * Courbe lissée avec aire en dégradé, animation de tracé et lecture au doigt.
 * [onSelect] reçoit l'index du point touché (ou null au relâchement).
 */
@Composable
fun LineChart(
    values: List<Float>,
    color: Color,
    modifier: Modifier = Modifier,
    height: Dp = 160.dp,
    selected: Int? = null,
    onSelect: ((Int?) -> Unit)? = null,
    showGrid: Boolean = true,
    animationKey: Any? = values.size,
) {
    val c = Essa.colors
    val progress = remember { Animatable(0f) }
    LaunchedEffect(animationKey) {
        progress.snapTo(0f)
        progress.animateTo(1f, tween(1100, easing = FastOutSlowInEasing))
    }
    if (values.size < 2) { Box(modifier.fillMaxWidth().height(height)); return }
    val min = values.min(); val max = values.max()
    val range = (max - min).takeIf { it > 0f } ?: 1f
    var gesture = Modifier.fillMaxWidth().height(height)
    if (onSelect != null) {
        gesture = gesture
            .pointerInput(values) {
                detectDragGestures(
                    onDragStart = { o -> onSelect(indexAt(o.x, size.width.toFloat(), values.size)) },
                    onDragEnd = { onSelect(null) }, onDragCancel = { onSelect(null) },
                ) { change, _ -> onSelect(indexAt(change.position.x, size.width.toFloat(), values.size)) }
            }
            .pointerInput(values) { detectTapGestures(onPress = { o -> onSelect(indexAt(o.x, size.width.toFloat(), values.size)); tryAwaitRelease(); onSelect(null) }) }
    }
    Canvas(modifier.then(gesture)) {
        val w = size.width; val h = size.height
        val top = 8.dp.toPx(); val bottom = h - 6.dp.toPx()
        fun x(i: Int) = w * i / (values.size - 1)
        fun y(v: Float) = bottom - (v - min) / range * (bottom - top)
        if (showGrid) for (k in 0..3) {
            val gy = top + (bottom - top) * k / 3f
            drawLine(c.chartGrid, Offset(0f, gy), Offset(w, gy), 1.dp.toPx(), pathEffect = PathEffect.dashPathEffect(floatArrayOf(6f, 8f)))
        }
        val line = Path()
        line.moveTo(x(0), y(values[0]))
        for (i in 1 until values.size) {
            val px = x(i - 1); val py = y(values[i - 1]); val cx = x(i); val cy = y(values[i])
            val mid = (px + cx) / 2
            line.cubicTo(mid, py, mid, cy, cx, cy)
        }
        val area = Path().apply { addPath(line); lineTo(w, h); lineTo(0f, h); close() }
        clipRect(right = w * progress.value) {
            drawPath(area, Brush.verticalGradient(listOf(color.copy(alpha = 0.28f), color.copy(alpha = 0f)), top, h))
            drawPath(line, color, style = Stroke(2.5.dp.toPx(), cap = StrokeCap.Round, join = StrokeJoin.Round))
        }
        val sel = selected
        if (sel != null && sel in values.indices) {
            val sx = x(sel); val sy = y(values[sel])
            drawLine(c.textMuted.copy(alpha = 0.5f), Offset(sx, top), Offset(sx, bottom), 1.dp.toPx())
            drawCircle(color.copy(alpha = 0.25f), 10.dp.toPx(), Offset(sx, sy))
            drawCircle(color, 5.dp.toPx(), Offset(sx, sy))
            drawCircle(Color.White, 2.dp.toPx(), Offset(sx, sy))
        } else if (progress.value >= 1f) {
            val lx = x(values.size - 1); val ly = y(values.last())
            drawCircle(color.copy(alpha = 0.25f), 8.dp.toPx(), Offset(lx, ly))
            drawCircle(color, 4.dp.toPx(), Offset(lx, ly))
        }
    }
}

private fun indexAt(x: Float, width: Float, count: Int): Int =
    ((x / width) * (count - 1) + 0.5f).toInt().coerceIn(0, count - 1)

/** Mini-courbe (sparkline) sans axes. */
@Composable
fun Sparkline(values: List<Float>, color: Color, modifier: Modifier = Modifier) {
    LineChart(values, color, modifier, height = 44.dp, showGrid = false)
}

/**
 * Histogramme animé. Une seconde série facultative ([secondary]) est dessinée à côté
 * (comparaison revenus / dépenses). [highlight] met une barre en avant.
 */
@Composable
fun BarChart(
    values: List<Float>,
    labels: List<String>,
    color: Color,
    modifier: Modifier = Modifier,
    secondary: List<Float>? = null,
    secondaryColor: Color = Essa.colors.positive,
    highlight: Int? = values.lastIndex,
    height: Dp = 150.dp,
    limit: Float? = null,
    onSelect: ((Int) -> Unit)? = null,
) {
    val c = Essa.colors
    val progress = remember { Animatable(0f) }
    LaunchedEffect(values, secondary) {
        progress.snapTo(0f)
        progress.animateTo(1f, tween(900, easing = FastOutSlowInEasing))
    }
    val max = (listOfNotNull(values.maxOrNull(), secondary?.maxOrNull(), limit).maxOrNull() ?: 0f).takeIf { it > 0f } ?: 1f
    Column(modifier.fillMaxWidth()) {
        Canvas(
            Modifier.fillMaxWidth().height(height).then(
                if (onSelect != null) Modifier.pointerInput(values) {
                    detectTapGestures { o -> onSelect(((o.x / size.width) * values.size).toInt().coerceIn(0, values.size - 1)) }
                } else Modifier,
            ),
        ) {
            val n = values.size.coerceAtLeast(1)
            val slot = size.width / n
            val groups = if (secondary != null) 2 else 1
            val barW = (slot * (if (groups == 2) 0.32f else 0.52f)).coerceAtMost(26.dp.toPx())
            val gap = 3.dp.toPx()
            val r = CornerRadius(barW / 2.4f)
            for (k in 0..3) {
                val gy = size.height * k / 3f
                drawLine(c.chartGrid, Offset(0f, gy), Offset(size.width, gy), 1.dp.toPx())
            }
            values.forEachIndexed { i, v ->
                val groupW = barW * groups + gap * (groups - 1)
                val x0 = slot * i + (slot - groupW) / 2
                val hh = (v / max) * size.height * progress.value
                val isHi = highlight == i
                drawRoundRect(
                    if (isHi) Brush.verticalGradient(listOf(color, color.copy(alpha = 0.7f))) else Brush.verticalGradient(listOf(color.copy(alpha = 0.45f), color.copy(alpha = 0.25f))),
                    Offset(x0, size.height - hh), Size(barW, hh.coerceAtLeast(2f)), r,
                )
                secondary?.getOrNull(i)?.let { sv ->
                    val sh = (sv / max) * size.height * progress.value
                    drawRoundRect(
                        Brush.verticalGradient(listOf(secondaryColor.copy(alpha = if (isHi) 1f else 0.6f), secondaryColor.copy(alpha = if (isHi) 0.7f else 0.3f))),
                        Offset(x0 + barW + gap, size.height - sh), Size(barW, sh.coerceAtLeast(2f)), r,
                    )
                }
            }
            if (limit != null && limit > 0f) {
                val ly = size.height - (limit / max) * size.height
                drawLine(c.warning, Offset(0f, ly), Offset(size.width, ly), 1.5.dp.toPx(), pathEffect = PathEffect.dashPathEffect(floatArrayOf(10f, 8f)))
            }
        }
        Spacer(Modifier.height(8.dp))
        Row(Modifier.fillMaxWidth()) {
            labels.forEachIndexed { i, l ->
                Text(l, style = EssaType.small, color = if (highlight == i) c.text else c.textMuted, textAlign = TextAlign.Center, maxLines = 1, modifier = Modifier.weight(1f))
            }
        }
    }
}

/** Anneau de répartition (catégories) avec contenu central. */
@Composable
fun DonutChart(
    segments: List<Pair<Float, Color>>,
    size: Dp,
    thickness: Dp = 18.dp,
    selected: Int? = null,
    center: @Composable () -> Unit = {},
) {
    val c = Essa.colors
    val progress = remember { Animatable(0f) }
    LaunchedEffect(segments.size, segments.sumOf { it.first.toDouble() }) {
        progress.snapTo(0f)
        progress.animateTo(1f, tween(1100, easing = FastOutSlowInEasing))
    }
    Box(Modifier.size(size), contentAlignment = Alignment.Center) {
        Canvas(Modifier.fillMaxSize()) {
            val sw = thickness.toPx()
            val total = segments.sumOf { it.first.toDouble() }.toFloat().takeIf { it > 0f } ?: 1f
            val inset = sw / 2 + 4.dp.toPx()
            val arc = Size(this.size.width - inset * 2, this.size.height - inset * 2)
            drawArc(c.surfaceAlt, 0f, 360f, false, Offset(inset, inset), arc, style = Stroke(sw))
            var start = -90f
            val gapDeg = if (segments.size > 1) 2.2f else 0f
            segments.forEachIndexed { i, (v, col) ->
                val sweep = 360f * (v / total) * progress.value
                val isSel = selected == i
                drawArc(
                    col.copy(alpha = if (selected == null || isSel) 1f else 0.35f), start + gapDeg / 2, (sweep - gapDeg).coerceAtLeast(0.5f), false,
                    Offset(inset, inset), arc, style = Stroke(if (isSel) sw * 1.25f else sw, cap = StrokeCap.Butt),
                )
                start += sweep
            }
        }
        center()
    }
}

/** Légende horizontale simple. */
@Composable
fun LegendDot(color: Color, label: String) {
    val c = Essa.colors
    Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(6.dp)) {
        Canvas(Modifier.size(9.dp)) { drawCircle(color) }
        Text(label, style = EssaType.small, color = c.textSecondary)
    }
}
