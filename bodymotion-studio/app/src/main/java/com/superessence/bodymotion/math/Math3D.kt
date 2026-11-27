package com.superessence.bodymotion.math

import kotlin.math.abs
import kotlin.math.acos
import kotlin.math.asin
import kotlin.math.atan2
import kotlin.math.cos
import kotlin.math.sin
import kotlin.math.sqrt

/** Vecteur 3D immuable (double précision pour les calculs de squelette). */
data class Vec3(val x: Double, val y: Double, val z: Double) {
    operator fun plus(o: Vec3) = Vec3(x + o.x, y + o.y, z + o.z)
    operator fun minus(o: Vec3) = Vec3(x - o.x, y - o.y, z - o.z)
    operator fun times(s: Double) = Vec3(x * s, y * s, z * s)
    operator fun div(s: Double) = Vec3(x / s, y / s, z / s)
    operator fun unaryMinus() = Vec3(-x, -y, -z)
    infix fun dot(o: Vec3) = x * o.x + y * o.y + z * o.z
    infix fun cross(o: Vec3) = Vec3(y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x)
    fun length() = sqrt(x * x + y * y + z * z)
    fun normalized(): Vec3 { val l = length(); return if (l < 1e-12) ZERO else this / l }
    fun lerp(o: Vec3, t: Double) = Vec3(x + (o.x - x) * t, y + (o.y - y) * t, z + (o.z - z) * t)
    fun isFinite() = x.isFinite() && y.isFinite() && z.isFinite()

    companion object {
        val ZERO = Vec3(0.0, 0.0, 0.0)
        val ONE = Vec3(1.0, 1.0, 1.0)
        val X = Vec3(1.0, 0.0, 0.0)
        val Y = Vec3(0.0, 1.0, 0.0)
        val Z = Vec3(0.0, 0.0, 1.0)
    }
}

/** Quaternion unitaire (x, y, z, w), convention glTF. */
data class Quat(val x: Double, val y: Double, val z: Double, val w: Double) {
    operator fun times(o: Quat) = Quat(
        w * o.x + x * o.w + y * o.z - z * o.y,
        w * o.y - x * o.z + y * o.w + z * o.x,
        w * o.z + x * o.y - y * o.x + z * o.w,
        w * o.w - x * o.x - y * o.y - z * o.z,
    )

    fun conjugate() = Quat(-x, -y, -z, w)
    fun inverse(): Quat { val n = x * x + y * y + z * z + w * w; return Quat(-x / n, -y / n, -z / n, w / n) }
    fun dot(o: Quat) = x * o.x + y * o.y + z * o.z + w * o.w
    fun normalized(): Quat {
        val l = sqrt(x * x + y * y + z * z + w * w)
        return if (l < 1e-12) IDENTITY else Quat(x / l, y / l, z / l, w / l)
    }
    fun negate() = Quat(-x, -y, -z, -w)

    fun rotate(v: Vec3): Vec3 {
        val u = Vec3(x, y, z)
        val t = (u cross v) * 2.0
        return v + t * w + (u cross t)
    }

    /** Angle de rotation (radians, 0..π). */
    fun angle(): Double = 2.0 * acos(abs(w).coerceAtMost(1.0))

    fun slerp(o: Quat, t: Double): Quat {
        var b = o
        var d = dot(b)
        if (d < 0) { b = b.negate(); d = -d }
        if (d > 0.9995) {
            return Quat(x + (b.x - x) * t, y + (b.y - y) * t, z + (b.z - z) * t, w + (b.w - w) * t).normalized()
        }
        val th0 = acos(d.coerceIn(-1.0, 1.0))
        val th = th0 * t
        val s0 = cos(th) - d * sin(th) / sin(th0)
        val s1 = sin(th) / sin(th0)
        return Quat(x * s0 + b.x * s1, y * s0 + b.y * s1, z * s0 + b.z * s1, w * s0 + b.w * s1).normalized()
    }

    fun toMat3(): Mat3 {
        val xx = x * x; val yy = y * y; val zz = z * z
        val xy = x * y; val xz = x * z; val yz = y * z
        val wx = w * x; val wy = w * y; val wz = w * z
        return Mat3(
            1 - 2 * (yy + zz), 2 * (xy - wz), 2 * (xz + wy),
            2 * (xy + wz), 1 - 2 * (xx + zz), 2 * (yz - wx),
            2 * (xz - wy), 2 * (yz + wx), 1 - 2 * (xx + yy),
        )
    }

    /**
     * Angles d'Euler (degrés) pour l'ordre « XYZ » au sens FBX/BVH : R = Rz * Ry * Rx
     * (rotation X appliquée en premier).
     */
    fun toEulerXYZDegrees(): Vec3 {
        val m = toMat3()
        val sy = (-m.m20).coerceIn(-1.0, 1.0)
        val ry = asin(sy)
        val rx: Double
        val rz: Double
        if (abs(sy) < 0.99999) {
            rx = atan2(m.m21, m.m22)
            rz = atan2(m.m10, m.m00)
        } else {
            rx = atan2(-m.m12, m.m11)
            rz = 0.0
        }
        return Vec3(Math.toDegrees(rx), Math.toDegrees(ry), Math.toDegrees(rz))
    }

    /** Angles d'Euler (degrés) pour R = Rz * Rx * Ry (canaux BVH « Zrotation Xrotation Yrotation »). */
    fun toEulerZXYDegrees(): Vec3 {
        val m = toMat3()
        val sx = m.m21.coerceIn(-1.0, 1.0)
        val rx = asin(sx)
        val ry: Double
        val rz: Double
        if (abs(sx) < 0.99999) {
            ry = atan2(-m.m20, m.m22)
            rz = atan2(-m.m01, m.m11)
        } else {
            ry = atan2(m.m02, m.m00)
            rz = 0.0
        }
        return Vec3(Math.toDegrees(rx), Math.toDegrees(ry), Math.toDegrees(rz))
    }

    companion object {
        val IDENTITY = Quat(0.0, 0.0, 0.0, 1.0)

        fun axisAngle(axis: Vec3, angle: Double): Quat {
            val a = axis.normalized()
            val s = sin(angle / 2)
            return Quat(a.x * s, a.y * s, a.z * s, cos(angle / 2))
        }

        /** Rotation minimale qui amène la direction [from] sur [to]. */
        fun fromTo(from: Vec3, to: Vec3): Quat {
            val f = from.normalized()
            val t = to.normalized()
            if (f == Vec3.ZERO || t == Vec3.ZERO) return IDENTITY
            val d = f dot t
            if (d > 0.999999) return IDENTITY
            if (d < -0.999999) {
                var axis = Vec3.X cross f
                if (axis.length() < 1e-6) axis = Vec3.Y cross f
                return axisAngle(axis, Math.PI)
            }
            val c = f cross t
            return Quat(c.x, c.y, c.z, 1 + d).normalized()
        }

        fun fromEulerXYZDegrees(e: Vec3): Quat {
            val qx = axisAngle(Vec3.X, Math.toRadians(e.x))
            val qy = axisAngle(Vec3.Y, Math.toRadians(e.y))
            val qz = axisAngle(Vec3.Z, Math.toRadians(e.z))
            return (qz * qy * qx).normalized()
        }

        fun fromMat3(m: Mat3): Quat {
            val tr = m.m00 + m.m11 + m.m22
            val q = if (tr > 0) {
                val s = sqrt(tr + 1.0) * 2
                Quat((m.m21 - m.m12) / s, (m.m02 - m.m20) / s, (m.m10 - m.m01) / s, 0.25 * s)
            } else if (m.m00 > m.m11 && m.m00 > m.m22) {
                val s = sqrt(1.0 + m.m00 - m.m11 - m.m22) * 2
                Quat(0.25 * s, (m.m01 + m.m10) / s, (m.m02 + m.m20) / s, (m.m21 - m.m12) / s)
            } else if (m.m11 > m.m22) {
                val s = sqrt(1.0 + m.m11 - m.m00 - m.m22) * 2
                Quat((m.m01 + m.m10) / s, 0.25 * s, (m.m12 + m.m21) / s, (m.m02 - m.m20) / s)
            } else {
                val s = sqrt(1.0 + m.m22 - m.m00 - m.m11) * 2
                Quat((m.m02 + m.m20) / s, (m.m12 + m.m21) / s, 0.25 * s, (m.m10 - m.m01) / s)
            }
            return q.normalized()
        }

        /** Repère orthonormé construit à partir d'un axe X (gauche du corps) et d'un axe Y (haut). */
        fun fromBasis(xAxis: Vec3, yHint: Vec3): Quat {
            val x = xAxis.normalized()
            var y = (yHint - x * (yHint dot x)).normalized()
            if (y == Vec3.ZERO) y = Vec3.Y
            val z = x cross y
            return fromMat3(Mat3(x.x, y.x, z.x, x.y, y.y, z.y, x.z, y.z, z.z))
        }
    }
}

/** Matrice 3x3 en lignes (m[ligne][colonne]). */
data class Mat3(
    val m00: Double, val m01: Double, val m02: Double,
    val m10: Double, val m11: Double, val m12: Double,
    val m20: Double, val m21: Double, val m22: Double,
)

/**
 * Matrice 4x4 stockée en colonnes (column-major, comme glTF et Filament).
 * Index : m[col * 4 + ligne].
 */
class Mat4(val m: DoubleArray = DoubleArray(16).also { it[0] = 1.0; it[5] = 1.0; it[10] = 1.0; it[15] = 1.0 }) {
    operator fun get(row: Int, col: Int) = m[col * 4 + row]

    operator fun times(o: Mat4): Mat4 {
        val r = DoubleArray(16)
        for (c in 0 until 4) for (rw in 0 until 4) {
            var s = 0.0
            for (k in 0 until 4) s += this[rw, k] * o[k, c]
            r[c * 4 + rw] = s
        }
        return Mat4(r)
    }

    fun transformPoint(p: Vec3) = Vec3(
        this[0, 0] * p.x + this[0, 1] * p.y + this[0, 2] * p.z + this[0, 3],
        this[1, 0] * p.x + this[1, 1] * p.y + this[1, 2] * p.z + this[1, 3],
        this[2, 0] * p.x + this[2, 1] * p.y + this[2, 2] * p.z + this[2, 3],
    )

    fun transformDir(p: Vec3) = Vec3(
        this[0, 0] * p.x + this[0, 1] * p.y + this[0, 2] * p.z,
        this[1, 0] * p.x + this[1, 1] * p.y + this[1, 2] * p.z,
        this[2, 0] * p.x + this[2, 1] * p.y + this[2, 2] * p.z,
    )

    fun translation() = Vec3(m[12], m[13], m[14])

    /** Décomposition TRS (échelle positive supposée). */
    fun decompose(): Trs {
        val t = translation()
        val cx = Vec3(m[0], m[1], m[2]); val cy = Vec3(m[4], m[5], m[6]); val cz = Vec3(m[8], m[9], m[10])
        var sx = cx.length(); val sy = cy.length(); val sz = cz.length()
        val det = cx dot (cy cross cz)
        if (det < 0) sx = -sx
        val r = Mat3(
            m[0] / sx, m[4] / sy, m[8] / sz,
            m[1] / sx, m[5] / sy, m[9] / sz,
            m[2] / sx, m[6] / sy, m[10] / sz,
        )
        return Trs(t, Quat.fromMat3(r), Vec3(sx, sy, sz))
    }

    fun inverse(): Mat4 {
        val a = m
        val inv = DoubleArray(16)
        inv[0] = a[5] * a[10] * a[15] - a[5] * a[11] * a[14] - a[9] * a[6] * a[15] + a[9] * a[7] * a[14] + a[13] * a[6] * a[11] - a[13] * a[7] * a[10]
        inv[4] = -a[4] * a[10] * a[15] + a[4] * a[11] * a[14] + a[8] * a[6] * a[15] - a[8] * a[7] * a[14] - a[12] * a[6] * a[11] + a[12] * a[7] * a[10]
        inv[8] = a[4] * a[9] * a[15] - a[4] * a[11] * a[13] - a[8] * a[5] * a[15] + a[8] * a[7] * a[13] + a[12] * a[5] * a[11] - a[12] * a[7] * a[9]
        inv[12] = -a[4] * a[9] * a[14] + a[4] * a[10] * a[13] + a[8] * a[5] * a[14] - a[8] * a[6] * a[13] - a[12] * a[5] * a[10] + a[12] * a[6] * a[9]
        inv[1] = -a[1] * a[10] * a[15] + a[1] * a[11] * a[14] + a[9] * a[2] * a[15] - a[9] * a[3] * a[14] - a[13] * a[2] * a[11] + a[13] * a[3] * a[10]
        inv[5] = a[0] * a[10] * a[15] - a[0] * a[11] * a[14] - a[8] * a[2] * a[15] + a[8] * a[3] * a[14] + a[12] * a[2] * a[11] - a[12] * a[3] * a[10]
        inv[9] = -a[0] * a[9] * a[15] + a[0] * a[11] * a[13] + a[8] * a[1] * a[15] - a[8] * a[3] * a[13] - a[12] * a[1] * a[11] + a[12] * a[3] * a[9]
        inv[13] = a[0] * a[9] * a[14] - a[0] * a[10] * a[13] - a[8] * a[1] * a[14] + a[8] * a[2] * a[13] + a[12] * a[1] * a[10] - a[12] * a[2] * a[9]
        inv[2] = a[1] * a[6] * a[15] - a[1] * a[7] * a[14] - a[5] * a[2] * a[15] + a[5] * a[3] * a[14] + a[13] * a[2] * a[7] - a[13] * a[3] * a[6]
        inv[6] = -a[0] * a[6] * a[15] + a[0] * a[7] * a[14] + a[4] * a[2] * a[15] - a[4] * a[3] * a[14] - a[12] * a[2] * a[7] + a[12] * a[3] * a[6]
        inv[10] = a[0] * a[5] * a[15] - a[0] * a[7] * a[13] - a[4] * a[1] * a[15] + a[4] * a[3] * a[13] + a[12] * a[1] * a[7] - a[12] * a[3] * a[5]
        inv[14] = -a[0] * a[5] * a[14] + a[0] * a[6] * a[13] + a[4] * a[1] * a[14] - a[4] * a[2] * a[13] - a[12] * a[1] * a[6] + a[12] * a[2] * a[5]
        inv[3] = -a[1] * a[6] * a[11] + a[1] * a[7] * a[10] + a[5] * a[2] * a[11] - a[5] * a[3] * a[10] - a[9] * a[2] * a[7] + a[9] * a[3] * a[6]
        inv[7] = a[0] * a[6] * a[11] - a[0] * a[7] * a[10] - a[4] * a[2] * a[11] + a[4] * a[3] * a[10] + a[8] * a[2] * a[7] - a[8] * a[3] * a[6]
        inv[11] = -a[0] * a[5] * a[11] + a[0] * a[7] * a[9] + a[4] * a[1] * a[11] - a[4] * a[3] * a[9] - a[8] * a[1] * a[7] + a[8] * a[3] * a[5]
        inv[15] = a[0] * a[5] * a[10] - a[0] * a[6] * a[9] - a[4] * a[1] * a[10] + a[4] * a[2] * a[9] + a[8] * a[1] * a[6] - a[8] * a[2] * a[5]
        var det = a[0] * inv[0] + a[1] * inv[4] + a[2] * inv[8] + a[3] * inv[12]
        if (abs(det) < 1e-20) return Mat4()
        det = 1.0 / det
        for (i in 0 until 16) inv[i] *= det
        return Mat4(inv)
    }

    fun toFloatArray() = FloatArray(16) { m[it].toFloat() }

    companion object {
        fun identity() = Mat4()
        fun fromTrs(t: Vec3, r: Quat, s: Vec3): Mat4 {
            val rm = r.toMat3()
            val a = DoubleArray(16)
            a[0] = rm.m00 * s.x; a[1] = rm.m10 * s.x; a[2] = rm.m20 * s.x
            a[4] = rm.m01 * s.y; a[5] = rm.m11 * s.y; a[6] = rm.m21 * s.y
            a[8] = rm.m02 * s.z; a[9] = rm.m12 * s.z; a[10] = rm.m22 * s.z
            a[12] = t.x; a[13] = t.y; a[14] = t.z; a[15] = 1.0
            return Mat4(a)
        }
        fun fromColumnMajor(f: List<Double>) = Mat4(DoubleArray(16) { f[it] })
    }
}

data class Trs(val t: Vec3, val r: Quat, val s: Vec3) {
    fun toMat4() = Mat4.fromTrs(t, r, s)
}
