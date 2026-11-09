package com.superessence.noctea.audio

import android.content.Context
import android.net.Uri
import androidx.annotation.OptIn
import androidx.media3.common.C
import androidx.media3.common.MediaItem
import androidx.media3.common.util.UnstableApi
import androidx.media3.exoplayer.drm.DrmSessionManagerProvider
import androidx.media3.exoplayer.source.ConcatenatingMediaSource2
import androidx.media3.exoplayer.source.DefaultMediaSourceFactory
import androidx.media3.exoplayer.source.MediaSource
import androidx.media3.exoplayer.source.SilenceMediaSource
import androidx.media3.exoplayer.upstream.LoadErrorHandlingPolicy
import com.superessence.noctea.data.Seg
import java.io.File

/**
 * Transforme une timeline (clips vocaux + vrais silences) en une seule source Media3
 * continue : une séance = un seul élément pour le lecteur, la notification et l'écran verrouillé,
 * avec une durée exacte, une barre de progression et un déplacement précis.
 *
 * Les silences utilisent un fichier Opus silencieux découpé (même format que la voix),
 * ce qui garantit des enchaînements sans coupure.
 */
@OptIn(UnstableApi::class)
class NocteaMediaSourceFactory(
    private val context: Context,
    private val resolver: () -> PlayableResolver,
) : MediaSource.Factory {

    private val delegate = DefaultMediaSourceFactory(context)

    override fun setDrmSessionManagerProvider(provider: DrmSessionManagerProvider): MediaSource.Factory {
        delegate.setDrmSessionManagerProvider(provider); return this
    }

    override fun setLoadErrorHandlingPolicy(policy: LoadErrorHandlingPolicy): MediaSource.Factory {
        delegate.setLoadErrorHandlingPolicy(policy); return this
    }

    override fun getSupportedTypes(): IntArray = delegate.supportedTypes

    override fun createMediaSource(mediaItem: MediaItem): MediaSource {
        val playable = resolver().resolve(mediaItem.mediaId)
        if (playable == null) {
            // Élément inconnu (ex. supprimé) : courte source silencieuse plutôt qu'un plantage.
            return ConcatenatingMediaSource2.Builder().setMediaItem(mediaItem)
                .add(SilenceMediaSource(1_000_000L), 1_000L).build()
        }
        val builder = ConcatenatingMediaSource2.Builder()
            .setMediaItem(mediaItem)
            .setMediaSourceFactory(delegate)
        if (playable.kind == PlayableKind.MIXER) {
            builder.add(SilenceMediaSource(playable.durationMs * 1000L), playable.durationMs)
            return builder.build()
        }
        var count = 0
        for (seg in playable.segments) {
            when (seg) {
                is Seg.Clip -> builder.add(asset("voice/${seg.id}.ogg"), seg.ms.coerceAtLeast(1)).also { count++ }
                is Seg.Fx -> builder.add(asset("voice/${seg.name}.ogg"), seg.ms.coerceAtLeast(1)).also { count++ }
                is Seg.File -> {
                    if (File(seg.path).exists()) {
                        builder.add(MediaItem.fromUri(Uri.fromFile(File(seg.path))), seg.ms.coerceAtLeast(1)); count++
                    } else {
                        count += addSilence(builder, seg.ms)
                    }
                }
                is Seg.Silence -> count += addSilence(builder, seg.ms)
            }
        }
        if (count == 0) builder.add(SilenceMediaSource(1_000_000L), 1_000L)
        return builder.build()
    }

    private fun addSilence(builder: ConcatenatingMediaSource2.Builder, ms: Long): Int {
        var left = ms
        var n = 0
        while (left > 0) {
            val chunk = minOf(left, SILENCE_FILE_MS)
            if (chunk < 40) break
            val item = MediaItem.Builder()
                .setUri("asset:///voice/silence.ogg")
                .setClippingConfiguration(
                    MediaItem.ClippingConfiguration.Builder().setEndPositionMs(chunk).build(),
                )
                .build()
            builder.add(item, chunk)
            left -= chunk
            n++
        }
        return n
    }

    private fun asset(path: String) = MediaItem.fromUri("asset:///$path")

    companion object {
        const val SILENCE_FILE_MS = 59_000L
        @Suppress("unused")
        private const val UNSET = C.TIME_UNSET
    }
}
