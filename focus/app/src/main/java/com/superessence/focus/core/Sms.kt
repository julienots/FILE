package com.superessence.focus.core

import android.Manifest
import android.app.NotificationManager
import android.app.PendingIntent
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.net.Uri
import android.os.Build
import android.provider.Telephony
import android.telephony.PhoneNumberUtils
import android.telephony.SmsManager
import androidx.core.app.NotificationCompat
import androidx.core.content.ContextCompat
import com.superessence.focus.FocusApp
import com.superessence.focus.MainActivity
import com.superessence.focus.R

data class Msg(val id: Long, val body: String, val date: Long, val mine: Boolean, val pending: Boolean = false)

/** Conversation SMS avec Romane, et uniquement elle. */
object Sms {
    val PERMS = arrayOf(Manifest.permission.READ_SMS, Manifest.permission.SEND_SMS, Manifest.permission.RECEIVE_SMS)

    fun granted(c: Context) = PERMS.all { ContextCompat.checkSelfPermission(c, it) == PackageManager.PERMISSION_GRANTED }

    fun isRomane(address: String?): Boolean {
        val n = Store.romaneNumber
        return n.isNotBlank() && !address.isNullOrBlank() && PhoneNumberUtils.compare(address, n)
    }

    fun load(c: Context): List<Msg> {
        if (!granted(c) || Store.romaneNumber.isBlank()) return emptyList()
        val out = ArrayList<Msg>()
        runCatching {
            c.contentResolver.query(
                Uri.parse("content://sms"), arrayOf("_id", "address", "body", "date", "type"),
                null, null, "date DESC",
            )?.use { cur ->
                var scanned = 0
                while (cur.moveToNext() && scanned < 5000 && out.size < 400) {
                    scanned++
                    if (!isRomane(cur.getString(1))) continue
                    val type = cur.getInt(4)
                    out += Msg(cur.getLong(0), cur.getString(2) ?: "", cur.getLong(3), mine = type != 1, pending = type in 4..6)
                }
            }
        }
        return out
    }

    fun send(c: Context, text: String): Boolean = runCatching {
        val sm = if (Build.VERSION.SDK_INT >= 31) c.getSystemService(SmsManager::class.java) else @Suppress("DEPRECATION") SmsManager.getDefault()
        val parts = sm.divideMessage(text)
        sm.sendMultipartTextMessage(Store.romaneNumber, null, parts, null, null)
        true
    }.getOrDefault(false)
}

/** Quand Romane écrit pendant une session, Focus. affiche lui-même la notification. */
class SmsReceiver : BroadcastReceiver() {
    override fun onReceive(c: Context, i: Intent) {
        Store.init(c)
        if (!Session.blocking) return
        val msgs = runCatching { Telephony.Sms.Intents.getMessagesFromIntent(i) }.getOrNull() ?: return
        val from = msgs.firstOrNull()?.originatingAddress
        if (!Sms.isRomane(from)) return
        val body = msgs.joinToString("") { it.messageBody ?: "" }
        val pi = PendingIntent.getActivity(
            c, 3, Intent(c, MainActivity::class.java).putExtra("tab", 2).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK),
            PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT,
        )
        val n = NotificationCompat.Builder(c, FocusApp.CH_ROMANE)
            .setSmallIcon(R.drawable.ic_stat)
            .setContentTitle(Store.romaneName)
            .setContentText(body)
            .setStyle(NotificationCompat.BigTextStyle().bigText(body))
            .setPriority(NotificationCompat.PRIORITY_HIGH)
            .setContentIntent(pi).setAutoCancel(true).build()
        runCatching { c.getSystemService(NotificationManager::class.java).notify(10, n) }
    }
}
