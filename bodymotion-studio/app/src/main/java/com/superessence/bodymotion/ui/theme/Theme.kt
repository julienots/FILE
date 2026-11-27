package com.superessence.bodymotion.ui.theme

import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Typography
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.Font
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.sp
import com.superessence.bodymotion.R

object C {
    val Bg = Color(0xFF0B0C0F)
    val Black = Color(0xFF07080B)
    val Panel = Color(0xFF121317)
    val Panel2 = Color(0xFF16181E)
    val Bar = Color(0xFF0E0F12)
    val Border = Color(0xFF1E2127)
    val Border2 = Color(0xFF2A2F3A)
    val Text = Color(0xFFE8EBF0)
    val Text2 = Color(0xFFC9D1DC)
    val Muted = Color(0xFF8A93A2)
    val Faint = Color(0xFF5C6370)
    val Cyan = Color(0xFF22D3EE)
    val CyanSoft = Color(0xFF67E8F9)
    val Violet = Color(0xFFA78BFA)
    val Warn = Color(0xFFFBBF24)
    val Danger = Color(0xFFEF4444)
    val Ok = Color(0xFF4ADE80)
    val SeYellow = Color(0xFFFFC72C)
    val AxisX = Color(0xFFF87171); val AxisY = Color(0xFF4ADE80); val AxisZ = Color(0xFF60A5FA)
}

val Grotesk = FontFamily(Font(R.font.spacegrotesk_semibold, FontWeight.SemiBold), Font(R.font.spacegrotesk_bold, FontWeight.Bold))
val InterF = FontFamily(Font(R.font.inter_regular, FontWeight.Normal), Font(R.font.inter_medium, FontWeight.Medium), Font(R.font.inter_semibold, FontWeight.SemiBold))
val Mono = FontFamily(Font(R.font.jetbrainsmono_medium, FontWeight.Medium))
val Fredoka = FontFamily(Font(R.font.fredoka_bold, FontWeight.Bold))

object T {
    val title = TextStyle(fontFamily = Grotesk, fontWeight = FontWeight.SemiBold, fontSize = 20.sp, color = C.Text)
    val h2 = TextStyle(fontFamily = Grotesk, fontWeight = FontWeight.SemiBold, fontSize = 16.sp, color = C.Text)
    val body = TextStyle(fontFamily = InterF, fontSize = 15.sp, color = C.Text)
    val bodyM = TextStyle(fontFamily = InterF, fontWeight = FontWeight.Medium, fontSize = 15.sp, color = C.Text)
    val small = TextStyle(fontFamily = InterF, fontSize = 12.sp, color = C.Muted)
    val label = TextStyle(fontFamily = Mono, fontSize = 11.sp, letterSpacing = 0.8.sp, color = C.Muted)
    val mono = TextStyle(fontFamily = Mono, fontSize = 12.sp, color = C.Text2)
}

@Composable
fun BmTheme(content: @Composable () -> Unit) {
    MaterialTheme(
        colorScheme = darkColorScheme(
            primary = C.Cyan, onPrimary = C.Black, secondary = C.Violet, background = C.Bg, surface = C.Panel,
            onBackground = C.Text, onSurface = C.Text, surfaceVariant = C.Panel2, onSurfaceVariant = C.Text2, outline = C.Border2, error = C.Danger,
        ),
        typography = Typography(bodyLarge = T.body, bodyMedium = T.body, labelLarge = T.bodyM),
        content = content,
    )
}
