package com.superessence.focus.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxScope
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.Font
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.TextUnit
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.superessence.focus.R

object C {
    val Bg = Color(0xFF0B0B0F)
    val Surface = Color(0xFF16161C)
    val Surface2 = Color(0xFF22222B)
    val Line = Color(0xFF2E2E39)
    val Yellow = Color(0xFFFFC928)
    val YellowDeep = Color(0xFFFFA81F)
    val Text = Color(0xFFF6F6F8)
    val Muted = Color(0xFF9A9AA8)
    val Green = Color(0xFF6EE7A0)
    val Red = Color(0xFFFF6B7A)
}

val Fredoka = FontFamily(Font(R.font.fredoka_bold, FontWeight.Bold), Font(R.font.fredoka_medium, FontWeight.Medium))
val Nunito = FontFamily(
    Font(R.font.nunito_400, FontWeight.Normal),
    Font(R.font.nunito_600, FontWeight.SemiBold),
    Font(R.font.nunito_700, FontWeight.Bold),
)

@Composable
fun FocusTheme(content: @Composable () -> Unit) {
    MaterialTheme(
        colorScheme = darkColorScheme(
            primary = C.Yellow, onPrimary = Color.Black, background = C.Bg, surface = C.Surface,
            onSurface = C.Text, onBackground = C.Text, surfaceVariant = C.Surface2, secondaryContainer = C.Surface2,
            onSecondaryContainer = C.Yellow, outline = C.Line,
        ),
        typography = MaterialTheme.typography.copy(
            bodyLarge = TextStyle(fontFamily = Nunito, fontSize = 16.sp),
            bodyMedium = TextStyle(fontFamily = Nunito, fontSize = 14.sp),
            labelLarge = TextStyle(fontFamily = Nunito, fontWeight = FontWeight.Bold, fontSize = 15.sp),
            labelMedium = TextStyle(fontFamily = Nunito, fontWeight = FontWeight.SemiBold, fontSize = 12.sp),
        ),
        content = content,
    )
}

@Composable
fun Title(text: String, size: TextUnit = 28.sp, color: Color = C.Text, modifier: Modifier = Modifier) =
    Text(text, modifier, color = color, fontFamily = Fredoka, fontWeight = FontWeight.Bold, fontSize = size)

@Composable
fun Body(text: String, color: Color = C.Muted, size: TextUnit = 14.sp, weight: FontWeight = FontWeight.Normal, modifier: Modifier = Modifier) =
    Text(text, modifier, color = color, fontFamily = Nunito, fontSize = size, fontWeight = weight, lineHeight = size * 1.35f)

@Composable
fun Panel(modifier: Modifier = Modifier, color: Color = C.Surface, content: @Composable ColumnScope.() -> Unit) =
    Column(modifier.clip(RoundedCornerShape(24.dp)).background(color).padding(18.dp), content = content)

@Composable
fun Pill(modifier: Modifier = Modifier, color: Color = C.Surface2, content: @Composable BoxScope.() -> Unit) =
    Box(modifier.clip(RoundedCornerShape(50)).background(color).padding(horizontal = 14.dp, vertical = 8.dp), content = content)
