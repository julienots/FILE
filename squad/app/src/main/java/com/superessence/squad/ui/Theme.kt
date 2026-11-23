package com.superessence.squad.ui

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Icon
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.produceState
import androidx.compose.runtime.remember
import androidx.compose.runtime.withFrameNanos
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.drawscope.DrawScope
import androidx.compose.ui.graphics.drawscope.drawIntoCanvas
import androidx.compose.ui.graphics.nativeCanvas
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.Font
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.superessence.squad.R
import com.superessence.squad.engine.Phase
import com.superessence.squad.model.Act
import com.superessence.squad.model.Look
import com.superessence.squad.model.Mascot
import com.superessence.squad.render.MascotPainter
import com.superessence.squad.render.Pose
import kotlin.math.cos
import kotlin.math.sin

val Fredoka = FontFamily(Font(R.font.fredoka_bold, FontWeight.Bold), Font(R.font.fredoka_medium, FontWeight.Medium))
val Nunito = FontFamily(Font(R.font.nunito_400, FontWeight.Normal), Font(R.font.nunito_700, FontWeight.Bold))

object C {
    val bg = Color(0xFF0A0A0A)
    val card = Color(0xFF161616)
    val card2 = Color(0xFF1F1F1F)
    val line = Color(0xFF2A2A2A)
    val yellow = Color(0xFFFFC928)
    val text = Color(0xFFF5F5F5)
    val dim = Color(0xFF9A9A9A)
    val red = Color(0xFFE53935)
    val green = Color(0xFF5BC26A)
    val blue = Color(0xFF4AA3FF)
}

val H1 = TextStyle(fontFamily = Fredoka, fontWeight = FontWeight.Bold, fontSize = 30.sp, color = C.text)
val H2 = TextStyle(fontFamily = Fredoka, fontWeight = FontWeight.Bold, fontSize = 20.sp, color = C.text)
val Body = TextStyle(fontFamily = Nunito, fontSize = 15.sp, color = C.text)
val Small = TextStyle(fontFamily = Nunito, fontSize = 13.sp, color = C.dim)
val Label = TextStyle(fontFamily = Nunito, fontWeight = FontWeight.Bold, fontSize = 11.sp, color = C.dim, letterSpacing = 1.5.sp)

/** Horloge d'animation (secondes). */
@Composable
fun animTime(): Float {
    val t by produceState(0f) {
        val start = withFrameNanos { it }
        while (true) withFrameNanos { value = (it - start) / 1e9f }
    }
    return t
}

/** Activité montrée dans l'appli selon l'heure et la personnalité. */
fun previewAct(m: Mascot, phase: Phase, t: Float): Act = when {
    phase == Phase.SLEEP || m.energy < 8f -> Act.SLEEP
    phase.isMeal -> Act.EAT
    phase == Phase.WORKOUT -> if ((t / 6f).toInt() % 2 == 0) Act.TRAIN else Act.FLEX
    phase == Phase.WAKING -> Act.COFFEE
    phase == Phase.WIND_DOWN -> if ((t / 5f).toInt() % 3 == 0) Act.YAWN else Act.IDLE
    else -> {
        val acts = m.archetype.hobbies.keys.filter { it != Act.WALK && it != Act.PUSHUP }.ifEmpty { listOf(Act.IDLE) }
        val slot = (t / 6f).toInt() + (m.id.hashCode() and 7)
        if (slot % 3 == 0) Act.IDLE else acts[slot % acts.size]
    }
}

@Composable
fun MascotCanvas(look: Look, act: Act, modifier: Modifier, fitness: Float = 30f, seed: Int = 0, talking: Boolean = false, facing: Float = 1f) {
    val t = animTime()
    val pose = remember { Pose() }
    Canvas(modifier) {
        pose.act = act; pose.t = t; pose.fitness = fitness; pose.seed = seed; pose.talking = talking; pose.facing = facing
        drawIntoCanvas { MascotPainter.draw(it.nativeCanvas, look, pose, size.width, size.height) }
    }
}

@Composable
fun StatBar(label: String, value: Float, color: Color, modifier: Modifier = Modifier) {
    Column(modifier) {
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
            Text(label, style = Small.copy(fontSize = 12.sp))
            Text("${value.toInt()}", style = Small.copy(fontSize = 12.sp, color = C.text, fontWeight = FontWeight.Bold))
        }
        Spacer(Modifier.height(4.dp))
        Box(Modifier.fillMaxWidth().height(7.dp).clip(RoundedCornerShape(4.dp)).background(C.line)) {
            Box(Modifier.fillMaxWidth(value / 100f).height(7.dp).clip(RoundedCornerShape(4.dp)).background(if (value < 25) C.red else color))
        }
    }
}

@Composable
fun YellowButton(text: String, icon: ImageVector? = null, modifier: Modifier = Modifier, enabled: Boolean = true, onClick: () -> Unit) {
    Row(
        modifier.clip(RoundedCornerShape(14.dp)).background(if (enabled) C.yellow else C.line)
            .clickable(enabled = enabled, onClick = onClick).padding(horizontal = 18.dp, vertical = 14.dp),
        verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.Center,
    ) {
        if (icon != null) { Icon(icon, null, tint = Color.Black, modifier = Modifier.size(20.dp)); Spacer(Modifier.width(8.dp)) }
        Text(text, style = Body.copy(fontWeight = FontWeight.Bold, color = Color.Black))
    }
}

@Composable
fun GhostButton(text: String, icon: ImageVector? = null, modifier: Modifier = Modifier, color: Color = C.text, onClick: () -> Unit) {
    Row(
        modifier.clip(RoundedCornerShape(14.dp)).border(1.dp, C.line, RoundedCornerShape(14.dp)).background(C.card)
            .clickable(onClick = onClick).padding(horizontal = 14.dp, vertical = 12.dp),
        verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.Center,
    ) {
        if (icon != null) { Icon(icon, null, tint = color, modifier = Modifier.size(18.dp)); Spacer(Modifier.width(6.dp)) }
        Text(text, style = Body.copy(fontWeight = FontWeight.Bold, color = color, fontSize = 14.sp))
    }
}

/** Le logo SuperEssence Studio : hexagone jaune, monogramme S taillé, point signature. */
fun DrawScope.studioMark(size: Float, center: Offset, progress: Float = 1f, rotation: Float = 0f, cutColor: Color = Color.Black) {
    val r = size / 2f
    val hex = Path().apply {
        for (i in 0..5) {
            val a = Math.toRadians((60.0 * i - 90.0) + rotation).toFloat()
            val p = Offset(center.x + r * cos(a), center.y + r * sin(a))
            if (i == 0) moveTo(p.x, p.y) else lineTo(p.x, p.y)
        }
        close()
    }
    drawPath(hex, Brush.linearGradient(listOf(Color(0xFFFFD84D), Color(0xFFFFB800)), Offset(center.x - r, center.y - r), Offset(center.x + r, center.y + r)))
    if (progress <= 0f) return
    // S monogramme géométrique (unités sur un carré de 28 centré)
    val u = size / 40f
    fun o(x: Float, y: Float) = Offset(center.x + (x - 54f) * u, center.y + (y - 54.5f) * u)
    val s = Path().apply {
        val pts = listOf(66f to 40f, 46f to 40f, 40f to 46f, 40f to 51f, 46f to 57f, 60f to 57f, 60f to 63f, 42f to 63f, 42f to 69f, 62f to 69f, 68f to 63f, 68f to 57f, 62f to 51f, 48f to 51f, 48f to 46f, 66f to 46f)
        pts.forEachIndexed { i, (x, y) -> val p = o(x, y); if (i == 0) moveTo(p.x, p.y) else lineTo(p.x, p.y) }
        close()
    }
    drawPath(s, cutColor.copy(alpha = progress))
}

@Composable
fun StudioMark(size: Dp, modifier: Modifier = Modifier) {
    Canvas(modifier.size(size)) { studioMark(this.size.minDimension, center) }
}
