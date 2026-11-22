package com.superessence.pulsar

import android.Manifest
import android.os.Build
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.SystemBarStyle
import androidx.activity.compose.BackHandler
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.animation.AnimatedContent
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.slideInVertically
import androidx.compose.animation.togetherWith
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.superessence.pulsar.player.Player
import com.superessence.pulsar.ui.HomeScreen
import com.superessence.pulsar.ui.PulsarTheme
import com.superessence.pulsar.ui.StudioScreen

class MainActivity : ComponentActivity() {
    private val notifPermission = registerForActivityResult(ActivityResultContracts.RequestPermission()) { }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge(statusBarStyle = SystemBarStyle.dark(0x00000000), navigationBarStyle = SystemBarStyle.dark(0x00000000))
        Player.init(this)
        if (Build.VERSION.SDK_INT >= 33 && !Player.store.onboarded) {
            notifPermission.launch(Manifest.permission.POST_NOTIFICATIONS)
            Player.store.onboarded = true
        }
        setContent {
            PulsarTheme {
                val st by Player.state.collectAsStateWithLifecycle()
                var studio by rememberSaveable { mutableStateOf(false) }
                BackHandler(studio) { studio = false }
                AnimatedContent(studio, transitionSpec = { (fadeIn() + slideInVertically { it / 12 }) togetherWith fadeOut() }, label = "nav") { s ->
                    if (s && st.song != null) StudioScreen(st) { studio = false }
                    else HomeScreen(st) { studio = true }
                }
            }
        }
    }
}
