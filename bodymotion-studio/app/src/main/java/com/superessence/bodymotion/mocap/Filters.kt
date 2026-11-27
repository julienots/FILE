package com.superessence.bodymotion.mocap

import com.superessence.bodymotion.math.Vec3
import kotlin.math.PI
import kotlin.math.abs

/** Filtre « One Euro » : lisse les tremblements à l'arrêt sans retarder les mouvements rapides. */
class OneEuro(private val minCutoff: Double, private val beta: Double, private val dCutoff: Double = 1.0) {
    private var x = 0.0; private var dx = 0.0; private var t = -1.0
    private fun alpha(cutoff: Double, dt: Double): Double { val tau = 1.0 / (2 * PI * cutoff); return 1.0 / (1.0 + tau / dt) }
    fun filter(v: Double, timeS: Double): Double {
        if (t < 0) { x = v; t = timeS; return v }
        val dt = (timeS - t).coerceAtLeast(1e-3); t = timeS
        val d = (v - x) / dt
        dx += alpha(dCutoff, dt) * (d - dx)
        val cutoff = minCutoff + beta * abs(dx)
        x += alpha(cutoff, dt) * (v - x)
        return x
    }
    fun reset() { t = -1.0 }
}

/** Filtre un ensemble de points 3D (un filtre par coordonnée). */
class PointSetFilter(n: Int, minCutoff: Double, beta: Double) {
    private val f = Array(n * 3) { OneEuro(minCutoff, beta) }
    fun apply(p: Array<Vec3>, timeS: Double): Array<Vec3> = Array(p.size) { i ->
        Vec3(f[i * 3].filter(p[i].x, timeS), f[i * 3 + 1].filter(p[i].y, timeS), f[i * 3 + 2].filter(p[i].z, timeS))
    }
    fun reset() = f.forEach { it.reset() }
}

object SmoothingPresets {
    /** (minCutoff, beta) pour faible / moyen / fort. */
    fun params(level: Int) = when (level) { 0 -> 2.5 to 0.8; 2 -> 0.6 to 0.25; else -> 1.2 to 0.5 }
}
