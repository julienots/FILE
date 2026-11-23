package com.superessence.egide.ui

import android.content.Intent
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.animation.core.Animatable
import androidx.compose.animation.core.CubicBezierEasing
import androidx.compose.animation.core.LinearEasing
import androidx.compose.animation.core.tween
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.alpha
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.BlendMode
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.CompositingStrategy
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlinx.coroutines.launch

/** Écran d'ouverture : le logo SuperEssence Studio se trace, puis l'appli s'ouvre. */
class SplashActivity : ComponentActivity() {
    private var leaving = false

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        setContent { EgideTheme { Splash(onDone = ::openApp) } }
    }

    private fun openApp() {
        if (leaving) return
        leaving = true
        startActivity(Intent(this, MainActivity::class.java))
        @Suppress("DEPRECATION")
        overridePendingTransition(android.R.anim.fade_in, android.R.anim.fade_out)
        finish()
    }
}

private val Smooth = CubicBezierEasing(0.65f, 0f, 0.35f, 1f)

@Composable
internal fun Splash(onDone: () -> Unit) {
    val drop = remember { Animatable(0f) }
    val s = remember { Animatable(0f) }
    val glow = remember { Animatable(0f) }
    val word = remember { Animatable(0f) }
    val studio = remember { Animatable(0f) }
    val shine = remember { Animatable(-0.3f) }
    val exit = remember { Animatable(1f) }

    LaunchedEffect(Unit) {
        launch { drop.animateTo(1f, tween(1100, easing = Smooth)) }
        launch { s.animateTo(1f, tween(800, delayMillis = 650, easing = Smooth)) }
        launch { glow.animateTo(1f, tween(700, delayMillis = 1250)) }
        launch { word.animateTo(1f, tween(800, delayMillis = 1200, easing = Smooth)) }
        launch { studio.animateTo(1f, tween(700, delayMillis = 1650, easing = Smooth)) }
        launch { shine.animateTo(1.3f, tween(900, delayMillis = 1900, easing = LinearEasing)) }
        exit.animateTo(0f, tween(450, delayMillis = 2900))
        onDone()
    }

    Box(
        Modifier
            .fillMaxSize()
            .background(C.Ink)
            .clickable(remember { MutableInteractionSource() }, indication = null) { onDone() },
        contentAlignment = Alignment.Center,
    ) {
        // halo de fond
        Canvas(Modifier.fillMaxSize().alpha(glow.value * exit.value)) {
            drawCircle(
                Brush.radialGradient(
                    listOf(C.Violet.copy(alpha = 0.22f), Color.Transparent),
                    center = Offset(size.width / 2, size.height * 0.42f),
                    radius = size.minDimension * 0.75f,
                ),
                radius = size.minDimension * 0.75f,
                center = Offset(size.width / 2, size.height * 0.42f),
            )
        }
        Column(
            horizontalAlignment = Alignment.CenterHorizontally,
            modifier = Modifier.graphicsLayer { alpha = exit.value; scaleX = 0.96f + 0.04f * exit.value; scaleY = scaleX },
        ) {
            EssenceLogo(
                Modifier.width(112.dp),
                dropProgress = drop.value,
                sProgress = s.value,
                glow = glow.value,
            )
            Spacer(Modifier.height(36.dp))
            Box(
                contentAlignment = Alignment.Center,
                modifier = Modifier.graphicsLayer { compositingStrategy = CompositingStrategy.Offscreen },
            ) {
                Text(
                    "SUPERESSENCE",
                    modifier = Modifier.graphicsLayer {
                        alpha = word.value
                        translationY = (1f - word.value) * 24f
                    },
                    style = TextStyle(
                        color = C.Text,
                        fontSize = 26.sp,
                        fontWeight = FontWeight.Light,
                        letterSpacing = (14 - 7 * word.value).sp,
                    ),
                )
                // reflet lumineux qui balaie le nom
                Canvas(Modifier.matchParentSize()) {
                    val x = size.width * shine.value
                    drawRect(
                        Brush.linearGradient(
                            listOf(Color.Transparent, Color.White.copy(alpha = 0.55f), Color.Transparent),
                            start = Offset(x - 60f, 0f), end = Offset(x + 60f, size.height),
                        ),
                        alpha = if (shine.value in 0f..1f) word.value else 0f,
                        blendMode = BlendMode.SrcAtop,
                    )
                }
            }
            Spacer(Modifier.height(10.dp))
            Row3(studio.value)
        }
    }
}

@Composable
private fun Row3(p: Float) {
    androidx.compose.foundation.layout.Row(
        verticalAlignment = Alignment.CenterVertically,
        modifier = Modifier.alpha(p),
    ) {
        Box(Modifier.width((28 * p).dp).height(1.dp).background(Brush.horizontalGradient(listOf(Color.Transparent, C.Violet))))
        Text(
            "  STUDIO  ",
            style = TextStyle(color = C.Cyan, fontSize = 12.sp, fontWeight = FontWeight.Medium, letterSpacing = 6.sp),
        )
        Box(Modifier.width((28 * p).dp).height(1.dp).background(Brush.horizontalGradient(listOf(C.Cyan, Color.Transparent))))
    }
}
