package com.superessence.tagforge.ui

import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.content.Intent
import android.graphics.Bitmap
import android.net.Uri
import android.widget.Toast
import androidx.compose.animation.core.*
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.filled.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.FilterQuality
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.text.input.VisualTransformation
import androidx.compose.ui.unit.dp
import androidx.compose.foundation.text.KeyboardOptions
import com.google.zxing.BarcodeFormat
import com.google.zxing.EncodeHintType
import com.google.zxing.qrcode.QRCodeWriter
import com.google.zxing.qrcode.decoder.ErrorCorrectionLevel
import com.superessence.tagforge.nfc.Support

@Composable
fun ScreenHeader(title: String, subtitle: String? = null, onBack: (() -> Unit)? = null, actions: @Composable RowScope.() -> Unit = {}) {
    Row(Modifier.fillMaxWidth().padding(start = 8.dp, end = 8.dp, top = 8.dp, bottom = 4.dp), verticalAlignment = Alignment.CenterVertically) {
        if (onBack != null) IconButton(onClick = onBack) { Icon(Icons.AutoMirrored.Filled.ArrowBack, "Retour") } else Spacer(Modifier.width(8.dp))
        Column(Modifier.weight(1f).padding(start = 4.dp)) {
            Text(title, style = MaterialTheme.typography.headlineSmall, fontWeight = FontWeight.Bold)
            if (subtitle != null) Text(subtitle, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
        }
        actions()
    }
}

@Composable
fun SectionTitle(text: String, modifier: Modifier = Modifier) {
    Text(text.uppercase(), modifier.padding(top = 18.dp, bottom = 8.dp), style = MaterialTheme.typography.labelMedium,
        color = MaterialTheme.colorScheme.primary, fontWeight = FontWeight.Bold)
}

@Composable
fun Panel(modifier: Modifier = Modifier, onClick: (() -> Unit)? = null, content: @Composable ColumnScope.() -> Unit) {
    val m = modifier.fillMaxWidth().clip(RoundedCornerShape(18.dp)).background(MaterialTheme.colorScheme.surface)
        .let { if (onClick != null) it.clickable(onClick = onClick) else it }.padding(16.dp)
    Column(m, content = content)
}

@Composable
fun SupportBadge(s: Support) {
    val (bg, fg) = if (s == Support.NATIVE) Teal.copy(alpha = .15f) to Teal else Violet.copy(alpha = .18f) to Violet
    Text(s.label, Modifier.clip(RoundedCornerShape(8.dp)).background(bg).padding(horizontal = 8.dp, vertical = 2.dp),
        color = fg, style = MaterialTheme.typography.labelSmall, fontWeight = FontWeight.SemiBold)
}

@Composable
fun IconBubble(icon: ImageVector, tint: Color = MaterialTheme.colorScheme.primary, size: Int = 40) {
    Box(Modifier.size(size.dp).clip(CircleShape).background(tint.copy(alpha = .14f)), contentAlignment = Alignment.Center) {
        Icon(icon, null, tint = tint, modifier = Modifier.size((size * 0.55).dp))
    }
}

@Composable
fun InfoNote(text: String, icon: ImageVector = Icons.Filled.Info, color: Color = MaterialTheme.colorScheme.secondary) {
    Row(Modifier.fillMaxWidth().clip(RoundedCornerShape(14.dp)).background(color.copy(alpha = .12f)).padding(12.dp)) {
        Icon(icon, null, tint = color, modifier = Modifier.size(20.dp))
        Spacer(Modifier.width(10.dp))
        Text(text, style = MaterialTheme.typography.bodySmall)
    }
}

@Composable
fun KeyValue(k: String, v: String) {
    Row(Modifier.fillMaxWidth().padding(vertical = 3.dp)) {
        Text(k, Modifier.width(130.dp), style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
        Text(v, Modifier.weight(1f), style = MaterialTheme.typography.bodySmall)
    }
}

@Composable
fun SwitchRow(title: String, checked: Boolean, subtitle: String? = null, onChange: (Boolean) -> Unit) {
    Row(Modifier.fillMaxWidth().clickable { onChange(!checked) }.padding(vertical = 6.dp), verticalAlignment = Alignment.CenterVertically) {
        Column(Modifier.weight(1f)) {
            Text(title, style = MaterialTheme.typography.bodyLarge)
            if (subtitle != null) Text(subtitle, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
        }
        Switch(checked, onChange)
    }
}

@Composable
fun Field(
    value: String, onChange: (String) -> Unit, label: String, hint: String = "", modifier: Modifier = Modifier,
    multiline: Boolean = false, keyboard: KeyboardType = KeyboardType.Text, password: Boolean = false,
    trailing: @Composable (() -> Unit)? = null,
) {
    var show by remember { mutableStateOf(false) }
    OutlinedTextField(
        value, onChange, modifier.fillMaxWidth().padding(vertical = 4.dp),
        label = { Text(label) },
        placeholder = if (hint.isNotEmpty()) ({ Text(hint) }) else null,
        singleLine = !multiline, minLines = if (multiline) 3 else 1,
        keyboardOptions = KeyboardOptions(keyboardType = keyboard),
        visualTransformation = if (password && !show) PasswordVisualTransformation() else VisualTransformation.None,
        trailingIcon = trailing ?: if (password) ({ IconButton(onClick = { show = !show }) { Icon(if (show) Icons.Filled.VisibilityOff else Icons.Filled.Visibility, null) } }) else null,
        shape = RoundedCornerShape(14.dp),
    )
}

@Composable
fun SelectField(value: String, options: List<Pair<String, String>>, label: String, onChange: (String) -> Unit) {
    var open by remember { mutableStateOf(false) }
    Box(Modifier.fillMaxWidth().padding(vertical = 4.dp)) {
        OutlinedTextField(
            options.firstOrNull { it.first == value }?.second ?: value, {}, Modifier.fillMaxWidth(),
            readOnly = true, label = { Text(label) }, singleLine = true,
            trailingIcon = { Icon(Icons.Filled.ArrowDropDown, null) }, shape = RoundedCornerShape(14.dp),
        )
        Box(Modifier.matchParentSize().clip(RoundedCornerShape(14.dp)).clickable { open = true })
        DropdownMenu(open, { open = false }, Modifier.heightIn(max = 360.dp)) {
            options.forEach { (k, v) -> DropdownMenuItem(text = { Text(v) }, onClick = { onChange(k); open = false }) }
        }
    }
}

/** Animation d'ondes NFC. */
@Composable
fun NfcPulse(modifier: Modifier = Modifier, color: Color = Teal) {
    val t = rememberInfiniteTransition(label = "pulse")
    val p by t.animateFloat(0f, 1f, infiniteRepeatable(tween(1800, easing = LinearEasing)), label = "p")
    Canvas(modifier) {
        val c = Offset(size.width / 2, size.height / 2)
        val maxR = size.minDimension / 2
        for (i in 0 until 3) {
            val f = (p + i / 3f) % 1f
            drawCircle(color.copy(alpha = (1f - f) * .55f), radius = maxR * (0.25f + 0.75f * f), center = c, style = Stroke(width = 4f))
        }
        drawCircle(color.copy(alpha = .9f), radius = maxR * 0.2f, center = c)
    }
}

fun qrBitmap(text: String, size: Int = 720): Bitmap {
    val m = QRCodeWriter().encode(text, BarcodeFormat.QR_CODE, size, size, mapOf(EncodeHintType.MARGIN to 1, EncodeHintType.ERROR_CORRECTION to ErrorCorrectionLevel.M, EncodeHintType.CHARACTER_SET to "UTF-8"))
    val px = IntArray(size * size) { i -> if (m[i % size, i / size]) 0xFF000000.toInt() else 0xFFFFFFFF.toInt() }
    return Bitmap.createBitmap(px, size, size, Bitmap.Config.ARGB_8888)
}

@Composable
fun QrDialog(text: String, onDismiss: () -> Unit) {
    val bmp = remember(text) { runCatching { qrBitmap(text) }.getOrNull() }
    AlertDialog(
        onDismissRequest = onDismiss,
        confirmButton = { TextButton(onClick = onDismiss) { Text("Fermer") } },
        title = { Text("QR code équivalent") },
        text = {
            Column(horizontalAlignment = Alignment.CenterHorizontally) {
                if (bmp != null) Image(bmp.asImageBitmap(), null, Modifier.size(260.dp).clip(RoundedCornerShape(12.dp)), filterQuality = FilterQuality.None)
                else Text("Contenu trop long pour un QR code.")
                Spacer(Modifier.height(8.dp))
                Text("Pour les téléphones sans NFC (iPhone anciens…) : imprimez-le à côté du tag.", style = MaterialTheme.typography.bodySmall)
            }
        },
    )
}

fun Context.copy(text: String) {
    getSystemService(ClipboardManager::class.java).setPrimaryClip(ClipData.newPlainText("TagForge", text))
    Toast.makeText(this, "Copié", Toast.LENGTH_SHORT).show()
}

fun Context.openUri(u: String) {
    runCatching { startActivity(Intent(Intent.ACTION_VIEW, Uri.parse(u)).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)) }
        .onFailure { Toast.makeText(this, "Aucune appli pour ouvrir ce lien", Toast.LENGTH_SHORT).show() }
}

fun Context.shareText(text: String) {
    startActivity(Intent.createChooser(Intent(Intent.ACTION_SEND).setType("text/plain").putExtra(Intent.EXTRA_TEXT, text), "Partager"))
}

fun Context.toast(s: String) = Toast.makeText(this, s, Toast.LENGTH_LONG).show()
