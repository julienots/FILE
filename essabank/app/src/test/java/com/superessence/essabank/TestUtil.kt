package com.superessence.essabank

import android.os.Looper
import org.robolectric.Shadows.shadowOf

/** Laisse tourner le fil principal (Robolectric) jusqu'à ce que [done] soit vrai. */
fun awaitMain(timeoutMs: Long = 20_000, done: () -> Boolean) {
    val end = System.currentTimeMillis() + timeoutMs
    while (!done()) {
        check(System.currentTimeMillis() < end) { "Délai dépassé" }
        shadowOf(Looper.getMainLooper()).idle()
        Thread.sleep(10)
    }
}
