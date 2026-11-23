package com.superessence.egide.ui

import android.Manifest
import android.app.Activity
import android.content.Intent
import android.content.pm.PackageManager
import android.net.VpnService
import android.os.Build
import android.os.Bundle
import android.provider.Settings
import android.widget.Toast
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Home
import androidx.compose.material.icons.filled.Lock
import androidx.compose.material.icons.filled.Menu
import androidx.compose.material.icons.filled.Settings
import androidx.compose.material3.Icon
import androidx.compose.material3.NavigationBar
import androidx.compose.material3.NavigationBarItem
import androidx.compose.material3.NavigationBarItemDefaults
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.core.content.ContextCompat
import com.superessence.egide.browser.BrowserActivity
import com.superessence.egide.core.Stats
import com.superessence.egide.vpn.EgideVpnService
import kotlinx.coroutines.delay

class MainActivity : ComponentActivity() {

    private val vpnConsent = registerForActivityResult(ActivityResultContracts.StartActivityForResult()) { r ->
        if (r.resultCode == Activity.RESULT_OK) EgideVpnService.start(this)
        else Toast.makeText(this, "Autorisation VPN refusée : la protection ne peut pas démarrer.", Toast.LENGTH_LONG).show()
    }
    private val notifPerm = registerForActivityResult(ActivityResultContracts.RequestPermission()) { askVpn() }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        setContent {
            EgideTheme {
                LaunchedEffect(Unit) { while (true) { Stats.publishNow(); delay(1000) } }
                Root(
                    onToggle = ::toggle,
                    openBrowser = { yt -> startActivity(BrowserActivity.intent(this, yt)) },
                )
            }
        }
    }

    fun toggle() {
        if (EgideVpnService.state.value != EgideVpnService.State.OFF) {
            EgideVpnService.stop(this)
            return
        }
        if (Build.VERSION.SDK_INT >= 33 &&
            ContextCompat.checkSelfPermission(this, Manifest.permission.POST_NOTIFICATIONS) != PackageManager.PERMISSION_GRANTED
        ) {
            notifPerm.launch(Manifest.permission.POST_NOTIFICATIONS)
        } else askVpn()
    }

    private fun askVpn() {
        val consent = VpnService.prepare(this)
        if (consent != null) vpnConsent.launch(consent) else EgideVpnService.start(this)
    }

    fun openSystem(action: String) {
        runCatching { startActivity(Intent(action)) }
            .onFailure { startActivity(Intent(Settings.ACTION_SETTINGS)) }
    }
}

private data class Tab(val label: String, val icon: ImageVector)

@Composable
private fun Root(onToggle: () -> Unit, openBrowser: (Boolean) -> Unit) {
    val tabs = remember {
        listOf(
            Tab("Accueil", Icons.Filled.Home),
            Tab("Journal", Icons.Filled.Menu),
            Tab("Listes", Icons.Filled.Lock),
            Tab("Réglages", Icons.Filled.Settings),
        )
    }
    var current by rememberSaveable { mutableIntStateOf(0) }
    Scaffold(
        containerColor = C.Ink,
        bottomBar = {
            NavigationBar(containerColor = C.Surface, tonalElevation = androidx.compose.ui.unit.Dp(0f)) {
                tabs.forEachIndexed { i, t ->
                    NavigationBarItem(
                        selected = current == i,
                        onClick = { current = i },
                        icon = { Icon(t.icon, contentDescription = t.label) },
                        label = { Text(t.label) },
                        colors = NavigationBarItemDefaults.colors(
                            selectedIconColor = C.Text,
                            selectedTextColor = C.Text,
                            indicatorColor = C.Violet.copy(alpha = 0.35f),
                            unselectedIconColor = C.Muted,
                            unselectedTextColor = C.Muted,
                        ),
                    )
                }
            }
        },
    ) { pad ->
        Box(Modifier.fillMaxSize().padding(pad)) {
            when (current) {
                0 -> HomeScreen(onToggle, openBrowser)
                1 -> JournalScreen()
                2 -> ListsScreen()
                else -> SettingsScreen()
            }
        }
    }
}
