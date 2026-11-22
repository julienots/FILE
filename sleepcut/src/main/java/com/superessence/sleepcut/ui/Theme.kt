package com.superessence.sleepcut.ui

import androidx.compose.material3.LocalContentColor
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Typography
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.sp

object S {
    val bg = Color(0xFF0A0A1F)
    val surface = Color(0xFF17173A)
    val surface2 = Color(0xFF252552)
    val line = Color(0xFF2E2E5E)
    val text = Color(0xFFF1EEFF)
    val dim = Color(0xFFA19FC9)
    val accent = Color(0xFF9D8CFF)
    val moon = Color(0xFFFFD27A)
    val pink = Color(0xFFFF7AA8)
    val good = Color(0xFF4BE3A0)
}

@Composable
fun SleepTheme(content: @Composable () -> Unit) = MaterialTheme(
    colorScheme = darkColorScheme(primary = S.accent, background = S.bg, surface = S.surface, onSurface = S.text, onBackground = S.text),
    typography = Typography(headlineSmall = TextStyle(fontWeight = FontWeight.Black, fontSize = 26.sp)),
) { CompositionLocalProvider(LocalContentColor provides S.text, content = content) }
