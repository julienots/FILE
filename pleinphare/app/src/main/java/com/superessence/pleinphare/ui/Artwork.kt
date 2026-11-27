package com.superessence.pleinphare.ui

import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.os.Build
import android.util.LruCache
import android.util.Size
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.MusicNote
import androidx.compose.material3.Icon
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.ImageBitmap
import androidx.compose.ui.graphics.Shape
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.platform.LocalContext
import androidx.compose.foundation.layout.fillMaxSize as fill
import com.superessence.pleinphare.data.Song
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import android.content.ContentUris
import android.content.Context
import android.provider.MediaStore
import android.net.Uri

/** Pochettes : chargées en arrière-plan, mises en cache, avec un visuel de repli. */
private object ArtCache {
    val images = LruCache<String, ImageBitmap>(160)
    val missing = HashSet<String>()
}

private fun loadArt(ctx: Context, songId: Long, albumId: Long, px: Int): Bitmap? {
    if (Build.VERSION.SDK_INT >= 29) {
        val uri = ContentUris.withAppendedId(MediaStore.Audio.Media.EXTERNAL_CONTENT_URI, songId)
        runCatching { return ctx.contentResolver.loadThumbnail(uri, Size(px, px), null) }
    }
    val art = ContentUris.withAppendedId(Uri.parse("content://media/external/audio/albumart"), albumId)
    return runCatching {
        ctx.contentResolver.openInputStream(art)?.use { input ->
            val bytes = input.readBytes()
            val o = BitmapFactory.Options().apply { inJustDecodeBounds = true }
            BitmapFactory.decodeByteArray(bytes, 0, bytes.size, o)
            var sample = 1
            while (o.outWidth / (sample * 2) >= px) sample *= 2
            BitmapFactory.decodeByteArray(bytes, 0, bytes.size, BitmapFactory.Options().apply { inSampleSize = sample })
        }
    }.getOrNull()
}

@Composable
fun Artwork(songId: Long?, albumId: Long, px: Int, shape: Shape, modifier: Modifier, iconFraction: Float = 0.42f) {
    val ctx = LocalContext.current
    val key = "$songId:$px"
    var img by remember(key) { mutableStateOf(ArtCache.images.get(key)) }
    LaunchedEffect(key) {
        if (img == null && songId != null && key !in ArtCache.missing) {
            val bmp = withContext(Dispatchers.IO) { loadArt(ctx, songId, albumId, px) }
            if (bmp != null) {
                val ib = bmp.asImageBitmap()
                ArtCache.images.put(key, ib)
                img = ib
            } else synchronized(ArtCache.missing) { ArtCache.missing += key }
        }
    }
    Box(
        modifier.clip(shape).background(Brush.linearGradient(listOf(PanelHi, Panel))),
        contentAlignment = Alignment.Center,
    ) {
        val bitmap = img
        if (bitmap != null) {
            Image(bitmap, null, Modifier.fill(), contentScale = ContentScale.Crop)
        } else {
            Icon(
                Icons.Rounded.MusicNote, null, tint = SuperYellow.copy(alpha = 0.85f),
                modifier = Modifier.fillMaxSize(iconFraction),
            )
        }
    }
}

@Composable
fun SongArtwork(song: Song, px: Int, shape: Shape, modifier: Modifier) =
    Artwork(song.id, song.albumId, px, shape, modifier)
