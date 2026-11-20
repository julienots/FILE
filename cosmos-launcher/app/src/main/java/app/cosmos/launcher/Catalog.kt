package app.cosmos.launcher

import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.graphics.Color
import android.graphics.LinearGradient
import android.graphics.drawable.Drawable
import android.net.Uri
import android.provider.Settings

enum class SystemAction { ALL_APPS, SETTINGS, WIFI, BLUETOOTH, DISPLAY, SOUND }

class Tile(
    val id: String,
    val label: String,
    val subtitle: String,
    val color1: Int,
    val color2: Int,
    val glyph: String,
    val pkgs: List<String> = emptyList(),
    val system: SystemAction? = null,
) {
    var pkg: String? = null
    var launchIntent: Intent? = null
    var banner: Drawable? = null
    var icon: Drawable? = null
    var shader: LinearGradient? = null
    var shortLabel: String? = null
    var fakeInstalled = false

    val available: Boolean get() = system != null || launchIntent != null || fakeInstalled
    val storePkg: String? get() = pkgs.firstOrNull()
}

class Row(val title: String, val tiles: List<Tile>)

object Catalog {
    private fun c(s: String) = Color.parseColor(s)

    private fun app(
        id: String, label: String, sub: String, c1: String, c2: String, glyph: String, vararg pkgs: String,
    ) = Tile(id, label, sub, c(c1), c(c2), glyph, pkgs.toList())

    private fun sys(id: String, label: String, sub: String, c1: String, c2: String, glyph: String, a: SystemAction) =
        Tile(id, label, sub, c(c1), c(c2), glyph, system = a)

    fun rows(): List<Row> = listOf(
        Row(
            "STREAMING", listOf(
                app("yt", "YouTube", "Vidéos, musique et lives", "#FF1F3D", "#8E0018", "▶", "com.google.android.youtube.tv", "com.google.android.youtube.tvunplugged"),
                app("nflx", "Netflix", "Films et séries", "#E50914", "#3A0003", "N", "com.netflix.ninja"),
                app("dplus", "Disney+", "Disney, Marvel, Star Wars", "#1B4DFF", "#0A1B6B", "D+", "com.disney.disneyplus"),
                app("prime", "Prime Video", "Films et séries Amazon", "#00B2F0", "#06405F", "P", "com.amazon.amazonvideo.livingroom", "com.amazon.avod"),
                app("canal", "Canal+", "Cinéma, sport, séries", "#2A2A2A", "#000000", "C+", "com.canal.android.canal"),
                app("atv", "Apple TV", "Séries et films originaux", "#3A3A3C", "#0B0B0C", "tv", "com.apple.atve.androidtv.appletv"),
                app("cr", "Crunchyroll", "Animés en streaming", "#FF7A1A", "#9A3A00", "CR", "com.crunchyroll.crunchyroid"),
                app("twitch", "Twitch", "Lives et jeux vidéo", "#9146FF", "#3B1C7A", "T", "tv.twitch.android.app"),
            ),
        ),
        Row(
            "TV GRATUITE & DIRECT", listOf(
                app("pluto", "Pluto TV", "Chaînes gratuites en direct", "#FFD60A", "#8A6A00", "P", "tv.pluto.android"),
                app("ftv", "France.tv", "Direct et replay", "#3D7BFF", "#0F2A7A", "F", "fr.francetv.pluzz"),
                app("tf1", "TF1+", "Direct et replay TF1", "#2D9CFF", "#0A3B73", "1", "fr.tf1.mytf1", "com.tf1.tf1plus"),
                app("6play", "6play", "Direct et replay M6", "#FFC400", "#8C5A00", "6", "fr.m6.m6replay"),
                app("arte", "Arte", "Culture et documentaires", "#FF6A00", "#7A2A00", "A", "tv.arte.plus7"),
                app("molotov", "Molotov", "Toute la TV en un seul endroit", "#17E3B4", "#06594A", "M", "tv.molotov.app"),
                app("freetv", "Free TV", "Vos chaînes Free", "#CD1E25", "#4A0508", "F", "fr.freebox.tv", "fr.freebox.player"),
            ),
        ),
        Row(
            "MUSIQUE & MÉDIAS", listOf(
                app("spotify", "Spotify", "Musique et podcasts", "#1ED760", "#0A5A26", "S", "com.spotify.tv.android"),
                app("ytm", "YouTube Music", "Musique en streaming", "#FF3355", "#5A0A1A", "♪", "com.google.android.youtube.tvmusic"),
                app("kodi", "Kodi", "Centre multimédia", "#1FB6EA", "#0A4A66", "K", "org.xbmc.kodi"),
                app("plex", "Plex", "Votre bibliothèque", "#F2A900", "#7A4F00", "P", "com.plexapp.android"),
                app("vlc", "VLC", "Lecteur universel", "#FF8A00", "#7A3F00", "V", "org.videolan.vlc"),
            ),
        ),
        Row(
            "SYSTÈME", listOf(
                sys("all", "Toutes les apps", "Ouvrir la bibliothèque", "#7C4DFF", "#2A1670", "▦", SystemAction.ALL_APPS),
                sys("settings", "Paramètres", "Réglages de la box", "#5C6BC0", "#1A2060", "⚙", SystemAction.SETTINGS),
                sys("wifi", "Wi-Fi", "Réseau et connexion", "#26C6DA", "#0A5560", "◠", SystemAction.WIFI),
                sys("bt", "Bluetooth", "Manettes et casques", "#3F8CFF", "#0F2F7A", "B", SystemAction.BLUETOOTH),
                sys("display", "Affichage", "Résolution et image", "#AB47BC", "#4A1458", "◐", SystemAction.DISPLAY),
                sys("sound", "Son", "Audio et HDMI", "#EC407A", "#6A1030", "♬", SystemAction.SOUND),
                app("store", "Play Store", "Installer des applications", "#34A853", "#0F4A22", "▷", "com.android.vending"),
            ),
        ),
    )

    fun resolve(pm: PackageManager, t: Tile) {
        t.launchIntent = null; t.pkg = null; t.banner = null; t.icon = null
        for (p in t.pkgs) {
            val i = pm.getLeanbackLaunchIntentForPackage(p) ?: pm.getLaunchIntentForPackage(p)
            if (i != null) {
                i.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_RESET_TASK_IF_NEEDED)
                t.pkg = p
                t.launchIntent = i
                t.banner = runCatching { pm.getApplicationBanner(p) }.getOrNull()
                t.icon = runCatching { pm.getApplicationIcon(p) }.getOrNull()
                break
            }
        }
    }

    private val palette = listOf(
        "#7C4DFF" to "#2A1670", "#00B8D4" to "#00424D", "#FF4081" to "#6A0F30", "#FFAB00" to "#6A4500",
        "#00C853" to "#0A4A24", "#448AFF" to "#10306E", "#E040FB" to "#4A1158", "#FF6E40" to "#6A2410",
    )

    fun allApps(ctx: Context): List<Tile> {
        val pm = ctx.packageManager
        val seen = HashSet<String>()
        val out = ArrayList<Tile>()
        for (cat in listOf(Intent.CATEGORY_LEANBACK_LAUNCHER, Intent.CATEGORY_LAUNCHER)) {
            val q = Intent(Intent.ACTION_MAIN).addCategory(cat)
            val list = runCatching { pm.queryIntentActivities(q, 0) }.getOrDefault(emptyList())
            for (ri in list) {
                val p = ri.activityInfo.packageName
                if (p == ctx.packageName || !seen.add(p)) continue
                val label = ri.loadLabel(pm).toString()
                val (a, b) = palette[(label.hashCode() and 0x7fffffff) % palette.size]
                val intent = Intent(Intent.ACTION_MAIN).addCategory(cat)
                    .setClassName(p, ri.activityInfo.name)
                    .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_RESET_TASK_IF_NEEDED)
                out += Tile(p, label, "", c(a), c(b), label.firstOrNull()?.uppercase() ?: "?").also {
                    it.pkg = p
                    it.launchIntent = intent
                    it.banner = runCatching { ri.activityInfo.loadBanner(pm) }.getOrNull()
                    it.icon = runCatching { ri.loadIcon(pm) }.getOrNull()
                }
            }
        }
        return out.sortedBy { it.label.lowercase() }
    }

    fun systemIntent(a: SystemAction): Intent = Intent(
        when (a) {
            SystemAction.SETTINGS -> Settings.ACTION_SETTINGS
            SystemAction.WIFI -> Settings.ACTION_WIFI_SETTINGS
            SystemAction.BLUETOOTH -> Settings.ACTION_BLUETOOTH_SETTINGS
            SystemAction.DISPLAY -> Settings.ACTION_DISPLAY_SETTINGS
            SystemAction.SOUND -> Settings.ACTION_SOUND_SETTINGS
            SystemAction.ALL_APPS -> Settings.ACTION_SETTINGS
        },
    ).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)

    fun storeIntent(pkg: String): Intent =
        Intent(Intent.ACTION_VIEW, Uri.parse("market://details?id=$pkg")).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
}
