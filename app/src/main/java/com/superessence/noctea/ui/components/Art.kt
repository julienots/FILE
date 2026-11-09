package com.superessence.noctea.ui.components

import androidx.compose.foundation.Canvas
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Rect
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.drawscope.DrawScope
import androidx.compose.ui.graphics.drawscope.Stroke
import kotlin.math.PI
import kotlin.math.cos
import kotlin.math.sin
import kotlin.random.Random

private data class ArtPalette(val top: Color, val bottom: Color, val glow: Color, val ink: Color)

private val palettes = mapOf(
    "moon" to ArtPalette(Color(0xFF2B2D6E), Color(0xFF0C0D2A), Color(0xFFF4D58D), Color(0xFF080920)),
    "stars" to ArtPalette(Color(0xFF1B1F5C), Color(0xFF070A24), Color(0xFFB9A8FF), Color(0xFF050716)),
    "ocean" to ArtPalette(Color(0xFF203A6E), Color(0xFF0A1530), Color(0xFFE8E2C0), Color(0xFF071022)),
    "clouds" to ArtPalette(Color(0xFF4B3C7E), Color(0xFF1A1440), Color(0xFFF7C9E0), Color(0xFF120E2C)),
    "forest" to ArtPalette(Color(0xFF1C3A4A), Color(0xFF07141C), Color(0xFFCFE8C8), Color(0xFF04100F)),
    "cabin" to ArtPalette(Color(0xFF233154), Color(0xFF0A0F1E), Color(0xFFFFC27A), Color(0xFF060A14)),
    "mountain" to ArtPalette(Color(0xFF5A3F7A), Color(0xFF1A1636), Color(0xFFFFB8A0), Color(0xFF110D24)),
    "garden" to ArtPalette(Color(0xFF163E44), Color(0xFF071A1E), Color(0xFFE9F59A), Color(0xFF051214)),
    "snow" to ArtPalette(Color(0xFF34477A), Color(0xFF111A36), Color(0xFFFFFFFF), Color(0xFF0B1226)),
    "breath" to ArtPalette(Color(0xFF26407A), Color(0xFF0D1636), Color(0xFF9FD3FF), Color(0xFF081024)),
    "lotus" to ArtPalette(Color(0xFF3A2A6A), Color(0xFF120C2E), Color(0xFFF7B6D2), Color(0xFF0D0822)),
    "heart" to ArtPalette(Color(0xFF4A2459), Color(0xFF180A24), Color(0xFFFF9FB5), Color(0xFF12071A)),
    "sun" to ArtPalette(Color(0xFF6B4C8C), Color(0xFFE8A0A0), Color(0xFFFFE3A0), Color(0xFF2A1B3E)),
    "fire" to ArtPalette(Color(0xFF3A1E2A), Color(0xFF120810), Color(0xFFFFA054), Color(0xFF0C0508)),
    "rain" to ArtPalette(Color(0xFF2A3554), Color(0xFF0C1122), Color(0xFFAFC8FF), Color(0xFF070B18)),
    "space" to ArtPalette(Color(0xFF241246), Color(0xFF050314), Color(0xFFC59BFF), Color(0xFF030210)),
)

/** Illustration nocturne générée (vectorielle, nette à toutes les tailles). */
@Composable
fun ArtIllustration(art: String, modifier: Modifier = Modifier) {
    val pal = palettes[art] ?: palettes.getValue("moon")
    Canvas(modifier) {
        drawRect(Brush.verticalGradient(listOf(pal.top, pal.bottom)))
        val w = size.width
        val h = size.height
        val rnd = Random(art.hashCode())
        fun stars(n: Int, maxY: Float = 0.6f, alpha: Float = 0.8f) {
            repeat(n) {
                drawCircle(Color.White.copy(alpha = rnd.nextFloat() * alpha + 0.15f), radius = rnd.nextFloat() * w * 0.008f + w * 0.003f,
                    center = Offset(rnd.nextFloat() * w, rnd.nextFloat() * h * maxY))
            }
        }
        fun glowAt(c: Offset, r: Float, color: Color = pal.glow, a: Float = 0.45f) =
            drawCircle(Brush.radialGradient(listOf(color.copy(alpha = a), Color.Transparent), c, r), r, c)

        when (art) {
            "moon" -> {
                stars(26)
                val c = Offset(w * 0.58f, h * 0.4f)
                glowAt(c, w * 0.42f)
                drawCrescent(c, w * 0.2f, listOf(Color(0xFFFFF4D6), pal.glow, Color(0xFFB9A8FF)), starColor = Color(0xFFFFF8E7))
                hills(pal.ink, 0.82f, 0.05f, 2)
            }
            "stars" -> {
                stars(60, 0.95f, 0.9f)
                val pts = listOf(0.2f to 0.3f, 0.35f to 0.22f, 0.5f to 0.32f, 0.66f to 0.25f, 0.78f to 0.42f, 0.62f to 0.5f)
                for (i in 0 until pts.size - 1) {
                    drawLine(pal.glow.copy(alpha = 0.5f), Offset(pts[i].first * w, pts[i].second * h), Offset(pts[i + 1].first * w, pts[i + 1].second * h), strokeWidth = w * 0.005f)
                }
                pts.forEach { drawSparkle(Offset(it.first * w, it.second * h), w * 0.03f, Color.White) }
                hills(pal.ink, 0.8f, 0.07f, 3)
            }
            "ocean" -> {
                stars(20, 0.45f)
                val c = Offset(w * 0.5f, h * 0.38f)
                glowAt(c, w * 0.35f)
                drawCircle(pal.glow, w * 0.12f, c)
                drawRect(Brush.verticalGradient(listOf(Color(0xFF13284C), pal.ink), startY = h * 0.55f), Offset(0f, h * 0.55f), Size(w, h * 0.45f))
                for (i in 0 until 7) {
                    val y = h * (0.6f + i * 0.055f)
                    val len = w * (0.08f + 0.05f * i)
                    drawLine(pal.glow.copy(alpha = 0.55f - i * 0.06f), Offset(w * 0.5f - len, y), Offset(w * 0.5f + len, y), strokeWidth = w * 0.008f, cap = StrokeCap.Round)
                }
            }
            "clouds" -> {
                stars(18, 0.4f)
                glowAt(Offset(w * 0.7f, h * 0.25f), w * 0.3f)
                drawCircle(pal.glow.copy(alpha = 0.9f), w * 0.07f, Offset(w * 0.7f, h * 0.25f))
                cloud(Offset(w * 0.3f, h * 0.5f), w * 0.22f, Color.White.copy(alpha = 0.22f))
                cloud(Offset(w * 0.68f, h * 0.66f), w * 0.26f, Color.White.copy(alpha = 0.28f))
                cloud(Offset(w * 0.35f, h * 0.84f), w * 0.3f, Color.White.copy(alpha = 0.35f))
            }
            "forest", "cabin", "garden" -> {
                stars(24, 0.5f)
                glowAt(Offset(w * 0.75f, h * 0.2f), w * 0.25f, a = 0.35f)
                drawCircle(pal.glow.copy(alpha = 0.85f), w * 0.05f, Offset(w * 0.75f, h * 0.2f))
                if (art == "garden") {
                    repeat(18) {
                        val c = Offset(rnd.nextFloat() * w, h * (0.45f + rnd.nextFloat() * 0.45f))
                        glowAt(c, w * 0.04f, a = 0.8f)
                        drawCircle(pal.glow, w * 0.006f, c)
                    }
                }
                pines(pal.ink.copy(alpha = 0.7f), 0.72f, 0.28f, 7, rnd)
                if (art == "cabin") {
                    val bx = w * 0.42f; val by = h * 0.72f; val bw = w * 0.22f; val bh = h * 0.14f
                    drawRect(Color(0xFF1A1420), Offset(bx, by), Size(bw, bh))
                    val roof = Path().apply { moveTo(bx - bw * 0.12f, by); lineTo(bx + bw / 2, by - bh * 0.7f); lineTo(bx + bw * 1.12f, by); close() }
                    drawPath(roof, Color(0xFF120C16))
                    val win = Offset(bx + bw * 0.6f, by + bh * 0.3f)
                    glowAt(win + Offset(bw * 0.08f, bh * 0.15f), w * 0.12f, a = 0.6f)
                    drawRect(pal.glow, win, Size(bw * 0.18f, bh * 0.3f))
                }
                pines(pal.ink, 0.86f, 0.3f, 9, rnd)
            }
            "mountain" -> {
                stars(16, 0.35f)
                glowAt(Offset(w * 0.5f, h * 0.55f), w * 0.55f, a = 0.35f)
                mountainLayer(Color(0xFF3A2C5E), 0.45f, rnd)
                mountainLayer(Color(0xFF261C44), 0.58f, rnd)
                mountainLayer(pal.ink, 0.72f, rnd)
            }
            "snow" -> {
                hills(Color(0xFFDDE6FF).copy(alpha = 0.18f), 0.7f, 0.08f, 2)
                pines(Color(0xFF0E1630), 0.85f, 0.25f, 8, rnd)
                repeat(70) {
                    drawCircle(Color.White.copy(alpha = rnd.nextFloat() * 0.7f + 0.2f), w * (0.004f + rnd.nextFloat() * 0.01f), Offset(rnd.nextFloat() * w, rnd.nextFloat() * h))
                }
            }
            "breath" -> {
                stars(14)
                val c = Offset(w / 2, h / 2)
                for (i in 4 downTo 1) {
                    drawCircle(pal.glow.copy(alpha = 0.08f * i), w * 0.1f * i, c)
                    drawCircle(pal.glow.copy(alpha = 0.35f), w * 0.1f * i, c, style = Stroke(w * 0.004f))
                }
                drawCircle(Color.White.copy(alpha = 0.85f), w * 0.06f, c)
            }
            "lotus" -> {
                stars(18, 0.5f)
                val c = Offset(w / 2, h * 0.62f)
                glowAt(c, w * 0.45f, a = 0.4f)
                for (i in -3..3) {
                    val ang = (i * 22f) * (PI / 180f).toFloat()
                    rotatePetal(c, w * 0.26f, w * 0.085f, ang, pal.glow.copy(alpha = 0.55f + 0.06f * (3 - kotlin.math.abs(i))))
                }
                drawLine(pal.glow.copy(alpha = 0.4f), Offset(w * 0.15f, h * 0.68f), Offset(w * 0.85f, h * 0.68f), strokeWidth = w * 0.006f)
            }
            "heart" -> {
                stars(22)
                val c = Offset(w / 2, h * 0.5f)
                glowAt(c, w * 0.45f, a = 0.5f)
                val s = w * 0.2f
                val path = Path().apply {
                    moveTo(c.x, c.y + s * 0.9f)
                    cubicTo(c.x - s * 1.6f, c.y - s * 0.1f, c.x - s * 0.7f, c.y - s * 1.2f, c.x, c.y - s * 0.4f)
                    cubicTo(c.x + s * 0.7f, c.y - s * 1.2f, c.x + s * 1.6f, c.y - s * 0.1f, c.x, c.y + s * 0.9f)
                    close()
                }
                drawPath(path, Brush.verticalGradient(listOf(Color(0xFFFFD1DC), pal.glow), startY = c.y - s, endY = c.y + s))
            }
            "sun" -> {
                val c = Offset(w / 2, h * 0.68f)
                glowAt(c, w * 0.6f, a = 0.7f)
                drawCircle(pal.glow, w * 0.16f, c)
                hills(Color(0xFF7A4E7E), 0.7f, 0.06f, 2)
                hills(pal.ink, 0.82f, 0.05f, 3)
            }
            "fire" -> {
                stars(10, 0.3f, 0.5f)
                val c = Offset(w / 2, h * 0.72f)
                glowAt(c, w * 0.6f, a = 0.55f)
                flame(c, w * 0.22f, h * 0.42f, Color(0xFFFF7A3C))
                flame(c, w * 0.15f, h * 0.3f, Color(0xFFFFB45A))
                flame(c, w * 0.08f, h * 0.17f, Color(0xFFFFE6A8))
                drawRoundRect(Color(0xFF3A2016), Offset(w * 0.28f, h * 0.76f), Size(w * 0.44f, h * 0.05f), androidx.compose.ui.geometry.CornerRadius(w * 0.02f))
            }
            "rain" -> {
                cloud(Offset(w * 0.5f, h * 0.28f), w * 0.3f, Color.White.copy(alpha = 0.18f))
                repeat(55) {
                    val x = rnd.nextFloat() * w
                    val y = h * 0.3f + rnd.nextFloat() * h * 0.7f
                    drawLine(pal.glow.copy(alpha = 0.25f + rnd.nextFloat() * 0.4f), Offset(x, y), Offset(x - w * 0.02f, y + h * 0.06f), strokeWidth = w * 0.005f, cap = StrokeCap.Round)
                }
            }
            "space" -> {
                stars(80, 1f, 0.9f)
                val c = Offset(w * 0.55f, h * 0.55f)
                glowAt(c, w * 0.45f, a = 0.4f)
                drawCircle(Brush.linearGradient(listOf(Color(0xFFC59BFF), Color(0xFF4A2C8A)), c - Offset(w * 0.15f, w * 0.15f), c + Offset(w * 0.15f, w * 0.15f)), w * 0.17f, c)
                drawOval(pal.glow.copy(alpha = 0.6f), Offset(c.x - w * 0.3f, c.y - w * 0.045f), Size(w * 0.6f, w * 0.09f), style = Stroke(w * 0.012f))
            }
        }
    }
}

private fun DrawScope.hills(color: Color, base: Float, amp: Float, waves: Int) {
    val w = size.width; val h = size.height
    val path = Path().apply {
        moveTo(0f, h)
        lineTo(0f, h * base)
        val steps = 40
        for (i in 0..steps) {
            val x = w * i / steps
            val y = h * base - h * amp * sin(i / steps.toFloat() * PI.toFloat() * waves).let { kotlin.math.abs(it) }
            lineTo(x, y)
        }
        lineTo(w, h); close()
    }
    drawPath(path, color)
}

private fun DrawScope.pines(color: Color, base: Float, height: Float, count: Int, rnd: Random) {
    val w = size.width; val h = size.height
    drawRect(color, Offset(0f, h * base), Size(w, h * (1 - base)))
    for (i in 0 until count) {
        val x = w * (i + rnd.nextFloat() * 0.6f) / count
        val th = h * height * (0.6f + rnd.nextFloat() * 0.5f)
        val tw = th * 0.38f
        val path = Path().apply {
            moveTo(x, h * base - th)
            lineTo(x - tw / 2, h * base + 2f)
            lineTo(x + tw / 2, h * base + 2f)
            close()
        }
        drawPath(path, color)
    }
}

private fun DrawScope.mountainLayer(color: Color, base: Float, rnd: Random) {
    val w = size.width; val h = size.height
    val path = Path().apply {
        moveTo(0f, h)
        lineTo(0f, h * (base + 0.1f))
        var x = 0f
        while (x < w) {
            val pw = w * (0.18f + rnd.nextFloat() * 0.2f)
            lineTo(x + pw / 2, h * (base - rnd.nextFloat() * 0.12f))
            lineTo(x + pw, h * (base + 0.08f))
            x += pw
        }
        lineTo(w, h); close()
    }
    drawPath(path, color)
}

private fun DrawScope.cloud(c: Offset, r: Float, color: Color) {
    drawCircle(color, r * 0.5f, c + Offset(-r * 0.5f, r * 0.1f))
    drawCircle(color, r * 0.65f, c + Offset(0f, -r * 0.15f))
    drawCircle(color, r * 0.5f, c + Offset(r * 0.55f, r * 0.1f))
    drawRoundRect(color, Offset(c.x - r, c.y), Size(r * 2f, r * 0.55f), androidx.compose.ui.geometry.CornerRadius(r * 0.3f))
}

private fun DrawScope.flame(base: Offset, halfW: Float, height: Float, color: Color) {
    val p = Path().apply {
        moveTo(base.x, base.y - height)
        cubicTo(base.x + halfW * 0.4f, base.y - height * 0.6f, base.x + halfW * 1.2f, base.y - height * 0.3f, base.x + halfW * 0.6f, base.y)
        lineTo(base.x - halfW * 0.6f, base.y)
        cubicTo(base.x - halfW * 1.2f, base.y - height * 0.3f, base.x - halfW * 0.4f, base.y - height * 0.6f, base.x, base.y - height)
        close()
    }
    drawPath(p, Brush.verticalGradient(listOf(color.copy(alpha = 0.3f), color), startY = base.y - height, endY = base.y))
}

private fun DrawScope.rotatePetal(origin: Offset, len: Float, width: Float, angle: Float, color: Color) {
    val tip = Offset(origin.x + len * sin(angle), origin.y - len * cos(angle))
    val nx = cos(angle) * width
    val ny = sin(angle) * width
    val mid = Offset((origin.x + tip.x) / 2, (origin.y + tip.y) / 2)
    val p = Path().apply {
        moveTo(origin.x, origin.y)
        quadraticTo(mid.x + nx, mid.y + ny, tip.x, tip.y)
        quadraticTo(mid.x - nx, mid.y - ny, origin.x, origin.y)
        close()
    }
    drawPath(p, color)
}

@Suppress("unused")
private fun unusedRect() = Rect.Zero
