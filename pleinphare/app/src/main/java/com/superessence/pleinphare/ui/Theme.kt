package com.superessence.pleinphare.ui

import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.Font
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import com.superessence.pleinphare.R

val SuperYellow = Color(0xFFFFC72C)
val Ink = Color(0xFF000000)
val Panel = Color(0xFF121214)
val PanelHi = Color(0xFF1D1D21)
val TextHi = Color(0xFFF7F7F7)
val TextMid = Color(0xFFB4B4BA)
val TextLow = Color(0xFF7A7A82)

val Fredoka = FontFamily(
    Font(R.font.fredoka_bold, FontWeight.Bold),
    Font(R.font.fredoka_medium, FontWeight.Medium),
)
val Nunito = FontFamily(
    Font(R.font.nunito_400, FontWeight.Normal),
    Font(R.font.nunito_600, FontWeight.SemiBold),
    Font(R.font.nunito_700, FontWeight.Bold),
)

@Composable
fun PleinPhareTheme(content: @Composable () -> Unit) {
    MaterialTheme(
        colorScheme = darkColorScheme(
            primary = SuperYellow, onPrimary = Ink,
            background = Ink, onBackground = TextHi,
            surface = Panel, onSurface = TextHi,
            surfaceVariant = PanelHi, onSurfaceVariant = TextMid,
        ),
        content = content,
    )
}
