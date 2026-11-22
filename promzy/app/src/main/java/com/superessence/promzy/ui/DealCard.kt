package com.superessence.promzy.ui

import android.content.Context
import android.content.Intent
import android.net.Uri
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.OpenInNew
import androidx.compose.material.icons.filled.Favorite
import androidx.compose.material.icons.filled.FavoriteBorder
import androidx.compose.material.icons.filled.LocalFireDepartment
import androidx.compose.material.icons.filled.Share
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextDecoration
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import coil.compose.SubcomposeAsyncImage
import com.superessence.promzy.data.Deal

fun timeAgo(t: Long, now: Long = System.currentTimeMillis()): String {
    if (t <= 0) return ""
    val s = ((now - t) / 1000).coerceAtLeast(0)
    return when {
        s < 60 -> "à l'instant"
        s < 3600 -> "il y a ${s / 60} min"
        s < 86_400 -> "il y a ${s / 3600} h"
        else -> "il y a ${s / 86_400} j"
    }
}

fun openDeal(ctx: Context, d: Deal) {
    runCatching { ctx.startActivity(Intent(Intent.ACTION_VIEW, Uri.parse(d.link)).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)) }
}

fun shareDeal(ctx: Context, d: Deal) {
    val text = buildString {
        append(d.title)
        d.price?.let { append(" — $it") }
        append("\n").append(d.link).append("\n\nTrouvé sur Promzy")
    }
    ctx.startActivity(Intent.createChooser(Intent(Intent.ACTION_SEND).setType("text/plain").putExtra(Intent.EXTRA_TEXT, text), "Partager la promo").addFlags(Intent.FLAG_ACTIVITY_NEW_TASK))
}

@Composable
private fun DealImage(d: Deal, modifier: Modifier) {
    Box(modifier.clip(RoundedCornerShape(14.dp)).background(Color.White), contentAlignment = Alignment.Center) {
        val fallback = @Composable {
            Box(Modifier.fillMaxSize().background(CardHi), contentAlignment = Alignment.Center) { PromzyMark(40.dp, color = Yellow.copy(alpha = 0.35f), shadow = false) }
        }
        if (d.image == null) fallback() else SubcomposeAsyncImage(
            model = d.image, contentDescription = null, contentScale = ContentScale.Fit,
            modifier = Modifier.fillMaxSize().padding(4.dp),
            loading = { fallback() }, error = { fallback() },
        )
    }
}

@Composable
fun TempBadge(t: Int) {
    val c = when { t >= 500 -> Live; t >= 200 -> Hot; t >= 50 -> Color(0xFFFF9F1C); else -> TextMid }
    Row(
        Modifier.clip(RoundedCornerShape(50)).background(c.copy(alpha = 0.16f)).padding(horizontal = 7.dp, vertical = 2.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Icon(Icons.Default.LocalFireDepartment, null, tint = c, modifier = Modifier.size(13.dp))
        Text("$t°", color = c, fontFamily = Nunito, fontWeight = FontWeight.Bold, fontSize = 12.sp)
    }
}

@Composable
fun Pill(text: String, bg: Color, fg: Color) {
    Text(
        text, color = fg, fontFamily = Nunito, fontWeight = FontWeight.Bold, fontSize = 10.sp, letterSpacing = 0.6.sp,
        modifier = Modifier.clip(RoundedCornerShape(50)).background(bg).padding(horizontal = 7.dp, vertical = 2.dp),
    )
}

@Composable
private fun PriceLine(d: Deal, big: Boolean) {
    Row(verticalAlignment = Alignment.Bottom) {
        Text(d.price ?: "Voir l'offre", color = Yellow, fontFamily = Fredoka, fontWeight = FontWeight.Bold, fontSize = if (d.price == null) 15.sp else if (big) 26.sp else 20.sp)
        d.oldPrice?.let {
            Spacer(Modifier.width(8.dp))
            Text(it, color = TextLow, fontFamily = Nunito, fontSize = 13.sp, textDecoration = TextDecoration.LineThrough, modifier = Modifier.padding(bottom = 2.dp))
        }
    }
}

@Composable
private fun MetaLine(d: Deal, fresh: Boolean) {
    Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(6.dp)) {
        if (fresh) Pill("NOUVEAU", Live, Color.White)
        d.temperature?.let { TempBadge(it) }
        val meta = listOfNotNull(d.merchant, timeAgo(d.publishedAt).takeIf { it.isNotEmpty() }).joinToString(" · ")
        Text(meta, color = TextLow, fontFamily = Nunito, fontSize = 12.sp, maxLines = 1, overflow = TextOverflow.Ellipsis)
    }
}

@Composable
fun DealCard(d: Deal, isFav: Boolean, onFav: () -> Unit, rank: Int? = null, fresh: Boolean = false) {
    val ctx = LocalContext.current
    Row(
        Modifier.fillMaxWidth().padding(horizontal = 16.dp, vertical = 5.dp)
            .clip(RoundedCornerShape(20.dp)).background(Card).clickable { openDeal(ctx, d) }.padding(10.dp),
    ) {
        Box {
            DealImage(d, Modifier.size(100.dp))
            rank?.let {
                Text(
                    "#$it", color = Color.Black, fontFamily = Fredoka, fontWeight = FontWeight.Bold, fontSize = 13.sp,
                    modifier = Modifier.padding(4.dp).clip(RoundedCornerShape(8.dp)).background(Yellow).padding(horizontal = 6.dp, vertical = 1.dp),
                )
            }
        }
        Spacer(Modifier.width(12.dp))
        Column(Modifier.weight(1f)) {
            MetaLine(d, fresh)
            Spacer(Modifier.height(4.dp))
            Text(d.title, color = TextHi, fontFamily = Nunito, fontWeight = FontWeight.SemiBold, fontSize = 14.5.sp, lineHeight = 19.sp, maxLines = 3, overflow = TextOverflow.Ellipsis)
            Spacer(Modifier.height(4.dp))
            Row(verticalAlignment = Alignment.CenterVertically) {
                Box(Modifier.weight(1f)) { PriceLine(d, false) }
                IconButton(onClick = { shareDeal(ctx, d) }, modifier = Modifier.size(34.dp)) { Icon(Icons.Default.Share, "Partager", tint = TextLow, modifier = Modifier.size(18.dp)) }
                IconButton(onClick = onFav, modifier = Modifier.size(34.dp)) {
                    Icon(if (isFav) Icons.Default.Favorite else Icons.Default.FavoriteBorder, "Favori", tint = if (isFav) Live else TextLow, modifier = Modifier.size(20.dp))
                }
            }
        }
    }
}

/** Carte « n°1 » mise en avant en haut du classement. */
@Composable
fun HeroDealCard(d: Deal, isFav: Boolean, onFav: () -> Unit, fresh: Boolean = false) {
    val ctx = LocalContext.current
    Column(
        Modifier.fillMaxWidth().padding(horizontal = 16.dp, vertical = 6.dp)
            .clip(RoundedCornerShape(24.dp))
            .background(Brush.verticalGradient(listOf(Color(0xFF2A2411), Card)))
            .border(1.dp, Yellow.copy(alpha = 0.35f), RoundedCornerShape(24.dp))
            .clickable { openDeal(ctx, d) }.padding(14.dp),
    ) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Pill("N°1 DU MOMENT", Yellow, Color.Black)
            Spacer(Modifier.width(8.dp))
            MetaLine(d, fresh)
        }
        Spacer(Modifier.height(12.dp))
        DealImage(d, Modifier.fillMaxWidth().aspectRatio(16f / 9f))
        Spacer(Modifier.height(12.dp))
        Text(d.title, color = TextHi, fontFamily = Nunito, fontWeight = FontWeight.Bold, fontSize = 17.sp, lineHeight = 22.sp, maxLines = 3, overflow = TextOverflow.Ellipsis)
        Spacer(Modifier.height(6.dp))
        Row(verticalAlignment = Alignment.CenterVertically) {
            Box(Modifier.weight(1f)) { PriceLine(d, true) }
            IconButton(onClick = onFav) { Icon(if (isFav) Icons.Default.Favorite else Icons.Default.FavoriteBorder, "Favori", tint = if (isFav) Live else TextMid) }
            Row(
                Modifier.clip(CircleShape).background(Yellow).clickable { openDeal(ctx, d) }.padding(horizontal = 14.dp, vertical = 8.dp),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                Text("J'en profite", color = Color.Black, fontFamily = Nunito, fontWeight = FontWeight.Bold, fontSize = 14.sp)
                Spacer(Modifier.width(4.dp))
                Icon(Icons.AutoMirrored.Filled.OpenInNew, null, tint = Color.Black, modifier = Modifier.size(15.dp))
            }
        }
    }
}
