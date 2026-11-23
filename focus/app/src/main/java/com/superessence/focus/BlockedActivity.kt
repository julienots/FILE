package com.superessence.focus

import android.content.Intent
import android.net.Uri
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.BackHandler
import androidx.activity.compose.setContent
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.superessence.focus.core.Apps
import com.superessence.focus.core.Session
import com.superessence.focus.core.Store
import com.superessence.focus.ui.Body
import com.superessence.focus.ui.C
import com.superessence.focus.ui.FocusMark
import com.superessence.focus.ui.FocusTheme
import com.superessence.focus.ui.Fredoka
import com.superessence.focus.ui.Title
import com.superessence.focus.ui.fmt
import com.superessence.focus.ui.rememberNow

/** Écran affiché quand on ouvre une appli bloquée pendant la session. */
class BlockedActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        Store.init(this)
        setContent {
            FocusTheme {
                val now = rememberNow()
                LaunchedEffect(now) { if (!Session.blocking) finish() }
                BackHandler { openFocus(0) }
                Column(
                    Modifier.fillMaxSize().background(Color.Black).padding(28.dp),
                    horizontalAlignment = Alignment.CenterHorizontally, verticalArrangement = Arrangement.Center,
                ) {
                    FocusMark(120.dp, glow = 1f)
                    Spacer(Modifier.height(24.dp))
                    Title("Pas maintenant 😌", 30.sp)
                    Spacer(Modifier.height(8.dp))
                    Body("Cette appli est bloquée pendant ta session de révision.", color = C.Text, size = 16.sp, modifier = Modifier.fillMaxWidth())
                    Spacer(Modifier.height(18.dp))
                    Text(fmt(Session.remainingMs(now)), color = C.Yellow, fontFamily = Fredoka, fontWeight = FontWeight.Bold, fontSize = 54.sp)
                    Body("avant la fin", size = 13.sp)
                    Spacer(Modifier.height(30.dp))
                    Big("Retour à mes révisions", true) { openFocus(0) }
                    Spacer(Modifier.height(10.dp))
                    Big("Écrire à ${Store.romaneName}", false) { openFocus(2) }
                    Spacer(Modifier.height(10.dp))
                    Big("Calculatrice", false) {
                        Apps.calculators(this@BlockedActivity).firstOrNull()?.let { Apps.launch(this@BlockedActivity, it); finish() }
                    }
                    Spacer(Modifier.height(16.dp))
                    TextButton({
                        Store.callGraceUntil = System.currentTimeMillis() + 120_000
                        runCatching { startActivity(Intent(Intent.ACTION_DIAL, Uri.parse("tel:112"))) }
                    }) { Text("Appel d'urgence (112)", color = C.Red, textAlign = TextAlign.Center) }
                }
            }
        }
    }

    private fun openFocus(tab: Int) {
        startActivity(Intent(this, MainActivity::class.java).putExtra("tab", tab).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK))
        finish()
    }

    @androidx.compose.runtime.Composable
    private fun Big(text: String, primary: Boolean, onClick: () -> Unit) {
        Button(
            onClick, Modifier.fillMaxWidth().height(54.dp), shape = RoundedCornerShape(18.dp),
            colors = ButtonDefaults.buttonColors(if (primary) C.Yellow else C.Surface2, if (primary) Color.Black else C.Text),
        ) { Text(text, fontFamily = Fredoka, fontWeight = FontWeight.Bold, fontSize = 17.sp) }
    }
}
