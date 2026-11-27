package com.superessence.pleinphare.data

import android.Manifest
import android.content.ContentUris
import android.content.Context
import android.content.pm.PackageManager
import android.net.Uri
import android.os.Build
import android.provider.MediaStore
import androidx.core.content.ContextCompat
import androidx.media3.common.MediaItem
import androidx.media3.common.MediaMetadata

/** Un morceau présent sur le téléphone. */
data class Song(
    val id: Long,
    val title: String,
    val artist: String,
    val album: String,
    val albumId: Long,
    val durationMs: Long,
    /** Date d'arrivée du fichier sur le téléphone, en secondes depuis 1970. */
    val dateAddedSec: Long,
) {
    val uri: Uri get() = ContentUris.withAppendedId(MediaStore.Audio.Media.EXTERNAL_CONTENT_URI, id)
    val artUri: Uri get() = ContentUris.withAppendedId(Uri.parse("content://media/external/audio/albumart"), albumId)

    fun toMediaItem(): MediaItem = MediaItem.Builder()
        .setMediaId(id.toString())
        .setUri(uri)
        .setMediaMetadata(
            MediaMetadata.Builder()
                .setTitle(title)
                .setArtist(artist)
                .setAlbumTitle(album)
                .setArtworkUri(artUri)
                .setIsPlayable(true)
                .setIsBrowsable(false)
                .setMediaType(MediaMetadata.MEDIA_TYPE_MUSIC)
                .build(),
        )
        .build()
}

/** Lit toute la musique du téléphone via MediaStore (mémoire, carte SD, téléchargements…). */
object MusicLibrary {
    @Volatile
    var cache: List<Song>? = null
        private set

    val permission: String
        get() = if (Build.VERSION.SDK_INT >= 33) Manifest.permission.READ_MEDIA_AUDIO
        else Manifest.permission.READ_EXTERNAL_STORAGE

    fun hasPermission(ctx: Context) =
        ContextCompat.checkSelfPermission(ctx, permission) == PackageManager.PERMISSION_GRANTED

    /** Bibliothèque en cache, chargée au besoin. Triée de la plus récente à la plus ancienne. */
    fun get(ctx: Context): List<Song> = cache ?: load(ctx)

    fun find(ctx: Context, id: Long): Song? = get(ctx).firstOrNull { it.id == id }

    // Dossiers qui contiennent des sons qui ne sont pas de la musique.
    private val excludedFolders = listOf(
        "whatsapp", "voice notes", "voice recorder", "recordings", "recorder", "call", "ringtones",
        "notifications", "alarms", "telegram audio", "audiobooks",
    )

    fun load(ctx: Context): List<Song> {
        if (!hasPermission(ctx)) return emptyList()
        val projection = buildList {
            add(MediaStore.Audio.Media._ID)
            add(MediaStore.Audio.Media.TITLE)
            add(MediaStore.Audio.Media.ARTIST)
            add(MediaStore.Audio.Media.ALBUM)
            add(MediaStore.Audio.Media.ALBUM_ID)
            add(MediaStore.Audio.Media.DURATION)
            add(MediaStore.Audio.Media.DATE_ADDED)
            add(MediaStore.Audio.Media.DATE_MODIFIED)
            add(MediaStore.Audio.Media.DISPLAY_NAME)
            @Suppress("DEPRECATION")
            add(if (Build.VERSION.SDK_INT >= 29) MediaStore.Audio.Media.RELATIVE_PATH else MediaStore.Audio.Media.DATA)
        }.toTypedArray()
        val songs = ArrayList<Song>()
        try {
            ctx.contentResolver.query(
                MediaStore.Audio.Media.EXTERNAL_CONTENT_URI,
                projection,
                "${MediaStore.Audio.Media.IS_MUSIC} != 0",
                null,
                null,
            )?.use { c ->
                val iId = c.getColumnIndexOrThrow(projection[0])
                val iTitle = c.getColumnIndexOrThrow(projection[1])
                val iArtist = c.getColumnIndexOrThrow(projection[2])
                val iAlbum = c.getColumnIndexOrThrow(projection[3])
                val iAlbumId = c.getColumnIndexOrThrow(projection[4])
                val iDur = c.getColumnIndexOrThrow(projection[5])
                val iAdded = c.getColumnIndexOrThrow(projection[6])
                val iMod = c.getColumnIndexOrThrow(projection[7])
                val iName = c.getColumnIndexOrThrow(projection[8])
                val iPath = c.getColumnIndexOrThrow(projection[9])
                while (c.moveToNext()) {
                    val path = (c.getString(iPath) ?: "").lowercase()
                    if (excludedFolders.any { path.contains(it) }) continue
                    val dur = c.getLong(iDur)
                    if (dur in 1 until 15_000) continue // bruitages, notes vocales très courtes
                    val name = c.getString(iName) ?: ""
                    val added = c.getLong(iAdded).takeIf { it > 0 } ?: c.getLong(iMod)
                    songs += Song(
                        id = c.getLong(iId),
                        title = cleanTitle(c.getString(iTitle), name),
                        artist = cleanArtist(c.getString(iArtist)),
                        album = c.getString(iAlbum)?.takeIf { it.isNotBlank() && it != "<unknown>" } ?: "",
                        albumId = c.getLong(iAlbumId),
                        durationMs = dur,
                        dateAddedSec = added,
                    )
                }
            }
        } catch (_: SecurityException) {
            return emptyList()
        }
        val sorted = sortByDate(songs, newestFirst = true)
        cache = sorted
        return sorted
    }

    private fun cleanTitle(title: String?, fileName: String): String =
        title?.takeIf { it.isNotBlank() } ?: fileName.substringBeforeLast('.').ifBlank { "Sans titre" }

    private fun cleanArtist(artist: String?): String =
        artist?.takeIf { it.isNotBlank() && it != "<unknown>" } ?: "Artiste inconnu"
}
