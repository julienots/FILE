package com.superessence.voxea.core

import de.sciss.jump3r.mp3.BitStream
import de.sciss.jump3r.mp3.GainAnalysis
import de.sciss.jump3r.mp3.GetAudio
import de.sciss.jump3r.mp3.ID3Tag
import de.sciss.jump3r.mp3.Lame
import de.sciss.jump3r.mp3.MPEGMode
import de.sciss.jump3r.mp3.Parse
import de.sciss.jump3r.mp3.Presets
import de.sciss.jump3r.mp3.PsyModel
import de.sciss.jump3r.mp3.Quantize
import de.sciss.jump3r.mp3.QuantizePVT
import de.sciss.jump3r.mp3.Reservoir
import de.sciss.jump3r.mp3.Takehiro
import de.sciss.jump3r.mp3.VBRTag
import de.sciss.jump3r.mp3.VbrMode
import de.sciss.jump3r.mp3.Version
import de.sciss.jump3r.mpg.Common
import de.sciss.jump3r.mpg.Interface
import de.sciss.jump3r.mpg.MPGLib
import java.io.OutputStream

/**
 * Encodeur MP3 (LAME porté en Java pur, jump3r, LGPL). On câble directement les modules bas niveau :
 * l'API « LameEncoder » de jump3r dépend de javax.sound, absent d'Android.
 */
object Mp3 {
    fun encode(pcm: FloatArray, sampleRate: Int, out: OutputStream, bitrate: Int = 128, onProgress: (Float) -> Unit = {}) {
        val lame = Lame(); val gaud = GetAudio(); val ga = GainAnalysis(); val bs = BitStream()
        val p = Presets(); val qupvt = QuantizePVT(); val qu = Quantize(); val vbr = VBRTag()
        val ver = Version(); val id3 = ID3Tag(); val rv = Reservoir(); val tak = Takehiro()
        val parse = Parse(); val mpg = MPGLib(); val intf = Interface(); val common = Common()
        lame.setModules(ga, bs, p, qupvt, qu, vbr, ver, id3, mpg)
        bs.setModules(ga, mpg, ver, vbr)
        id3.setModules(bs, ver)
        p.setModules(lame)
        qu.setModules(bs, rv, qupvt, tak)
        qupvt.setModules(tak, rv, lame.enc.psy)
        rv.setModules(bs)
        tak.setModules(qupvt)
        vbr.setModules(lame, bs, ver)
        gaud.setModules(parse, mpg)
        parse.setModules(ver, id3, p)
        mpg.setModules(intf, common)
        intf.setModules(vbr, common)

        val gfp = lame.lame_init()
        gfp.num_channels = 1
        gfp.in_samplerate = sampleRate
        gfp.out_samplerate = sampleRate
        gfp.mode = MPEGMode.MONO
        gfp.VBR = VbrMode.vbr_off
        gfp.brate = bitrate
        gfp.quality = 5
        gfp.bWriteVbrTag = false
        id3.id3tag_init(gfp)
        gfp.write_id3tag_automatic = false
        require(lame.lame_init_params(gfp) >= 0) { "Initialisation MP3 impossible" }

        val chunk = 4608
        val ints = IntArray(chunk)
        val buf = ByteArray(chunk * 2 + 7200)
        var i = 0
        while (i < pcm.size) {
            val n = minOf(chunk, pcm.size - i)
            for (k in 0 until n) ints[k] = (pcm[i + k].coerceIn(-1f, 1f) * Int.MAX_VALUE.toFloat()).toInt()
            val w = lame.lame_encode_buffer_int(gfp, ints, ints, n, buf, 0, buf.size)
            if (w > 0) out.write(buf, 0, w)
            i += n
            onProgress(i.toFloat() / pcm.size)
        }
        val w = lame.lame_encode_flush(gfp, buf, 0, buf.size)
        if (w > 0) out.write(buf, 0, w)
        lame.lame_close(gfp)
        out.flush()
    }
}
