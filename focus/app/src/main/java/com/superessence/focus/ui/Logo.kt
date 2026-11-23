package com.superessence.focus.ui

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.size
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.TextUnit
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlin.math.cos
import kotlin.math.sin

/**
 * Le logo Focus. : un anneau ouvert (la lentille qui se resserre) autour du point jaune SuperEssence,
 * avec une petite étincelle qui s'échappe par l'ouverture.
 * [ring] 0→1 dessine l'anneau, [dot] 0→1 fait apparaître le point, [spark] 0→1 l'étincelle.
 */
@Composable
fun FocusMark(size: Dp, modifier: Modifier = Modifier, ring: Float = 1f, dot: Float = 1f, spark: Float = 1f, glow: Float = 0f) {
    Canvas(modifier.size(size)) {
        val s = this.size.minDimension
        val c = Offset(this.size.width / 2, this.size.height / 2)
        val r = s * 0.36f
        val w = s * 0.12f
        if (glow > 0f) {
            drawCircle(Brush.radialGradient(listOf(C.Yellow.copy(alpha = 0.35f * glow), Color.Transparent), c, s * 0.55f), s * 0.55f, c)
        }
        drawArc(
            Color.White, startAngle = -15f, sweepAngle = 300f * ring, useCenter = false,
            topLeft = Offset(c.x - r, c.y - r), size = Size(2 * r, 2 * r), style = Stroke(w, cap = StrokeCap.Round),
        )
        drawCircle(C.Yellow, s * 0.14f * dot, c)
        if (spark > 0f) {
            val a = Math.toRadians(-45.0)
            val d = r * (1.0f + 0.12f * spark)
            drawCircle(C.Yellow, s * 0.055f * spark, Offset(c.x + d * cos(a).toFloat(), c.y + d * sin(a).toFloat()))
        }
    }
}

@Composable
fun Wordmark(size: TextUnit = 30.sp, modifier: Modifier = Modifier) {
    Row(modifier, verticalAlignment = Alignment.Bottom) {
        Text("Focus", color = C.Text, fontFamily = Fredoka, fontWeight = FontWeight.Bold, fontSize = size)
        Text(".", color = C.Yellow, fontFamily = Fredoka, fontWeight = FontWeight.Bold, fontSize = size)
    }
}

@Composable
fun LogoRow(markSize: Dp = 34.dp, text: TextUnit = 26.sp) {
    Row(verticalAlignment = Alignment.CenterVertically) {
        FocusMark(markSize)
        androidx.compose.foundation.layout.Spacer(Modifier.size(8.dp))
        Wordmark(text)
    }
}
