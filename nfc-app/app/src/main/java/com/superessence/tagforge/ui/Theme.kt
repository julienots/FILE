package com.superessence.tagforge.ui

import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Typography
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color

val Teal = Color(0xFF00E5C3)
val Violet = Color(0xFF8C7BFF)
val Amber = Color(0xFFFFC857)
val Danger = Color(0xFFFF6B6B)
val Ok = Color(0xFF4ADE80)

private val scheme = darkColorScheme(
    primary = Teal,
    onPrimary = Color(0xFF00201A),
    primaryContainer = Color(0xFF003D33),
    onPrimaryContainer = Color(0xFFB3FFF0),
    secondary = Violet,
    onSecondary = Color(0xFF1A1050),
    secondaryContainer = Color(0xFF2B2366),
    onSecondaryContainer = Color(0xFFE2DDFF),
    tertiary = Amber,
    background = Color(0xFF0B1020),
    onBackground = Color(0xFFE6E9F5),
    surface = Color(0xFF131A33),
    onSurface = Color(0xFFE6E9F5),
    surfaceVariant = Color(0xFF1C2445),
    onSurfaceVariant = Color(0xFFAEB6D6),
    surfaceContainer = Color(0xFF161E3B),
    surfaceContainerHigh = Color(0xFF1C2445),
    surfaceContainerHighest = Color(0xFF232C52),
    outline = Color(0xFF3A4475),
    outlineVariant = Color(0xFF2A3360),
    error = Danger,
)

@Composable
fun TagForgeTheme(content: @Composable () -> Unit) {
    MaterialTheme(colorScheme = scheme, typography = Typography(), content = content)
}
