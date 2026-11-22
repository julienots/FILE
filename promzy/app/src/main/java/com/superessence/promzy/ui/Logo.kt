package com.superessence.promzy.ui

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.PathFillType
import androidx.compose.ui.graphics.drawscope.rotate
import androidx.compose.ui.graphics.drawscope.translate
import androidx.compose.ui.text.SpanStyle
import androidx.compose.ui.text.buildAnnotatedString
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.withStyle
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.TextUnit
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp

/** Forme de l'étiquette (coordonnées 0..100) percée d'un œillet et d'un éclair. */
private fun tagPath(s: Float, boltProgress: Float = 1f): Path = Path().apply {
    fillType = PathFillType.EvenOdd
    fun p(x: Float, y: Float) = Offset(x * s / 100f, y * s / 100f)
    moveTo(p(30f, 20f).x, p(30f, 20f).y)
    lineTo(p(72f, 20f).x, p(72f, 20f).y)
    quadraticTo(p(80f, 20f).x, p(80f, 20f).y, p(80f, 28f).x, p(80f, 28f).y)
    lineTo(p(80f, 74f).x, p(80f, 74f).y)
    quadraticTo(p(80f, 82f).x, p(80f, 82f).y, p(72f, 82f).x, p(72f, 82f).y)
    lineTo(p(30f, 82f).x, p(30f, 82f).y)
    lineTo(p(10f, 51f).x, p(10f, 51f).y)
    close()
    addOval(androidx.compose.ui.geometry.Rect(p(24f, 51f), 5.5f * s / 100f))
    if (boltProgress > 0f) {
        val pts = listOf(60f to 29f, 44f to 54f, 54f to 54f, 49f to 73f, 67f to 46f, 57f to 46f, 63f to 29f)
        val c = Offset(55f, 51f)
        pts.forEachIndexed { i, (x, y) ->
            val q = p(c.x + (x - c.x) * boltProgress, c.y + (y - c.y) * boltProgress)
            if (i == 0) moveTo(q.x, q.y) else lineTo(q.x, q.y)
        }
        close()
    }
}

/** Le symbole Promzy : une étiquette de prix jaune inclinée, frappée d'un éclair. */
@Composable
fun PromzyMark(size: Dp, modifier: Modifier = Modifier, tilt: Float = -14f, bolt: Float = 1f, color: Color = Yellow, shadow: Boolean = true) {
    Canvas(modifier.size(size)) {
        val s = this.size.minDimension
        rotate(tilt) {
            if (shadow) translate(s * 0.03f, s * 0.035f) { drawPath(tagPath(s, bolt), Color(0xFF3A2E05)) }
            drawPath(tagPath(s, bolt), color)
        }
    }
}

@Composable
fun PromzyWordmark(fontSize: TextUnit = 30.sp, modifier: Modifier = Modifier) {
    Text(
        buildAnnotatedString {
            withStyle(SpanStyle(color = TextHi)) { append("prom") }
            withStyle(SpanStyle(color = Yellow)) { append("zy") }
        },
        fontFamily = Fredoka, fontWeight = FontWeight.Bold, fontSize = fontSize, modifier = modifier,
    )
}

@Composable
fun PromzyLockup(markSize: Dp = 34.dp, fontSize: TextUnit = 26.sp, modifier: Modifier = Modifier) {
    Row(modifier, verticalAlignment = Alignment.CenterVertically) {
        PromzyMark(markSize)
        Spacer(Modifier.width(6.dp))
        PromzyWordmark(fontSize)
    }
}
