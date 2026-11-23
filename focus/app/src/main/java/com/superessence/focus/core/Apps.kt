package com.superessence.focus.core

import android.content.Context
import android.content.Intent
import android.graphics.drawable.Drawable
import android.media.AudioManager
import android.telecom.TelecomManager
import android.view.inputmethod.InputMethodManager

data class AppInfo(val pkg: String, val label: String, val icon: Drawable?, val auto: Boolean)

/** Détermine quelles applications restent utilisables pendant une session bloquante. */
object Apps {
    private var cache: Set<String> = emptySet()
    private var cacheAt = 0L
    private var dialers: Set<String> = emptySet()

    /** Paquets système indispensables (barre d'état, écran d'accueil, clavier). */
    private fun base(c: Context): Set<String> {
        val now = System.currentTimeMillis()
        if (now - cacheAt < 60_000 && cache.isNotEmpty()) return cache
        val pm = c.packageManager
        val s = mutableSetOf(c.packageName, "android", "com.android.systemui")
        pm.queryIntentActivities(Intent(Intent.ACTION_MAIN).addCategory(Intent.CATEGORY_HOME), 0)
            .forEach { s += it.activityInfo.packageName }
        runCatching {
            c.getSystemService(InputMethodManager::class.java).enabledInputMethodList.forEach { s += it.packageName }
        }
        s += calculators(c)
        dialers = buildSet {
            add("com.android.server.telecom"); add("com.android.incallui"); add("com.samsung.android.incallui")
            add("com.google.android.dialer"); add("com.android.dialer"); add("com.samsung.android.dialer")
            runCatching { c.getSystemService(TelecomManager::class.java).defaultDialerPackage?.let { add(it) } }
            pm.queryIntentActivities(Intent(Intent.ACTION_DIAL), 0).forEach { add(it.activityInfo.packageName) }
        }
        cache = s; cacheAt = now
        return s
    }

    fun invalidate() { cacheAt = 0 }

    fun calculators(c: Context): Set<String> {
        val pm = c.packageManager
        val s = mutableSetOf<String>()
        pm.queryIntentActivities(Intent(Intent.ACTION_MAIN).addCategory(Intent.CATEGORY_APP_CALCULATOR), 0)
            .forEach { s += it.activityInfo.packageName }
        launchable(c).forEach { r ->
            val p = r.activityInfo.packageName
            val l = r.loadLabel(pm).toString().lowercase()
            if ("calculator" in p || "calculat" in l || "calc" == l) s += p
        }
        return s
    }

    private fun launchable(c: Context) =
        c.packageManager.queryIntentActivities(Intent(Intent.ACTION_MAIN).addCategory(Intent.CATEGORY_LAUNCHER), 0)

    private fun inCall(c: Context): Boolean {
        val m = runCatching { c.getSystemService(AudioManager::class.java).mode }.getOrDefault(AudioManager.MODE_NORMAL)
        return m == AudioManager.MODE_IN_CALL || m == AudioManager.MODE_RINGTONE || m == AudioManager.MODE_IN_COMMUNICATION
    }

    fun isDialer(c: Context, pkg: String): Boolean { base(c); return pkg in dialers }

    /** Peut-on ouvrir cette application pendant la session ? */
    fun allowed(c: Context, pkg: String): Boolean {
        if (pkg in base(c) || pkg in Store.allowedApps) return true
        if (pkg in dialers && (inCall(c) || System.currentTimeMillis() < Store.callGraceUntil)) return true
        return false
    }

    /** Peut-on laisser passer les notifications de cette application ? */
    fun notifAllowed(c: Context, pkg: String): Boolean =
        pkg in base(c) || pkg in Store.allowedApps || pkg in dialers

    /** Liste des applications installées (pour l'écran « Applis autorisées »). */
    fun installed(c: Context): List<AppInfo> {
        val pm = c.packageManager
        val calc = calculators(c)
        return launchable(c)
            .distinctBy { it.activityInfo.packageName }
            .filter { it.activityInfo.packageName != c.packageName }
            .map {
                val p = it.activityInfo.packageName
                AppInfo(p, it.loadLabel(pm).toString(), runCatching { it.loadIcon(pm) }.getOrNull(), p in calc)
            }
            .sortedWith(compareByDescending<AppInfo> { it.auto }.thenBy { it.label.lowercase() })
    }

    /** Au premier lancement, pré-autorise les applis dont le nom contient « ATI ». */
    fun initDefaults(c: Context) {
        if (Store.allowedInitialized) return
        val pm = c.packageManager
        val ati = launchable(c).filter {
            val l = it.loadLabel(pm).toString()
            Regex("\\bATI\\b").containsMatchIn(l) || it.activityInfo.packageName.lowercase().contains(".ati")
        }.map { it.activityInfo.packageName }
        Store.allowedApps = Store.allowedApps + ati
        Store.allowedInitialized = true
    }

    fun isInstalled(c: Context, pkg: String) =
        runCatching { c.packageManager.getPackageInfo(pkg, 0); true }.getOrDefault(false)

    fun launch(c: Context, pkg: String) {
        c.packageManager.getLaunchIntentForPackage(pkg)?.let {
            c.startActivity(it.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK))
        }
    }

}
