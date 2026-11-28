package com.superessence.voxea.ui

import androidx.compose.animation.core.Animatable
import androidx.compose.animation.core.CubicBezierEasing
import androidx.compose.animation.core.FastOutSlowInEasing
import androidx.compose.animation.core.LinearEasing
import androidx.compose.animation.core.RepeatMode
import androidx.compose.animation.core.animateFloat
import androidx.compose.animation.core.infiniteRepeatable
import androidx.compose.animation.core.rememberInfiniteTransition
import androidx.compose.animation.core.tween
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.navigationBarsPadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.alpha
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.drawWithContent
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.BlendMode
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.CompositingStrategy
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.TransformOrigin
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.em
import androidx.compose.ui.unit.sp
import com.superessence.voxea.R
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import kotlin.math.sin

private val Pop = CubicBezierEasing(0.34f, 1.56f, 0.64f, 1f)
private val Drop = CubicBezierEasing(0.3f, 1.6f, 0.5f, 1f)

/**
 * Démarrage : intro « SuperEssence. STUDIO » (reprise fidèle de la démo HTML), puis écran de
 * chargement du projet VOXÉA (sa propre palette), bouton « Toucher pour commencer ».
 */
@Composable
fun IntroScreen(canSkip: Boolean, onSeen: () -> Unit, onDone: () -> Unit) {
    var phase by remember { mutableIntStateOf(0) } // 0 = studio, 1 = chargement
    val studioAlpha = remember { Animatable(1f) }
    val scope = rememberCoroutineScope()
    var leaving by remember { mutableStateOf(false) }

    fun leaveStudio() {
        if (leaving) return
        leaving = true
        onSeen()
        scope.launch { studioAlpha.animateTo(0f, tween(350)); phase = 1 }
    }

    LaunchedEffect(Unit) { delay(2350); leaveStudio() }

    Box(Modifier.fillMaxSize().background(Color.Black)) {
        if (phase == 0) {
            StudioIntro(
                Modifier.fillMaxSize().alpha(studioAlpha.value)
                    .pointerInput(canSkip) { detectTapGestures { if (canSkip) leaveStudio() } },
            )
        } else {
            ProjectLoad(onDone)
        }
    }
}

@Composable
private fun StudioIntro(modifier: Modifier) {
    val word = remember { Animatable(0f) }
    val dot = remember { Animatable(0f) }
    val ring = remember { Animatable(0f) }
    val shine = remember { Animatable(0f) }
    val spread = remember { Animatable(0f) }
    LaunchedEffect(Unit) {
        launch { delay(150); word.animateTo(1f, tween(620, easing = Pop)) }
        launch { delay(720); dot.animateTo(1f, tween(500, easing = Drop)) }
        launch { delay(1000); ring.animateTo(1f, tween(550, easing = FastOutSlowInEasing)) }
        launch { delay(1050); shine.animateTo(1f, tween(700, easing = FastOutSlowInEasing)) }
        launch { delay(1100); spread.animateTo(1f, tween(700, easing = FastOutSlowInEasing)) }
    }
    Column(modifier, horizontalAlignment = Alignment.CenterHorizontally, verticalArrangement = androidx.compose.foundation.layout.Arrangement.Center) {
        Box(Modifier.fillMaxWidth(0.78f).widthIn(max = 420.dp).aspectRatio(720f / 152f)) {
            val w = word.value
            Image(
                painterResource(R.drawable.se_word), "SuperEssence",
                Modifier.fillMaxSize().graphicsLayer {
                    val s = 0.62f + 0.38f * w
                    scaleX = s; scaleY = s
                    alpha = (w / 0.6f).coerceIn(0f, 1f)
                    transformOrigin = TransformOrigin(0.5f, 0.6f)
                },
            )
            val d = dot.value
            Image(
                painterResource(R.drawable.se_dot), null,
                Modifier.fillMaxSize().graphicsLayer {
                    translationY = -1.4f * size.height * (1f - d)
                    alpha = (d / 0.3f).coerceIn(0f, 1f)
                },
            )
            // Anneau jaune qui s'élargit autour du point
            Canvas(Modifier.fillMaxSize()) {
                val r = ring.value
                if (r > 0f && r < 1f) {
                    val diam = size.width * 0.07f * (0.3f + 2.3f * r)
                    val c = Offset(size.width * (1f - 0.006f) - size.width * 0.035f, size.height * (1f - 0.12f) - size.width * 0.035f)
                    drawCircle(Vx.StudioYellow.copy(alpha = 0.9f * (1f - r)), diam / 2, c, style = Stroke(3.dp.toPx()))
                }
            }
            // Reflet lumineux masqué par le logo
            val sh = shine.value
            if (sh > 0f && sh < 1f) {
                Box(
                    Modifier.fillMaxSize()
                        .graphicsLayer { compositingStrategy = CompositingStrategy.Offscreen }
                        .drawWithContent {
                            drawContent()
                            val x = size.width * (1.5f - 2.1f * sh)
                            drawRect(
                                Brush.linearGradient(
                                    listOf(Color.Transparent, Color.White.copy(alpha = 0.85f), Color.Transparent),
                                    start = Offset(x - size.width * 0.3f, 0f), end = Offset(x + size.width * 0.3f, size.height),
                                ),
                                blendMode = BlendMode.SrcIn,
                            )
                        },
                ) { Image(painterResource(R.drawable.se_word), null, Modifier.fillMaxSize()) }
            }
        }
        val sp = spread.value
        Text(
            "STUDIO",
            Modifier.padding(top = 22.dp).alpha(sp),
            color = Color.White.copy(alpha = 0.72f), fontWeight = FontWeight.ExtraBold, fontSize = 13.sp,
            letterSpacing = (0.9f - 0.4f * sp).em,
        )
    }
}

private val tips = listOf(
    "Lis à ton rythme : chaque réplique devient un fichier.",
    "Touche l'écran ou un bouton de volume pour passer à la suivante.",
    "Une voix de femme, d'homme ou d'enfant… à partir de la tienne.",
    "Rate une phrase ? « Refaire » ne reprend que cette réplique.",
)

@Composable
private fun ProjectLoad(onDone: () -> Unit) {
    var progress by remember { mutableFloatStateOf(0f) }
    var tip by remember { mutableIntStateOf(0) }
    val fadeIn = remember { Animatable(0f) }
    LaunchedEffect(Unit) {
        launch { fadeIn.animateTo(1f, tween(400)) }
        var p = 0f
        while (p < 1f) {
            delay(140)
            p = (p + 0.06f + (Math.random() * 0.1).toFloat()).coerceAtMost(1f)
            progress = p
            if (Math.random() < 0.15) tip = (tip + 1) % tips.size
        }
    }
    val inf = rememberInfiniteTransition(label = "bob")
    val bob by inf.animateFloat(0f, 1f, infiniteRepeatable(tween(3000, easing = LinearEasing), RepeatMode.Restart), label = "b")
    val ready = progress >= 1f

    Box(
        Modifier.fillMaxSize().alpha(fadeIn.value)
            .background(Brush.radialGradient(listOf(Color(0xFF3A1F5C), Vx.Bg, Color(0xFF0B0616)), radius = 1600f))
            .clickable(remember { MutableInteractionSource() }, null, enabled = ready) { onDone() }
            .statusBarsPadding().navigationBarsPadding(),
    ) {
        Column(Modifier.align(Alignment.Center), horizontalAlignment = Alignment.CenterHorizontally) {
            Emblem(Modifier.size(132.dp, 150.dp).graphicsLayer { translationY = -5.dp.toPx() * sin(bob * 2 * Math.PI).toFloat().coerceAtLeast(0f) }, bob)
            Spacer(Modifier.height(14.dp))
            Text("VOXÉA", fontFamily = Fredoka, fontWeight = FontWeight.Bold, fontSize = 44.sp, color = Vx.Cream,
                style = androidx.compose.ui.text.TextStyle(shadow = androidx.compose.ui.graphics.Shadow(Color(0xFF0B0616), Offset(0f, 8f), 0f)))
            Text("DIALOGUES AVEC TA VOIX", color = Vx.Cyan, fontFamily = Nunito, fontWeight = FontWeight.Bold, fontSize = 13.sp, letterSpacing = 0.25.em)
            Box(Modifier.padding(top = 44.dp).width(280.dp).height(18.dp).clip(RoundedCornerShape(12.dp)).background(Color(0xFF0B0616)).padding(3.dp)) {
                Box(Modifier.fillMaxHeight().fillMaxWidth(progress).clip(RoundedCornerShape(9.dp))
                    .background(Brush.verticalGradient(listOf(Vx.Magenta, Color(0xFFB8336F)))))
            }
            Text("${(progress * 100).toInt()} %", Modifier.padding(top = 10.dp).alpha(0.8f), color = Vx.Cream, fontWeight = FontWeight.ExtraBold, fontSize = 13.sp)
            Box(Modifier.height(90.dp), contentAlignment = Alignment.Center) {
                if (ready) {
                    Box(
                        Modifier.padding(top = 30.dp).clip(RoundedCornerShape(16.dp)).background(Vx.Cream)
                            .clickable { onDone() }.padding(horizontal = 30.dp, vertical = 14.dp),
                    ) { Text("Toucher pour commencer", color = Vx.Bg, fontFamily = Fredoka, fontWeight = FontWeight.Bold, fontSize = 18.sp) }
                }
            }
        }
        Text(
            tips[tip], Modifier.align(Alignment.BottomCenter).padding(start = 24.dp, end = 24.dp, bottom = 36.dp).alpha(0.75f),
            color = Vx.Cream, fontSize = 14.sp, textAlign = TextAlign.Center, fontStyle = androidx.compose.ui.text.font.FontStyle.Italic,
            maxLines = 2, overflow = TextOverflow.Ellipsis,
        )
    }
}

/** Emblème : micro + ondes animées (magenta à gauche, cyan à droite). */
@Composable
fun Emblem(modifier: Modifier, t: Float = 0f) {
    Canvas(modifier) {
        val w = size.width; val h = size.height
        val cx = w / 2
        // ondes
        val heights = listOf(0.18f, 0.32f, 0.22f)
        for ((i, base) in heights.withIndex()) {
            val k = base + 0.06f * sin((t * 2 * Math.PI + i).toFloat()).toFloat()
            val dx = w * (0.30f + i * 0.09f)
            val stroke = w * 0.045f
            drawLine(Vx.Magenta, Offset(cx - dx, h * (0.42f - k / 2)), Offset(cx - dx, h * (0.42f + k / 2)), stroke, StrokeCap.Round)
            drawLine(Vx.Cyan, Offset(cx + dx, h * (0.42f - k / 2)), Offset(cx + dx, h * (0.42f + k / 2)), stroke, StrokeCap.Round)
        }
        // micro
        val mw = w * 0.26f; val mh = h * 0.40f
        drawRoundRect(Vx.Cream, Offset(cx - mw / 2, h * 0.14f), Size(mw, mh), CornerRadius(mw / 2))
        for (j in 0..2) drawLine(Color(0xFF3A1F5C), Offset(cx - mw / 2 + 4f, h * (0.36f + j * 0.05f)), Offset(cx + mw / 2 - 4f, h * (0.36f + j * 0.05f)), 3f)
        drawArc(Vx.Cream, 0f, 180f, false, Offset(cx - mw * 0.85f, h * 0.24f), Size(mw * 1.7f, mh * 0.95f), style = Stroke(w * 0.05f, cap = StrokeCap.Round))
        drawLine(Vx.Cream, Offset(cx, h * 0.69f), Offset(cx, h * 0.82f), w * 0.05f, StrokeCap.Round)
        drawLine(Vx.Cream, Offset(cx - w * 0.12f, h * 0.84f), Offset(cx + w * 0.12f, h * 0.84f), w * 0.05f, StrokeCap.Round)
    }
}
