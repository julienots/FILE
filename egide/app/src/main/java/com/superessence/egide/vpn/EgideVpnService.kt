package com.superessence.egide.vpn

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import android.content.pm.ServiceInfo
import android.net.VpnService
import android.os.Build
import android.os.ParcelFileDescriptor
import com.superessence.egide.R
import com.superessence.egide.core.Filter
import com.superessence.egide.core.Origin
import com.superessence.egide.core.Prefs
import com.superessence.egide.core.Stats
import com.superessence.egide.ui.MainActivity
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import java.io.FileInputStream
import java.io.FileOutputStream
import java.net.DatagramPacket
import java.net.DatagramSocket
import java.net.InetAddress
import java.util.concurrent.ExecutorService
import java.util.concurrent.LinkedBlockingQueue
import java.util.concurrent.ThreadPoolExecutor
import java.util.concurrent.TimeUnit

/**
 * VPN local (aucun serveur distant) : seules les requêtes DNS du téléphone passent par lui.
 * Les domaines de pub / pop-up / malware reçoivent une réponse vide (0.0.0.0) ;
 * les autres sont transmis tels quels au serveur DNS choisi.
 * Le reste du trafic (vidéos, navigation…) ne traverse pas le VPN : aucune perte de débit.
 */
class EgideVpnService : VpnService() {

    enum class State { OFF, STARTING, ON }

    companion object {
        const val ACTION_START = "com.superessence.egide.START"
        const val ACTION_STOP = "com.superessence.egide.STOP"
        private const val CHANNEL = "protection"
        private const val NOTIF_ID = 7

        private const val DNS4 = "10.111.222.53"
        private const val ADDR4 = "10.111.222.2"
        private const val DNS6 = "fd00:e61d:5e5e::53"
        private const val ADDR6 = "fd00:e61d:5e5e::2"

        private val _state = MutableStateFlow(State.OFF)
        val state: StateFlow<State> = _state

        fun start(ctx: Context) {
            val i = Intent(ctx, EgideVpnService::class.java).setAction(ACTION_START)
            if (Build.VERSION.SDK_INT >= 26) ctx.startForegroundService(i) else ctx.startService(i)
        }

        fun stop(ctx: Context) {
            ctx.startService(Intent(ctx, EgideVpnService::class.java).setAction(ACTION_STOP))
        }
    }

    private var tun: ParcelFileDescriptor? = null
    private var reader: Thread? = null
    private var pool: ExecutorService? = null
    @Volatile private var running = false

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        if (intent?.action == ACTION_STOP) {
            Prefs.protectionWanted = false
            shutdown()
            stopForeground(STOP_FOREGROUND_REMOVE)
            stopSelf()
            return START_NOT_STICKY
        }
        goForeground()
        if (!running) startTunnel()
        return START_STICKY
    }

    override fun onRevoke() {
        // Un autre VPN a pris la place, ou l'utilisateur a coupé depuis les réglages.
        Prefs.protectionWanted = false
        shutdown()
        stopSelf()
    }

    override fun onDestroy() {
        shutdown()
        super.onDestroy()
    }

    private fun goForeground() {
        val nm = getSystemService(NotificationManager::class.java)
        if (Build.VERSION.SDK_INT >= 26) {
            nm.createNotificationChannel(
                NotificationChannel(CHANNEL, "Protection active", NotificationManager.IMPORTANCE_MIN).apply {
                    setShowBadge(false)
                }
            )
        }
        val open = PendingIntent.getActivity(
            this, 0, Intent(this, MainActivity::class.java),
            PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT
        )
        val stopPi = PendingIntent.getService(
            this, 1, Intent(this, EgideVpnService::class.java).setAction(ACTION_STOP),
            PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT
        )
        val n = Notification.Builder(this, CHANNEL)
            .setSmallIcon(R.drawable.ic_shield)
            .setContentTitle("Égide protège ce téléphone")
            .setContentText("Pubs, pop-ups et sites dangereux bloqués")
            .setContentIntent(open)
            .setOngoing(true)
            .addAction(Notification.Action.Builder(null, "Désactiver", stopPi).build())
            .build()
        if (Build.VERSION.SDK_INT >= 34) {
            startForeground(NOTIF_ID, n, ServiceInfo.FOREGROUND_SERVICE_TYPE_SPECIAL_USE)
        } else {
            startForeground(NOTIF_ID, n)
        }
    }

    private fun startTunnel() {
        _state.value = State.STARTING
        running = true
        Prefs.protectionWanted = true
        reader = Thread({ runTunnel() }, "egide-tun").also { it.start() }
    }

    private fun runTunnel() {
        try {
            Filter.ensureLoaded()
            val builder = Builder()
                .setSession("Égide")
                .setMtu(1500)
                .addAddress(ADDR4, 24)
                .addDnsServer(DNS4)
                .addRoute(DNS4, 32)
                .setBlocking(false)
            runCatching {
                builder.addAddress(ADDR6, 120)
                builder.addDnsServer(DNS6)
                builder.addRoute(DNS6, 128)
            }
            if (Build.VERSION.SDK_INT >= 29) builder.setMetered(false)
            builder.setConfigureIntent(
                PendingIntent.getActivity(this, 0, Intent(this, MainActivity::class.java), PendingIntent.FLAG_IMMUTABLE)
            )
            val fd = builder.establish() ?: throw IllegalStateException("VPN non autorisé")
            tun = fd
            pool = ThreadPoolExecutor(4, 32, 30, TimeUnit.SECONDS, LinkedBlockingQueue(512),
                ThreadPoolExecutor.DiscardOldestPolicy())
            _state.value = State.ON
            val input = FileInputStream(fd.fileDescriptor)
            val output = FileOutputStream(fd.fileDescriptor)
            val buf = ByteArray(32767)
            val pfd = android.system.StructPollfd().apply {
                this.fd = fd.fileDescriptor
                events = android.system.OsConstants.POLLIN.toShort()
            }
            while (running && tun === fd) {
                pfd.revents = 0
                val ready = try { android.system.Os.poll(arrayOf(pfd), 1000) } catch (e: android.system.ErrnoException) {
                    if (e.errno == android.system.OsConstants.EINTR) continue else throw e
                }
                if (ready == 0) continue
                val n = try { input.read(buf) } catch (e: java.io.IOException) {
                    if (e.cause is android.system.ErrnoException &&
                        (e.cause as android.system.ErrnoException).errno == android.system.OsConstants.EAGAIN) continue
                    throw e
                }
                if (n < 0) break
                if (n == 0) continue
                val pkt = Packets.parseUdp(buf, n) ?: continue
                if (pkt.dstPort != 53) continue
                val q = DnsQuery.parse(pkt.payload) ?: continue
                if (Filter.isBlocked(q.name)) {
                    Stats.record(q.name, true, Origin.SYSTEM)
                    write(output, Packets.buildReply(pkt, q.blockedResponse()))
                } else {
                    Stats.record(q.name, false, Origin.SYSTEM)
                    pool?.execute { forward(pkt, output) }
                }
            }
        } catch (e: Exception) {
            // Erreur inattendue (et non un arrêt demandé) : on libère tout.
            if (running) {
                running = false
                runCatching { tun?.close() }
                tun = null
                stopForeground(STOP_FOREGROUND_REMOVE)
                stopSelf()
            }
        } finally {
            if (tun == null || !running) {
                _state.value = State.OFF
            }
            Stats.flush()
        }
    }

    private fun forward(pkt: UdpPacket, output: FileOutputStream) {
        val servers = Prefs.upstream.v4
        for (server in servers) {
            try {
                DatagramSocket().use { sock ->
                    protect(sock)
                    sock.soTimeout = 4000
                    val addr = InetAddress.getByName(server)
                    sock.send(DatagramPacket(pkt.payload, pkt.payload.size, addr, 53))
                    val resp = ByteArray(4096)
                    val dp = DatagramPacket(resp, resp.size)
                    sock.receive(dp)
                    write(output, Packets.buildReply(pkt, resp.copyOf(dp.length)))
                }
                return
            } catch (_: Exception) {
                // on essaie le serveur suivant
            }
        }
    }

    private fun write(out: FileOutputStream, data: ByteArray) {
        synchronized(out) {
            try { out.write(data) } catch (_: Exception) {}
        }
    }

    private fun shutdown() {
        running = false
        runCatching { tun?.close() }
        tun = null
        reader?.interrupt()
        reader = null
        pool?.shutdownNow()
        pool = null
        _state.value = State.OFF
        Stats.flush()
    }
}
