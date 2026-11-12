package com.superessence.essabank.ui.theme

import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.material3.lightColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.Immutable
import androidx.compose.runtime.staticCompositionLocalOf
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.Font
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.superessence.essabank.R
import com.superessence.essabank.core.model.Accent
import com.superessence.essabank.core.model.ThemeMode

/*
 * Design system ESSABANK : bleu nuit, blanc et bleu électrique.
 * Toutes les couleurs de l'interface passent par [EssaColors] (jamais de couleur codée en dur dans un écran).
 */

object Brand {
    val Night = Color(0xFF060A1C)
    val Night2 = Color(0xFF0A1130)
    val Navy = Color(0xFF101A45)
    val Electric = Color(0xFF2F6BFF)
    val ElectricLight = Color(0xFF6E9BFF)
    val Cyan = Color(0xFF3DD6FF)
    val White = Color(0xFFFFFFFF)
    val Success = Color(0xFF16B47E)
    val Danger = Color(0xFFF0475C)
    val Warning = Color(0xFFF5A524)
    val Gold = Color(0xFFD4B26A)

    /** Palette des comptes, coffres et bénéficiaires. */
    val palette = listOf(
        Color(0xFF2F6BFF), Color(0xFF12B886), Color(0xFF7B5CFF), Color(0xFFFF8A3D),
        Color(0xFF00B8D9), Color(0xFFE64980), Color(0xFFF5A524), Color(0xFF5C6B8A),
    )

    fun color(index: Int): Color = palette[((index % palette.size) + palette.size) % palette.size]

    fun accent(a: Accent): Color = when (a) {
        Accent.ELECTRIC -> Electric
        Accent.AZURE -> Color(0xFF00A3FF)
        Accent.VIOLET -> Color(0xFF7B5CFF)
        Accent.EMERALD -> Color(0xFF12B886)
        Accent.GOLD -> Color(0xFFC9A45C)
    }
}

@Immutable
data class EssaColors(
    val isDark: Boolean,
    val isOnyx: Boolean,
    val background: Color,
    val surface: Color,
    val surfaceAlt: Color,
    val elevated: Color,
    val border: Color,
    val text: Color,
    val textSecondary: Color,
    val textMuted: Color,
    val accent: Color,
    val accentSoft: Color,
    val onAccent: Color,
    val positive: Color,
    val negative: Color,
    val warning: Color,
    val heroStart: Color,
    val heroEnd: Color,
    val chartGrid: Color,
) {
    val hero: Brush get() = Brush.linearGradient(listOf(heroStart, heroEnd))
}

private fun lightColors(accent: Color) = EssaColors(
    isDark = false, isOnyx = false,
    background = Color(0xFFF3F5FB), surface = Color.White, surfaceAlt = Color(0xFFEDF1FA), elevated = Color.White,
    border = Color(0xFFE1E6F2), text = Color(0xFF0A1230), textSecondary = Color(0xFF4A5578), textMuted = Color(0xFF8A93AE),
    accent = accent, accentSoft = accent.copy(alpha = 0.10f), onAccent = Color.White,
    positive = Color(0xFF0E9F6E), negative = Color(0xFFE23D52), warning = Color(0xFFD98A00),
    heroStart = Color(0xFF0B1440), heroEnd = Color(0xFF1C3AA8), chartGrid = Color(0xFFE6EAF4),
)

private fun darkColors(accent: Color) = EssaColors(
    isDark = true, isOnyx = false,
    background = Brand.Night, surface = Color(0xFF0D1433), surfaceAlt = Color(0xFF121B42), elevated = Color(0xFF16204D),
    border = Color(0xFF1E2A5C), text = Color(0xFFF2F5FF), textSecondary = Color(0xFFB4BEDF), textMuted = Color(0xFF7581A8),
    accent = accent, accentSoft = accent.copy(alpha = 0.16f), onAccent = Color.White,
    positive = Color(0xFF2FD39A), negative = Color(0xFFFF6175), warning = Color(0xFFFFB648),
    heroStart = Color(0xFF14246E), heroEnd = Color(0xFF2F6BFF), chartGrid = Color(0xFF1A2550),
)

/** Interface exclusive ESSA BLACK : noir profond, reflets champagne. */
private fun onyxColors(accent: Color) = EssaColors(
    isDark = true, isOnyx = true,
    background = Color(0xFF000000), surface = Color(0xFF0C0C0E), surfaceAlt = Color(0xFF141417), elevated = Color(0xFF18181C),
    border = Color(0xFF26262B), text = Color(0xFFF5F3EE), textSecondary = Color(0xFFBDB8AC), textMuted = Color(0xFF7D786E),
    accent = accent, accentSoft = accent.copy(alpha = 0.15f), onAccent = Color(0xFF0B0B0C),
    positive = Color(0xFF3CCB8F), negative = Color(0xFFFF6B6B), warning = Color(0xFFE8B04B),
    heroStart = Color(0xFF16161A), heroEnd = Color(0xFF3A3326), chartGrid = Color(0xFF1E1E22),
)

val LocalEssaColors = staticCompositionLocalOf { darkColors(Brand.Electric) }

object EssaFonts {
    val display = FontFamily(
        Font(R.font.manrope_500, FontWeight.Medium),
        Font(R.font.manrope_600, FontWeight.SemiBold),
        Font(R.font.manrope_700, FontWeight.Bold),
        Font(R.font.manrope_800, FontWeight.ExtraBold),
    )
    val body = FontFamily(
        Font(R.font.inter_400, FontWeight.Normal),
        Font(R.font.inter_500, FontWeight.Medium),
        Font(R.font.inter_600, FontWeight.SemiBold),
        Font(R.font.inter_700, FontWeight.Bold),
    )
}

/** Échelle typographique. Les chiffres utilisent des numéraux tabulaires (alignement des montants). */
object EssaType {
    val hero = TextStyle(fontFamily = EssaFonts.display, fontWeight = FontWeight.ExtraBold, fontSize = 38.sp, letterSpacing = (-1).sp, fontFeatureSettings = "tnum")
    val h1 = TextStyle(fontFamily = EssaFonts.display, fontWeight = FontWeight.ExtraBold, fontSize = 28.sp, letterSpacing = (-0.6).sp)
    val h2 = TextStyle(fontFamily = EssaFonts.display, fontWeight = FontWeight.Bold, fontSize = 21.sp, letterSpacing = (-0.3).sp)
    val h3 = TextStyle(fontFamily = EssaFonts.display, fontWeight = FontWeight.Bold, fontSize = 17.sp, letterSpacing = (-0.1).sp)
    val title = TextStyle(fontFamily = EssaFonts.body, fontWeight = FontWeight.SemiBold, fontSize = 15.sp)
    val body = TextStyle(fontFamily = EssaFonts.body, fontWeight = FontWeight.Normal, fontSize = 14.sp, lineHeight = 20.sp)
    val bodyStrong = TextStyle(fontFamily = EssaFonts.body, fontWeight = FontWeight.SemiBold, fontSize = 14.sp)
    val small = TextStyle(fontFamily = EssaFonts.body, fontWeight = FontWeight.Medium, fontSize = 12.sp, lineHeight = 16.sp)
    val caption = TextStyle(fontFamily = EssaFonts.body, fontWeight = FontWeight.SemiBold, fontSize = 11.sp, letterSpacing = 0.6.sp)
    val amount = TextStyle(fontFamily = EssaFonts.body, fontWeight = FontWeight.SemiBold, fontSize = 15.sp, fontFeatureSettings = "tnum")
    val amountLarge = TextStyle(fontFamily = EssaFonts.display, fontWeight = FontWeight.ExtraBold, fontSize = 30.sp, letterSpacing = (-0.6).sp, fontFeatureSettings = "tnum")
    val mono = TextStyle(fontFamily = EssaFonts.body, fontWeight = FontWeight.Medium, fontSize = 17.sp, letterSpacing = 2.sp, fontFeatureSettings = "tnum")
}

object Dimens {
    val screen = 20.dp
    val gap = 12.dp
    val radius = 20.dp
    val radiusSmall = 14.dp
}

object Essa {
    val colors: EssaColors @Composable get() = LocalEssaColors.current
}

@Composable
fun EssaTheme(mode: ThemeMode, accent: Accent, content: @Composable () -> Unit) {
    val systemDark = isSystemInDarkTheme()
    val accentColor = if (mode == ThemeMode.ONYX && accent == Accent.ELECTRIC) Brand.Gold else Brand.accent(accent)
    val colors = when (mode) {
        ThemeMode.LIGHT -> lightColors(accentColor)
        ThemeMode.DARK -> darkColors(accentColor)
        ThemeMode.ONYX -> onyxColors(accentColor)
        ThemeMode.SYSTEM -> if (systemDark) darkColors(accentColor) else lightColors(accentColor)
    }
    val scheme = if (colors.isDark) darkColorScheme(
        primary = colors.accent, onPrimary = colors.onAccent, background = colors.background, onBackground = colors.text,
        surface = colors.surface, onSurface = colors.text, surfaceVariant = colors.surfaceAlt, onSurfaceVariant = colors.textSecondary,
        outline = colors.border, error = colors.negative, secondary = colors.accent, surfaceContainer = colors.surface,
        surfaceContainerHigh = colors.elevated, surfaceContainerHighest = colors.elevated, surfaceContainerLow = colors.surface,
    ) else lightColorScheme(
        primary = colors.accent, onPrimary = colors.onAccent, background = colors.background, onBackground = colors.text,
        surface = colors.surface, onSurface = colors.text, surfaceVariant = colors.surfaceAlt, onSurfaceVariant = colors.textSecondary,
        outline = colors.border, error = colors.negative, secondary = colors.accent, surfaceContainer = colors.surface,
        surfaceContainerHigh = colors.elevated, surfaceContainerHighest = colors.elevated, surfaceContainerLow = colors.surface,
    )
    CompositionLocalProvider(LocalEssaColors provides colors) {
        MaterialTheme(colorScheme = scheme, content = content)
    }
}
