package com.superessence.bodymotion

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.ui.Modifier
import com.superessence.bodymotion.ui.AppNav
import com.superessence.bodymotion.ui.theme.BmTheme
import com.superessence.bodymotion.ui.theme.C

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        setContent {
            BmTheme { Box(Modifier.fillMaxSize().background(C.Bg)) { AppNav() } }
        }
    }
}
