package com.superessence.promzy.ui

import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.Font
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import com.superessence.promzy.R

val Ink = Color(0xFF0B0B0D)
val Card = Color(0xFF17171B)
val CardHi = Color(0xFF222228)
val Line = Color(0xFF2C2C33)
val Yellow = Color(0xFFFFC928)
val Hot = Color(0xFFFF5A36)
val Live = Color(0xFFFF3B4E)
val TextHi = Color(0xFFF7F7F8)
val TextMid = Color(0xFFB4B4BD)
val TextLow = Color(0xFF7D7D88)

val Fredoka = FontFamily(Font(R.font.fredoka_bold, FontWeight.Bold), Font(R.font.fredoka_medium, FontWeight.Medium))
val Nunito = FontFamily(
    Font(R.font.nunito_400, FontWeight.Normal), Font(R.font.nunito_600, FontWeight.SemiBold), Font(R.font.nunito_700, FontWeight.Bold),
)

@Composable
fun PromzyTheme(content: @Composable () -> Unit) {
    MaterialTheme(
        colorScheme = darkColorScheme(
            primary = Yellow, onPrimary = Ink, background = Ink, surface = Card, onSurface = TextHi, onBackground = TextHi,
            surfaceVariant = CardHi, onSurfaceVariant = TextMid, outline = Line,
        ),
        content = content,
    )
}
