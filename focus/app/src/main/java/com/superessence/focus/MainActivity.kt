package com.superessence.focus

import android.Manifest
import android.content.Intent
import android.os.Build
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.SystemBarStyle
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.animation.Crossfade
import androidx.compose.animation.core.tween
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.Favorite
import androidx.compose.material.icons.rounded.Shield
import androidx.compose.material.icons.rounded.Style
import androidx.compose.material.icons.rounded.Timer
import androidx.compose.material3.Badge
import androidx.compose.material3.BadgedBox
import androidx.compose.material3.Icon
import androidx.compose.material3.NavigationBar
import androidx.compose.material3.NavigationBarItem
import androidx.compose.material3.NavigationBarItemDefaults
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import com.superessence.focus.core.Apps
import com.superessence.focus.core.BlockerService
import com.superessence.focus.core.Session
import com.superessence.focus.core.Store
import com.superessence.focus.ui.BlockScreen
import com.superessence.focus.ui.C
import com.superessence.focus.ui.CardsScreen
import com.superessence.focus.ui.FocusTheme
import com.superessence.focus.ui.HomeScreen
import com.superessence.focus.ui.RomaneScreen
import com.superessence.focus.ui.Splash

class MainActivity : ComponentActivity() {
    private val tab = mutableIntStateOf(0)
    private var showSplash = mutableStateOf(true)
    private val notifPerm = registerForActivityResult(ActivityResultContracts.RequestPermission()) {}

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge(SystemBarStyle.dark(Color.Black.hashCode()), SystemBarStyle.dark(0xFF0B0B0F.toInt()))
        Store.init(this)
        Apps.initDefaults(this)
        if (savedInstanceState != null) showSplash.value = false
        handle(intent)
        if (Build.VERSION.SDK_INT >= 33) notifPerm.launch(Manifest.permission.POST_NOTIFICATIONS)

        setContent {
            FocusTheme {
                var splash by showSplash
                Crossfade(splash, animationSpec = tween(600), label = "splash") { s ->
                    if (s) Splash { splash = false } else Main()
                }
            }
        }
    }

    override fun onNewIntent(intent: Intent) {
        super.onNewIntent(intent)
        handle(intent)
    }

    override fun onResume() {
        super.onResume()
        Session.check(this)
        Store.rev.intValue++
    }

    private fun handle(i: Intent?) {
        val t = i?.getIntExtra("tab", -1) ?: -1
        if (t >= 0) { tab.intValue = t; showSplash.value = false }
    }

    @androidx.compose.runtime.Composable
    private fun Main() {
        @Suppress("UNUSED_VARIABLE") val rev = Store.rev.intValue
        val needsSetup = !BlockerService.isEnabled(this)
        var current by rememberSaveable { tab }
        Scaffold(
            containerColor = C.Bg,
            bottomBar = {
                NavigationBar(containerColor = C.Surface) {
                    val items = listOf(
                        Triple("Réviser", Icons.Rounded.Timer, false),
                        Triple("Fiches", Icons.Rounded.Style, false),
                        Triple(Store.romaneName, Icons.Rounded.Favorite, false),
                        Triple("Blocage", Icons.Rounded.Shield, needsSetup),
                    )
                    items.forEachIndexed { i, (label, icon, badge) ->
                        NavigationBarItem(
                            selected = current == i, onClick = { current = i },
                            icon = {
                                BadgedBox(badge = { if (badge) Badge(containerColor = C.Yellow) }) { Icon(icon, label) }
                            },
                            label = { Text(label) },
                            colors = NavigationBarItemDefaults.colors(
                                selectedIconColor = Color.Black, indicatorColor = C.Yellow,
                                selectedTextColor = C.Yellow, unselectedIconColor = C.Muted, unselectedTextColor = C.Muted,
                            ),
                        )
                    }
                }
            },
        ) { pad ->
            Box(Modifier.fillMaxSize().background(C.Bg).padding(pad).statusBarsPadding()) {
                when (current) {
                    0 -> HomeScreen(onOpenTab = { current = it })
                    1 -> CardsScreen()
                    2 -> RomaneScreen()
                    else -> BlockScreen()
                }
            }
        }
    }
}
