package com.superessence.egide.ui

import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color

object C {
    val Ink = Color(0xFF07070D)
    val Surface = Color(0xFF11111B)
    val Surface2 = Color(0xFF1A1A29)
    val Line = Color(0xFF26263A)
    val Text = Color(0xFFF4F4F8)
    val Muted = Color(0xFF9A9AB2)
    val Violet = Color(0xFF8B5CF6)
    val Lilac = Color(0xFFC4B5FD)
    val Cyan = Color(0xFF22D3EE)
    val Green = Color(0xFF34D399)
    val Red = Color(0xFFF87171)
    val Amber = Color(0xFFFBBF24)
}

@Composable
fun EgideTheme(content: @Composable () -> Unit) {
    MaterialTheme(
        colorScheme = darkColorScheme(
            primary = C.Violet,
            onPrimary = Color.White,
            secondary = C.Cyan,
            background = C.Ink,
            onBackground = C.Text,
            surface = C.Surface,
            onSurface = C.Text,
            surfaceVariant = C.Surface2,
            onSurfaceVariant = C.Muted,
            outline = C.Line,
            error = C.Red,
        ),
        content = content,
    )
}
