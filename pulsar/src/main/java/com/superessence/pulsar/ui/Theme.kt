package com.superessence.pulsar.ui

import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Typography
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.sp

object P {
    val bg = Color(0xFF07070D)
    val surface = Color(0xFF12121C)
    val surface2 = Color(0xFF1B1B29)
    val line = Color(0xFF2A2A3D)
    val text = Color(0xFFF2F0FF)
    val dim = Color(0xFF9C9AB8)
    val chill = Color(0xFF8E7CFF)
    val chill2 = Color(0xFF5CE1E6)
    val mad = Color(0xFFFF2E63)
    val mad2 = Color(0xFFFFB800)
    val good = Color(0xFF3DDC97)
}

private val scheme = darkColorScheme(
    primary = Color(0xFFB69CFF),
    onPrimary = Color(0xFF1A0B3D),
    secondary = Color(0xFFFF4F8B),
    background = P.bg,
    onBackground = P.text,
    surface = P.surface,
    onSurface = P.text,
    surfaceVariant = P.surface2,
    onSurfaceVariant = P.dim,
    outline = P.line,
)

private val type = Typography(
    displaySmall = TextStyle(fontFamily = FontFamily.SansSerif, fontWeight = FontWeight.Black, fontSize = 34.sp, letterSpacing = 6.sp),
    headlineSmall = TextStyle(fontFamily = FontFamily.SansSerif, fontWeight = FontWeight.ExtraBold, fontSize = 24.sp),
    titleLarge = TextStyle(fontFamily = FontFamily.SansSerif, fontWeight = FontWeight.Bold, fontSize = 20.sp),
    titleMedium = TextStyle(fontFamily = FontFamily.SansSerif, fontWeight = FontWeight.Bold, fontSize = 16.sp),
    bodyLarge = TextStyle(fontFamily = FontFamily.SansSerif, fontSize = 15.sp),
    bodyMedium = TextStyle(fontFamily = FontFamily.SansSerif, fontSize = 14.sp),
    labelLarge = TextStyle(fontFamily = FontFamily.SansSerif, fontWeight = FontWeight.Bold, fontSize = 14.sp, letterSpacing = 0.5.sp),
    labelSmall = TextStyle(fontFamily = FontFamily.SansSerif, fontWeight = FontWeight.SemiBold, fontSize = 11.sp, letterSpacing = 0.8.sp),
)

@Composable
fun PulsarTheme(content: @Composable () -> Unit) = MaterialTheme(colorScheme = scheme, typography = type) {
    androidx.compose.runtime.CompositionLocalProvider(androidx.compose.material3.LocalContentColor provides P.text, content = content)
}
