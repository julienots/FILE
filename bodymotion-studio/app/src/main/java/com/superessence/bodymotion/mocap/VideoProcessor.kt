package com.superessence.bodymotion.mocap

import android.content.Context
import android.graphics.Bitmap
import android.media.MediaMetadataRetriever
import android.net.Uri
import kotlinx.coroutines.ensureActive
import kotlin.coroutines.coroutineContext

/** Capture depuis une vidéo importée : chaque image est analysée à la cadence demandée. */
object VideoProcessor {
    suspend fun process(ctx: Context, uri: Uri, fps: Int, tracker: BodyTracker, onProgress: (Float, PoseFrame?, Bitmap?) -> Unit): List<PoseFrame> {
        val mmr = MediaMetadataRetriever()
        try {
            mmr.setDataSource(ctx, uri)
            val durMs = mmr.extractMetadata(MediaMetadataRetriever.METADATA_KEY_DURATION)?.toLongOrNull() ?: throw IllegalArgumentException("Vidéo illisible.")
            val w = mmr.extractMetadata(MediaMetadataRetriever.METADATA_KEY_VIDEO_WIDTH)?.toIntOrNull() ?: 720
            val h = mmr.extractMetadata(MediaMetadataRetriever.METADATA_KEY_VIDEO_HEIGHT)?.toIntOrNull() ?: 1280
            val rot = mmr.extractMetadata(MediaMetadataRetriever.METADATA_KEY_VIDEO_ROTATION)?.toIntOrNull() ?: 0
            val (sw, sh) = if (rot == 90 || rot == 270) h to w else w to h
            val scale = 720f / maxOf(sw, sh)
            val tw = (w * scale).toInt().coerceAtLeast(64); val th = (h * scale).toInt().coerceAtLeast(64)
            val out = ArrayList<PoseFrame>()
            val step = 1000L / fps
            var t = 0L
            while (t <= durMs) {
                coroutineContext.ensureActive()
                val bmp = if (android.os.Build.VERSION.SDK_INT >= 27) mmr.getScaledFrameAtTime(t * 1000, MediaMetadataRetriever.OPTION_CLOSEST, tw, th)
                else mmr.getFrameAtTime(t * 1000, MediaMetadataRetriever.OPTION_CLOSEST)?.let { Bitmap.createScaledBitmap(it, tw, th, true) }
                if (bmp != null) {
                    val argb = if (bmp.config != Bitmap.Config.ARGB_8888) bmp.copy(Bitmap.Config.ARGB_8888, false) else bmp
                    val f = tracker.process(argb, t)
                    out.add(f)
                    onProgress(t.toFloat() / durMs, f, argb)
                }
                t += step
            }
            return out
        } finally { mmr.release() }
    }
}
