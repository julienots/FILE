package com.superessence.voxea.ui

import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.Font
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.material3.Typography
import androidx.compose.ui.unit.sp
import com.superessence.voxea.R
import com.superessence.voxea.core.VoiceType

object Vx {
    val Bg = Color(0xFF140A24)
    val Surface = Color(0xFF22143A)
    val Surface2 = Color(0xFF2E1C4D)
    val Line = Color(0xFF3D2A60)
    val Magenta = Color(0xFFE24A8D)
    val Cyan = Color(0xFF3FD0C9)
    val Cream = Color(0xFFF4EDE4)
    val Muted = Color(0xFFA99BC4)
    val Ok = Color(0xFF7BD88F)
    val Rec = Color(0xFFFF4D5E)
    // Couleurs réservées à l'intro du studio
    val StudioYellow = Color(0xFFFCCC24)
}

val Fredoka = FontFamily(Font(R.font.fredoka_bold, FontWeight.Bold), Font(R.font.fredoka_medium, FontWeight.Medium))
val Nunito = FontFamily(
    Font(R.font.nunito_400, FontWeight.Normal), Font(R.font.nunito_600, FontWeight.SemiBold), Font(R.font.nunito_700, FontWeight.Bold),
)

fun VoiceType.color(): Color = when (this) {
    VoiceType.ORIGINAL -> Vx.Cream
    VoiceType.FEMME -> Vx.Magenta
    VoiceType.HOMME -> Color(0xFF4C9BFF)
    VoiceType.ENFANT -> Color(0xFFFFB547)
    VoiceType.AGE -> Color(0xFFB48CFF)
    VoiceType.ROBOT -> Vx.Ok
}

fun VoiceType.emoji(): String = when (this) {
    VoiceType.ORIGINAL -> "🎙️"; VoiceType.FEMME -> "👩"; VoiceType.HOMME -> "👨"
    VoiceType.ENFANT -> "🧒"; VoiceType.AGE -> "👴"; VoiceType.ROBOT -> "🤖"
}

@Composable
fun VoxeaTheme(content: @Composable () -> Unit) {
    val base = Typography()
    val t = Typography(
        headlineLarge = TextStyle(fontFamily = Fredoka, fontWeight = FontWeight.Bold, fontSize = 30.sp),
        headlineMedium = TextStyle(fontFamily = Fredoka, fontWeight = FontWeight.Bold, fontSize = 24.sp),
        titleLarge = TextStyle(fontFamily = Fredoka, fontWeight = FontWeight.Medium, fontSize = 20.sp),
        titleMedium = TextStyle(fontFamily = Nunito, fontWeight = FontWeight.Bold, fontSize = 16.sp),
        bodyLarge = TextStyle(fontFamily = Nunito, fontSize = 16.sp, lineHeight = 22.sp),
        bodyMedium = TextStyle(fontFamily = Nunito, fontSize = 14.sp, lineHeight = 20.sp),
        bodySmall = TextStyle(fontFamily = Nunito, fontSize = 12.sp),
        labelLarge = TextStyle(fontFamily = Nunito, fontWeight = FontWeight.Bold, fontSize = 15.sp),
        labelMedium = base.labelMedium.copy(fontFamily = Nunito, fontWeight = FontWeight.SemiBold),
    )
    MaterialTheme(
        colorScheme = darkColorScheme(
            primary = Vx.Magenta, onPrimary = Color.White, secondary = Vx.Cyan, onSecondary = Vx.Bg,
            background = Vx.Bg, onBackground = Vx.Cream, surface = Vx.Surface, onSurface = Vx.Cream,
            surfaceVariant = Vx.Surface2, onSurfaceVariant = Vx.Muted, outline = Vx.Line, error = Vx.Rec,
            surfaceContainer = Vx.Surface, surfaceContainerHigh = Vx.Surface2, surfaceContainerHighest = Vx.Surface2,
        ),
        typography = t,
    ) {
        // Couleur de texte par défaut : crème (sinon noir hors Surface)
        androidx.compose.runtime.CompositionLocalProvider(androidx.compose.material3.LocalContentColor provides Vx.Cream, content = content)
    }
}
