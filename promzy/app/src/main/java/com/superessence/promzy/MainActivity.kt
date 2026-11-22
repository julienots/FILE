package com.superessence.promzy

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.viewModels
import androidx.compose.animation.Crossfade
import androidx.compose.animation.core.tween
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.runtime.DisposableEffect
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleEventObserver
import androidx.lifecycle.compose.LocalLifecycleOwner
import com.superessence.promzy.ui.MainScreen
import com.superessence.promzy.ui.PromzyTheme
import com.superessence.promzy.ui.SplashScreen

class MainActivity : ComponentActivity() {
    private val vm: PromzyViewModel by viewModels()

    override fun onCreate(savedInstanceState: Bundle?) {
        enableEdgeToEdge()
        super.onCreate(savedInstanceState)
        setContent {
            PromzyTheme {
                var splash by rememberSaveable { mutableStateOf(true) }
                val owner = LocalLifecycleOwner.current
                DisposableEffect(owner) {
                    val obs = LifecycleEventObserver { _, e ->
                        if (e == Lifecycle.Event.ON_START) vm.startLive()
                        if (e == Lifecycle.Event.ON_STOP) vm.stopLive()
                    }
                    owner.lifecycle.addObserver(obs)
                    onDispose { owner.lifecycle.removeObserver(obs) }
                }
                Crossfade(splash, animationSpec = tween(500), label = "root") { s ->
                    if (s) SplashScreen { splash = false } else MainScreen(vm)
                }
            }
        }
    }
}
