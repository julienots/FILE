package com.superessence.focus.ui

import android.Manifest
import android.app.Activity
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.database.ContentObserver
import android.net.Uri
import android.os.Handler
import android.os.Looper
import android.provider.ContactsContract
import android.widget.Toast
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.imePadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.rounded.Send
import androidx.compose.material.icons.rounded.Call
import androidx.compose.material.icons.rounded.Contacts
import androidx.compose.material.icons.rounded.Edit
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.core.content.ContextCompat
import com.superessence.focus.core.Msg
import com.superessence.focus.core.Session
import com.superessence.focus.core.Sms
import com.superessence.focus.core.Store
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

@Composable
fun RomaneScreen() {
    val ctx = LocalContext.current
    @Suppress("UNUSED_VARIABLE") val rev = Store.rev.intValue
    var permTick by remember { mutableIntStateOf(0) }
    var editing by remember { mutableStateOf(false) }
    val perms = rememberLauncherForActivityResult(ActivityResultContracts.RequestMultiplePermissions()) { permTick++ }
    @Suppress("UNUSED_EXPRESSION") permTick

    when {
        !Sms.granted(ctx) -> Column(Modifier.fillMaxSize().padding(16.dp), verticalArrangement = Arrangement.Center, horizontalAlignment = Alignment.CenterHorizontally) {
            Avatar(Store.romaneName, 90)
            Spacer(Modifier.height(16.dp))
            Title("Parler à ${Store.romaneName}", 24.sp)
            Spacer(Modifier.height(8.dp))
            Body(
                "Pendant tes sessions, toutes les messageries sont bloquées. Tu peux quand même écrire à ${Store.romaneName} ici, par SMS. " +
                    "Focus. a besoin de l'accès aux SMS pour afficher et envoyer vos messages (et seulement les vôtres).",
                modifier = Modifier.padding(horizontal = 8.dp),
            )
            Spacer(Modifier.height(20.dp))
            Button({ perms.launch(Sms.PERMS) }, shape = RoundedCornerShape(16.dp), colors = ButtonDefaults.buttonColors(C.Yellow, Color.Black)) {
                Text("Autoriser les SMS", fontFamily = Fredoka, fontWeight = FontWeight.Bold)
            }
        }
        Store.romaneNumber.isBlank() || editing -> Setup(onDone = { editing = false })
        else -> Chat(onEdit = {
            if (Session.blocking) Toast.makeText(ctx, "Impossible de changer de contact pendant une session", Toast.LENGTH_SHORT).show()
            else editing = true
        })
    }
}

@Composable
private fun Avatar(name: String, size: Int) {
    Box(
        Modifier.size(size.dp).clip(CircleShape).background(Brush.linearGradient(listOf(C.Yellow, C.YellowDeep))),
        contentAlignment = Alignment.Center,
    ) { Text(name.take(1).uppercase(), color = Color.Black, fontFamily = Fredoka, fontWeight = FontWeight.Bold, fontSize = (size * 0.45f).sp) }
}

@Composable
private fun Setup(onDone: () -> Unit) {
    val ctx = LocalContext.current
    var name by remember { mutableStateOf(Store.romaneName) }
    var number by remember { mutableStateOf(Store.romaneNumber) }
    val pick = rememberLauncherForActivityResult(ActivityResultContracts.StartActivityForResult()) { r ->
        val uri = r.data?.data
        if (r.resultCode == Activity.RESULT_OK && uri != null) {
            runCatching {
                ctx.contentResolver.query(
                    uri, arrayOf(ContactsContract.CommonDataKinds.Phone.NUMBER, ContactsContract.CommonDataKinds.Phone.DISPLAY_NAME), null, null, null,
                )?.use { c -> if (c.moveToFirst()) { number = c.getString(0) ?: number; name = c.getString(1) ?: name } }
            }
        }
    }
    Column(Modifier.fillMaxSize().padding(16.dp), verticalArrangement = Arrangement.spacedBy(14.dp)) {
        Title("Ton contact autorisé", 24.sp)
        Body("C'est la seule personne avec qui tu pourras parler pendant tes sessions. Elle ne peut pas être changée pendant une session.")
        OutlinedTextField(name, { name = it }, Modifier.fillMaxWidth(), label = { Text("Prénom") }, singleLine = true, colors = fieldColors(), shape = RoundedCornerShape(14.dp))
        OutlinedTextField(
            number, { number = it }, Modifier.fillMaxWidth(), label = { Text("Numéro de téléphone") }, singleLine = true,
            keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Phone), colors = fieldColors(), shape = RoundedCornerShape(14.dp),
        )
        Button(
            { pick.launch(Intent(Intent.ACTION_PICK, ContactsContract.CommonDataKinds.Phone.CONTENT_URI)) },
            Modifier.fillMaxWidth().height(50.dp), shape = RoundedCornerShape(16.dp), colors = ButtonDefaults.buttonColors(C.Surface2, C.Text),
        ) { Icon(Icons.Rounded.Contacts, null); Spacer(Modifier.width(8.dp)); Text("Choisir dans mes contacts") }
        Button(
            {
                if (number.filter { it.isDigit() }.length >= 6) {
                    Store.romaneName = name.ifBlank { "Romane" }.trim(); Store.romaneNumber = number.trim(); onDone()
                } else Toast.makeText(ctx, "Numéro invalide", Toast.LENGTH_SHORT).show()
            },
            Modifier.fillMaxWidth().height(54.dp), shape = RoundedCornerShape(16.dp), colors = ButtonDefaults.buttonColors(C.Yellow, Color.Black),
        ) { Text("Enregistrer", fontFamily = Fredoka, fontWeight = FontWeight.Bold, fontSize = 17.sp) }
    }
}

@Composable
private fun Chat(onEdit: () -> Unit) {
    val ctx = LocalContext.current
    val msgs = remember { mutableStateListOf<Msg>() }
    var reload by remember { mutableIntStateOf(0) }
    var text by remember { mutableStateOf("") }
    val callPerm = rememberLauncherForActivityResult(ActivityResultContracts.RequestPermission()) { if (it) call(ctx) }

    LaunchedEffect(reload, Store.romaneNumber) {
        val list = withContext(Dispatchers.IO) { Sms.load(ctx) }
        msgs.clear(); msgs.addAll(list)
    }
    DisposableEffect(Unit) {
        val obs = object : ContentObserver(Handler(Looper.getMainLooper())) {
            override fun onChange(selfChange: Boolean) { reload++ }
        }
        ctx.contentResolver.registerContentObserver(Uri.parse("content://sms"), true, obs)
        onDispose { ctx.contentResolver.unregisterContentObserver(obs) }
    }

    Column(Modifier.fillMaxSize().imePadding()) {
        Row(Modifier.fillMaxWidth().background(C.Surface).padding(horizontal = 16.dp, vertical = 12.dp), verticalAlignment = Alignment.CenterVertically) {
            Avatar(Store.romaneName, 44)
            Spacer(Modifier.width(12.dp))
            Column(Modifier.weight(1f)) {
                Title(Store.romaneName, 20.sp)
                Body(if (Session.blocking) "Seule conversation autorisée 💛" else Store.romaneNumber, size = 12.sp)
            }
            IconButton({
                if (ContextCompat.checkSelfPermission(ctx, Manifest.permission.CALL_PHONE) == PackageManager.PERMISSION_GRANTED) call(ctx)
                else callPerm.launch(Manifest.permission.CALL_PHONE)
            }) { Icon(Icons.Rounded.Call, "Appeler", tint = C.Yellow) }
            IconButton(onEdit) { Icon(Icons.Rounded.Edit, "Modifier", tint = C.Muted) }
        }
        LazyColumn(Modifier.weight(1f).fillMaxWidth(), reverseLayout = true, contentPadding = PaddingValues(12.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
            items(msgs, key = { it.id }) { Bubble(it) }
            if (msgs.isEmpty()) item {
                Body("Aucun message pour l'instant. Dis bonjour à ${Store.romaneName} 👋", modifier = Modifier.fillMaxWidth().padding(30.dp))
            }
        }
        Row(Modifier.fillMaxWidth().padding(12.dp), verticalAlignment = Alignment.CenterVertically) {
            OutlinedTextField(
                text, { text = it }, Modifier.weight(1f), placeholder = { Text("Message à ${Store.romaneName}…") },
                colors = fieldColors(), shape = RoundedCornerShape(24.dp), maxLines = 5,
            )
            Spacer(Modifier.width(8.dp))
            Box(
                Modifier.size(52.dp).clip(CircleShape).background(if (text.isBlank()) C.Surface2 else C.Yellow).clickable(enabled = text.isNotBlank()) {
                    val t = text.trim()
                    if (Sms.send(ctx, t)) {
                        msgs.add(0, Msg(-System.nanoTime(), t, System.currentTimeMillis(), mine = true, pending = true))
                        text = ""
                    } else Toast.makeText(ctx, "Envoi impossible", Toast.LENGTH_SHORT).show()
                },
                contentAlignment = Alignment.Center,
            ) { Icon(Icons.AutoMirrored.Rounded.Send, "Envoyer", tint = if (text.isBlank()) C.Muted else Color.Black) }
        }
    }
}

private val timeFmt = SimpleDateFormat("dd/MM HH:mm", Locale.FRENCH)

@Composable
private fun Bubble(m: Msg) {
    Box(Modifier.fillMaxWidth(), contentAlignment = if (m.mine) Alignment.CenterEnd else Alignment.CenterStart) {
        Column(horizontalAlignment = if (m.mine) Alignment.End else Alignment.Start) {
            Box(
                Modifier.widthIn(max = 290.dp)
                    .clip(RoundedCornerShape(topStart = 20.dp, topEnd = 20.dp, bottomStart = if (m.mine) 20.dp else 6.dp, bottomEnd = if (m.mine) 6.dp else 20.dp))
                    .background(if (m.mine) C.Yellow else C.Surface2)
                    .padding(horizontal = 14.dp, vertical = 10.dp),
            ) { Text(m.body, color = if (m.mine) Color.Black else C.Text, fontFamily = Nunito, fontSize = 15.sp) }
            Body(if (m.pending) "envoi…" else timeFmt.format(Date(m.date)), size = 10.sp, modifier = Modifier.padding(horizontal = 6.dp, vertical = 2.dp))
        }
    }
}

private fun call(ctx: Context) {
    Store.callGraceUntil = System.currentTimeMillis() + 30_000
    runCatching { ctx.startActivity(Intent(Intent.ACTION_CALL, Uri.parse("tel:" + Store.romaneNumber))) }
}
