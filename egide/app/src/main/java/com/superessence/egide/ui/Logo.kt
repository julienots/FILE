package com.superessence.egide.ui

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Rect
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.PathMeasure
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.StrokeJoin
import androidx.compose.ui.graphics.drawscope.DrawScope
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.graphics.drawscope.withTransform

/**
 * Logo SuperEssence Studio : une goutte (l'essence) tracée d'un seul trait,
 * dont le cœur dessine un S. Dessiné en vectoriel, dans un repère 100 × 120.
 */
object LogoPaths {
    val drop: Path = Path().apply {
        moveTo(50f, 4f)
        cubicTo(50f, 4f, 12f, 44f, 12f, 78f)
        arcTo(Rect(12f, 40f, 88f, 116f), 180f, -180f, false)
        cubicTo(88f, 44f, 50f, 4f, 50f, 4f)
        close()
    }
    val s: Path = Path().apply {
        arcTo(Rect(38f, 54f, 62f, 78f), -20f, -250f, true)
        arcTo(Rect(38f, 78f, 62f, 102f), -90f, 250f, false)
    }
}

val LogoBrush = Brush.linearGradient(
    0f to Color(0xFFC4B5FD),
    0.45f to Color(0xFF8B5CF6),
    1f to Color(0xFF22D3EE),
    start = Offset(10f, 0f),
    end = Offset(90f, 120f),
)

/**
 * @param dropProgress 0..1 : avancement du tracé de la goutte
 * @param sProgress 0..1 : avancement du tracé du S
 * @param glow 0..1 : intensité du halo lumineux
 */
@Composable
fun EssenceLogo(
    modifier: Modifier = Modifier,
    dropProgress: Float = 1f,
    sProgress: Float = 1f,
    glow: Float = 0f,
    stroke: Float = 7f,
) {
    val dropMeasure = remember { PathMeasure().apply { setPath(LogoPaths.drop, false) } }
    val sMeasure = remember { PathMeasure().apply { setPath(LogoPaths.s, false) } }
    val dropSeg = remember { Path() }
    val sSeg = remember { Path() }
    Canvas(modifier.aspectRatio(100f / 120f)) {
        val scale = size.width / 100f
        withTransform({ scale(scale, scale, pivot = Offset.Zero) }) {
            dropSeg.reset(); sSeg.reset()
            dropMeasure.getSegment(0f, dropMeasure.length * dropProgress.coerceIn(0f, 1f), dropSeg, true)
            sMeasure.getSegment(0f, sMeasure.length * sProgress.coerceIn(0f, 1f), sSeg, true)
            if (glow > 0f) {
                for ((w, a) in listOf(stroke * 4.5f to 0.06f, stroke * 2.6f to 0.12f)) {
                    drawLogoPath(dropSeg, w, a * glow)
                    drawLogoPath(sSeg, w, a * glow)
                }
            }
            drawLogoPath(dropSeg, stroke, 1f)
            drawLogoPath(sSeg, stroke, 1f)
        }
    }
}

private fun DrawScope.drawLogoPath(p: Path, width: Float, alpha: Float) {
    drawPath(p, LogoBrush, alpha = alpha, style = Stroke(width, cap = StrokeCap.Round, join = StrokeJoin.Round))
}
