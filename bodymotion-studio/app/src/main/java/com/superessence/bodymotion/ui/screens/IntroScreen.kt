package com.superessence.bodymotion.ui.screens

import androidx.compose.animation.core.Animatable
import androidx.compose.animation.core.LinearEasing
import androidx.compose.animation.core.tween
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.*
import androidx.compose.material3.Text
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.alpha
import androidx.compose.ui.draw.scale
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.PathEffect
import androidx.compose.ui.graphics.PathMeasure
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.StrokeJoin
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.graphics.drawscope.rotate
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.text.SpanStyle
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.buildAnnotatedString
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.withStyle
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.superessence.bodymotion.ui.theme.*
import kotlin.math.PI
import kotlin.math.cos
import kotlin.math.sin
import kotlin.random.Random

/**
 * Séquence de démarrage : signature SUPER ESSENCE STUDIO (symbole lumineux, particules, logotype)
 * puis apparition du logo BodyMotion Studio. Toucher pour passer.
 */
@Composable
fun IntroScreen(full: Boolean, onDone: () -> Unit) {
    val total = if (full) 6200 else 3200
    val t = remember { Animatable(0f) }
    var done by remember { mutableStateOf(false) }
    fun finish() { if (!done) { done = true; onDone() } }
    LaunchedEffect(Unit) { t.animateTo(total.toFloat(), tween(total, easing = LinearEasing)); finish() }
    val ms = t.value
    // Phases (ms) — version courte : mêmes étapes, accélérées.
    val k = total / 6200f
    fun ph(a: Float, b: Float) = ((ms - a * k) / ((b - a) * k)).coerceIn(0f, 1f)
    val symbolIn = ph(0f, 1100f)
    val wordIn = ph(900f, 2200f)
    val seOut = ph(3000f, 3500f)
    val bmIn = ph(3300f, 4500f)
    val bmWord = ph(4000f, 5000f)
    val particles = remember { List(46) { Triple(Random.nextFloat(), Random.nextFloat(), 0.4f + Random.nextFloat() * 0.6f) } }

    Box(
        Modifier.fillMaxSize().background(Color.Black)
            .clickable(interactionSource = remember { MutableInteractionSource() }, indication = null) { finish() },
        contentAlignment = Alignment.Center,
    ) {
        // Fond : halo doré (Super Essence) puis bleu nuit (BodyMotion)
        Canvas(Modifier.fillMaxSize()) {
            val c = Offset(size.width / 2, size.height * 0.42f)
            drawCircle(Brush.radialGradient(listOf(Color(0xFF2A1F05).copy(alpha = 0.9f * symbolIn * (1 - seOut)), Color.Transparent), c, size.minDimension * 0.75f), size.minDimension * 0.75f, c)
            drawCircle(Brush.radialGradient(listOf(Color(0xFF0E2430).copy(alpha = bmIn), Color.Transparent), c, size.minDimension * 0.85f), size.minDimension * 0.85f, c)
            // Particules lumineuses (profondeur : taille et vitesse variables)
            for ((x0, y0, d) in particles) {
                val life = ((ms / 5200f) * d + y0) % 1f
                val x = x0 * size.width + sin((life * 6 + x0 * 10).toDouble()).toFloat() * 14f * d
                val y = size.height * (0.85f - life * 0.6f)
                val a = (sin(life * PI).toFloat() * symbolIn * (1 - seOut * 0.7f)).coerceIn(0f, 1f)
                val col = if (bmIn > 0.5f) C.Cyan else C.SeYellow
                drawCircle(col.copy(alpha = a * 0.35f), 6f * d, Offset(x, y))
                drawCircle(Color.White.copy(alpha = a * 0.9f), 1.6f * d + 0.6f, Offset(x, y))
            }
        }

        // ---------------- Signature Super Essence Studio
        if (seOut < 1f) Column(
            Modifier.alpha(1 - seOut).graphicsLayer { translationY = -seOut * 40f },
            horizontalAlignment = Alignment.CenterHorizontally,
        ) {
            Canvas(Modifier.size(150.dp).scale(0.7f + 0.3f * easeOut(symbolIn)).alpha(symbolIn)) {
                val c = center; val r = size.minDimension / 2
                rotate(ms / 50f) {
                    drawCircle(C.SeYellow.copy(alpha = 0.28f), r * 0.95f, c, style = Stroke(1.5f, pathEffect = PathEffect.dashPathEffect(floatArrayOf(3f, 11f))))
                    drawArc(C.SeYellow.copy(alpha = 0.55f), 0f, 70f, false, Offset(c.x - r * 0.76f, c.y - r * 0.76f), androidx.compose.ui.geometry.Size(r * 1.52f, r * 1.52f), style = Stroke(2f, cap = StrokeCap.Round))
                }
                val pulse = 1f + 0.05f * sin(ms / 400f)
                drawCircle(Brush.radialGradient(listOf(Color(0xFFFFE7A0).copy(alpha = 0.8f), C.SeYellow.copy(alpha = 0.25f), Color.Transparent), c, r * 0.72f * pulse), r * 0.72f * pulse, c)
                fun star(rr: Float, inner: Float): Path = Path().apply {
                    for (i in 0 until 4) {
                        val a = (i * PI / 2 - PI / 2).toFloat()
                        val p = Offset(c.x + cos(a) * rr, c.y + sin(a) * rr)
                        val m = Offset(c.x + cos(a + PI.toFloat() / 4) * inner, c.y + sin(a + PI.toFloat() / 4) * inner)
                        if (i == 0) moveTo(p.x, p.y) else lineTo(p.x, p.y)
                        quadraticTo(c.x + cos(a + PI.toFloat() / 4) * inner * 0.2f, c.y + sin(a + PI.toFloat() / 4) * inner * 0.2f, m.x, m.y)
                        val nx = Offset(c.x + cos(a + PI.toFloat() / 2) * rr, c.y + sin(a + PI.toFloat() / 2) * rr)
                        quadraticTo(c.x + cos(a + PI.toFloat() / 4) * inner * 0.2f, c.y + sin(a + PI.toFloat() / 4) * inner * 0.2f, nx.x, nx.y)
                    }
                    close()
                }
                drawPath(star(r * 0.62f * pulse, r * 0.16f), Brush.linearGradient(listOf(Color(0xFFFFF4CC), Color(0xFFFFB800)), Offset(c.x - r, c.y - r), Offset(c.x + r, c.y + r)))
                drawPath(star(r * 0.3f, r * 0.08f), Color.White.copy(alpha = 0.9f))
            }
            Spacer(Modifier.height(36.dp))
            Box(Modifier.alpha(wordIn).graphicsLayer { translationY = (1 - easeOut(wordIn)) * 24f }) {
                val word = buildAnnotatedString {
                    withStyle(SpanStyle(color = Color.White)) { append("Super") }
                    withStyle(SpanStyle(color = C.SeYellow)) { append("Essence.") }
                }
                Text(word, style = TextStyle(fontFamily = Fredoka, fontWeight = FontWeight.Bold, fontSize = 44.sp, color = Color(0xFF2B2B2B)), modifier = Modifier.offset(3.dp, 4.dp))
                Text(word, style = TextStyle(fontFamily = Fredoka, fontWeight = FontWeight.Bold, fontSize = 44.sp))
            }
            Spacer(Modifier.height(12.dp))
            Text("S T U D I O", style = TextStyle(fontFamily = Grotesk, fontSize = 14.sp, letterSpacing = 5.sp, color = Color(0xFFE6E1D3)), modifier = Modifier.alpha(ph(1500f, 2400f)))
            Spacer(Modifier.height(48.dp))
            Text("PRÉSENTE", style = TextStyle(fontFamily = InterF, fontSize = 12.sp, letterSpacing = 4.sp, color = Color(0xFFA8A294)), modifier = Modifier.alpha(ph(2000f, 2700f)))
        }

        // ---------------- BodyMotion Studio
        if (bmIn > 0f) Column(horizontalAlignment = Alignment.CenterHorizontally, modifier = Modifier.alpha(bmIn)) {
            Box(Modifier.size(150.dp).scale(0.85f + 0.15f * easeOut(bmIn)).background(Brush.radialGradient(listOf(Color(0xFF173040), Color(0xFF0B0C11))), androidx.compose.foundation.shape.RoundedCornerShape(38.dp)), contentAlignment = Alignment.Center) {
                DrawnLogo(progress = easeOut(bmIn), modifier = Modifier.size(104.dp))
            }
            Spacer(Modifier.height(28.dp))
            Column(horizontalAlignment = Alignment.CenterHorizontally, modifier = Modifier.alpha(bmWord).graphicsLayer { translationY = (1 - easeOut(bmWord)) * 18f }) {
                Text(buildAnnotatedString {
                    withStyle(SpanStyle(color = C.Text)) { append("Body") }
                    withStyle(SpanStyle(brush = Brush.linearGradient(listOf(C.Cyan, C.Violet)))) { append("Motion") }
                }, style = TextStyle(fontFamily = Grotesk, fontWeight = FontWeight.Bold, fontSize = 42.sp, letterSpacing = (-1).sp))
                Spacer(Modifier.height(8.dp))
                Text("S T U D I O", style = TextStyle(fontFamily = Grotesk, fontSize = 13.sp, letterSpacing = 4.sp, color = C.Text2))
            }
        }
        Text("SUPER ESSENCE STUDIO — CREATE. CAPTURE. ANIMATE.", style = T.label.copy(fontSize = 9.sp, color = C.Faint),
            modifier = Modifier.align(Alignment.BottomCenter).navigationBarsPadding().padding(bottom = 28.dp).alpha(bmWord))
    }
}

private fun easeOut(x: Float) = 1 - (1 - x) * (1 - x) * (1 - x)

/** Logo BodyMotion dessiné progressivement (tracé des os puis articulations). */
@Composable
private fun DrawnLogo(progress: Float, modifier: Modifier) {
    Canvas(modifier) {
        val s = size.width / 64f
        val brush = Brush.linearGradient(listOf(C.Violet, C.Cyan), Offset(6 * s, 54 * s), Offset(54 * s, 8 * s))
        val segs = listOf(
            floatArrayOf(36f, 19f, 30f, 36f), floatArrayOf(35f, 21f, 42f, 28f), floatArrayOf(42f, 28f, 49f, 23f),
            floatArrayOf(35f, 21f, 27f, 26f), floatArrayOf(27f, 26f, 21f, 21f), floatArrayOf(30f, 36f, 39f, 43f),
            floatArrayOf(39f, 43f, 37f, 55f), floatArrayOf(30f, 36f, 24f, 46f), floatArrayOf(24f, 46f, 14f, 49f),
        )
        val trail = Path().apply { moveTo(3 * s, 50 * s); cubicTo(9 * s, 44 * s, 14 * s, 42 * s, 21 * s, 41 * s); moveTo(10 * s, 28 * s); cubicTo(13 * s, 24 * s, 17 * s, 22 * s, 22 * s, 21 * s) }
        val pm = PathMeasure().apply { setPath(trail, false) }
        val part = Path(); pm.getSegment(0f, pm.length * progress, part, true)
        drawPath(trail, brush, alpha = 0.5f * progress, style = Stroke(2.2f * s, cap = StrokeCap.Round))
        segs.forEachIndexed { i, a ->
            val p = ((progress * segs.size) - i).coerceIn(0f, 1f)
            if (p > 0) drawLine(brush, Offset(a[0] * s, a[1] * s), Offset((a[0] + (a[2] - a[0]) * p) * s, (a[1] + (a[3] - a[1]) * p) * s), 3.8f * s, StrokeCap.Round)
        }
        drawCircle(brush, 5.2f * s * progress, Offset(39 * s, 11.5f * s))
        if (progress > 0.8f) for (p in listOf(Offset(35f, 21f), Offset(42f, 28f), Offset(30f, 36f), Offset(39f, 43f), Offset(27f, 26f), Offset(24f, 46f))) {
            val q = Offset(p.x * s, p.y * s); val a = (progress - 0.8f) / 0.2f
            drawCircle(C.Black, 1.9f * s, q, alpha = a); drawCircle(Color(0xFFE9FBFF), 1.9f * s, q, alpha = a, style = Stroke(1.4f * s, join = StrokeJoin.Round))
        }
    }
}
