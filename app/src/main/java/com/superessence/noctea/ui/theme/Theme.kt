package com.superessence.noctea.ui.theme

import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Typography
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.Immutable
import androidx.compose.runtime.staticCompositionLocalOf
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.Font
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontStyle
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.em
import androidx.compose.ui.unit.sp
import com.superessence.noctea.R

/** Palette d'un thème NOCTÉA. */
@Immutable
data class NightPalette(
    val name: String,
    val label: String,
    val sky0: Color,      // haut du ciel
    val sky1: Color,      // milieu
    val sky2: Color,      // bas
    val glow: Color,      // halo diffus
    val accent: Color,    // lavande
    val moon: Color,      // or lunaire
    val text: Color,
    val textDim: Color,
    val card: Color,
    val cardBorder: Color,
)

object Palettes {
    val Nuit = NightPalette(
        "nuit", "Nuit profonde",
        Color(0xFF1A1C48), Color(0xFF0D1030), Color(0xFF07081A),
        Color(0xFF6C5CE7), Color(0xFFB9A8FF), Color(0xFFF4D58D),
        Color(0xFFEEF0FF), Color(0xFF9EA3D6), Color(0x1AFFFFFF), Color(0x22FFFFFF),
    )
    val Aurore = NightPalette(
        "aurore", "Aurore violette",
        Color(0xFF2E1A4F), Color(0xFF170E30), Color(0xFF0A0618),
        Color(0xFFB45FD8), Color(0xFFE2B6FF), Color(0xFFFFD6A0),
        Color(0xFFF5EEFF), Color(0xFFB8A3D6), Color(0x1AFFFFFF), Color(0x22FFFFFF),
    )
    val Ocean = NightPalette(
        "ocean", "Océan nocturne",
        Color(0xFF0F2A48), Color(0xFF081A30), Color(0xFF040C1A),
        Color(0xFF2E86C1), Color(0xFF9FD3FF), Color(0xFFE8E2C0),
        Color(0xFFEAF5FF), Color(0xFF93AFCB), Color(0x1AFFFFFF), Color(0x22FFFFFF),
    )
    /** Variante appliquée automatiquement en pleine nuit : moins de bleu, luminosité réduite. */
    fun deepNight(p: NightPalette) = p.copy(
        sky0 = Color(0xFF120E1C), sky1 = Color(0xFF0A0812), sky2 = Color(0xFF050409),
        glow = Color(0xFF6B3A3A), accent = Color(0xFFD9A6A0), moon = Color(0xFFE8B98A),
        text = Color(0xFFD8CFC8), textDim = Color(0xFF8E8078),
    )
    val all = listOf(Nuit, Aurore, Ocean)
    fun byName(n: String) = all.firstOrNull { it.name == n } ?: Nuit
}

val SuperYellow = Color(0xFFFFCC1F)

val LocalPalette = staticCompositionLocalOf { Palettes.Nuit }

val Cormorant = FontFamily(
    Font(R.font.cormorant_300, FontWeight.Light),
    Font(R.font.cormorant_500, FontWeight.Medium),
    Font(R.font.cormorant_600, FontWeight.SemiBold),
    Font(R.font.cormorant_italic, FontWeight.Normal, FontStyle.Italic),
)
val Nunito = FontFamily(
    Font(R.font.nunito_300, FontWeight.Light),
    Font(R.font.nunito_400, FontWeight.Normal),
    Font(R.font.nunito_600, FontWeight.SemiBold),
    Font(R.font.nunito_700, FontWeight.Bold),
)
val Fredoka = FontFamily(
    Font(R.font.fredoka_medium, FontWeight.Medium),
    Font(R.font.fredoka_bold, FontWeight.Bold),
)

private fun typography(p: NightPalette) = Typography(
    displayLarge = TextStyle(fontFamily = Cormorant, fontWeight = FontWeight.Light, fontSize = 46.sp, letterSpacing = 0.02.em, color = p.text),
    displayMedium = TextStyle(fontFamily = Cormorant, fontWeight = FontWeight.Medium, fontSize = 36.sp, color = p.text),
    headlineLarge = TextStyle(fontFamily = Cormorant, fontWeight = FontWeight.Medium, fontSize = 32.sp, color = p.text),
    headlineMedium = TextStyle(fontFamily = Cormorant, fontWeight = FontWeight.SemiBold, fontSize = 27.sp, color = p.text),
    headlineSmall = TextStyle(fontFamily = Cormorant, fontWeight = FontWeight.SemiBold, fontSize = 23.sp, color = p.text),
    titleLarge = TextStyle(fontFamily = Nunito, fontWeight = FontWeight.SemiBold, fontSize = 19.sp, color = p.text),
    titleMedium = TextStyle(fontFamily = Nunito, fontWeight = FontWeight.SemiBold, fontSize = 16.sp, color = p.text),
    titleSmall = TextStyle(fontFamily = Nunito, fontWeight = FontWeight.SemiBold, fontSize = 14.sp, color = p.text),
    bodyLarge = TextStyle(fontFamily = Nunito, fontWeight = FontWeight.Normal, fontSize = 16.sp, lineHeight = 24.sp, color = p.text),
    bodyMedium = TextStyle(fontFamily = Nunito, fontWeight = FontWeight.Normal, fontSize = 14.sp, lineHeight = 21.sp, color = p.textDim),
    bodySmall = TextStyle(fontFamily = Nunito, fontWeight = FontWeight.Normal, fontSize = 12.sp, color = p.textDim),
    labelLarge = TextStyle(fontFamily = Nunito, fontWeight = FontWeight.Bold, fontSize = 15.sp, letterSpacing = 0.03.em),
    labelMedium = TextStyle(fontFamily = Nunito, fontWeight = FontWeight.SemiBold, fontSize = 12.sp, letterSpacing = 0.05.em),
    labelSmall = TextStyle(fontFamily = Nunito, fontWeight = FontWeight.SemiBold, fontSize = 11.sp, letterSpacing = 0.08.em),
)

@Composable
fun NocteaTheme(palette: NightPalette, content: @Composable () -> Unit) {
    val scheme = darkColorScheme(
        primary = palette.accent,
        onPrimary = Color(0xFF14102E),
        secondary = palette.moon,
        onSecondary = Color(0xFF231A05),
        background = palette.sky2,
        onBackground = palette.text,
        surface = palette.sky1,
        onSurface = palette.text,
        surfaceVariant = palette.sky0,
        onSurfaceVariant = palette.textDim,
        outline = palette.cardBorder,
    )
    CompositionLocalProvider(LocalPalette provides palette) {
        MaterialTheme(colorScheme = scheme, typography = typography(palette), content = content)
    }
}
