package com.superessence.essabank.core.pdf

import java.io.ByteArrayOutputStream
import java.util.Locale

/**
 * Générateur PDF minimal (PDF 1.4, polices standard Helvetica, encodage WinAnsi),
 * sans dépendance : utilisable sur Android comme sur JVM et testable unitairement.
 */
class PdfDocumentBuilder(val width: Float = 595f, val height: Float = 842f) {
    private val pages = ArrayList<PdfPage>()

    fun page(block: PdfPage.() -> Unit): PdfPage = PdfPage(width, height).also { it.block(); pages += it }

    val pageCount: Int get() = pages.size

    fun build(title: String = "ESSABANK"): ByteArray {
        val out = ByteArrayOutputStream()
        val offsets = ArrayList<Int>()
        fun write(s: String) = out.write(s.toByteArray(Charsets.ISO_8859_1))
        fun obj(n: Int, body: () -> Unit) { offsets += out.size(); write("$n 0 obj\n"); body(); write("\nendobj\n") }

        write("%PDF-1.4\n%âãÏÓ\n")
        // 1 catalogue, 2 pages, 3 police normale, 4 police grasse, 5 infos, puis (contenu, page) par page.
        val first = 6
        val kids = pages.indices.joinToString(" ") { "${first + it * 2 + 1} 0 R" }
        obj(1) { write("<< /Type /Catalog /Pages 2 0 R >>") }
        obj(2) { write("<< /Type /Pages /Kids [$kids] /Count ${pages.size} >>") }
        obj(3) { write("<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>") }
        obj(4) { write("<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica-Bold /Encoding /WinAnsiEncoding >>") }
        obj(5) {
            write("<< /Title "); out.write(pdfString(title)); write(" /Producer (ESSABANK - Superessence Studio) /Subject (Simulation - aucune valeur reelle) >>")
        }
        pages.forEachIndexed { i, p ->
            val content = p.content()
            obj(first + i * 2) {
                write("<< /Length ${content.size} >>\nstream\n"); out.write(content); write("\nendstream")
            }
            obj(first + i * 2 + 1) {
                write("<< /Type /Page /Parent 2 0 R /MediaBox [0 0 ${fmt(width)} ${fmt(height)}] ")
                write("/Resources << /Font << /F1 3 0 R /F2 4 0 R >> /ExtGState << /GS1 << /ca 0.08 /CA 0.08 >> >> >> ")
                write("/Contents ${first + i * 2} 0 R >>")
            }
        }
        val xref = out.size()
        write("xref\n0 ${offsets.size + 1}\n0000000000 65535 f \n")
        for (o in offsets) write(String.format(Locale.ROOT, "%010d 00000 n \n", o))
        write("trailer\n<< /Size ${offsets.size + 1} /Root 1 0 R /Info 5 0 R >>\nstartxref\n$xref\n%%EOF\n")
        return out.toByteArray()
    }

    companion object {
        internal fun fmt(v: Float): String = String.format(Locale.ROOT, "%.2f", v)

        /** Encode une chaîne PDF littérale en WinAnsi, avec échappement. */
        internal fun pdfString(text: String): ByteArray {
            val out = ByteArrayOutputStream()
            out.write('('.code)
            for (b in toWinAnsi(text)) {
                val c = b.toInt() and 0xFF
                if (c == '('.code || c == ')'.code || c == '\\'.code) out.write('\\'.code)
                out.write(c)
            }
            out.write(')'.code)
            return out.toByteArray()
        }

        fun toWinAnsi(text: String): ByteArray {
            val normalized = text
                .replace('−', '-').replace(' ', ' ').replace(' ', ' ')
                .replace('→', '>').replace('←', '<').replace('…', '.')
            // Les caractères hors WinAnsi sont remplacés par « ? » par l'encodeur.
            return normalized.toByteArray(charset("windows-1252"))
        }

        /** Largeur approximative d'un texte en Helvetica (métriques AFM simplifiées). */
        fun textWidth(text: String, size: Float, bold: Boolean = false): Float {
            var units = 0
            for (ch in text) units += charWidth(ch, bold)
            return units * size / 1000f
        }

        private fun charWidth(c: Char, bold: Boolean): Int = when (c) {
            ' ' -> 278
            in '0'..'9' -> 556
            ',', '.', ':', ';' -> if (bold) 333 else 278
            '-', '−' -> 333
            '+', '=', '<', '>' -> 584
            '(', ')', '[', ']', '/' -> 333
            '\'', '"' -> 191
            '%' -> 889
            '€' -> 556
            '•' -> 350
            'i', 'j', 'l' -> if (bold) 278 else 222
            'f', 't', 'r', 'I' -> if (bold) 333 else 278
            'm' -> 833
            'w' -> if (bold) 778 else 722
            'M' -> 833
            'W' -> 944
            in 'a'..'z' -> if (bold) 590 else 556
            in 'A'..'Z' -> if (bold) 722 else 667
            else -> if (c.isLetter()) (if (bold) 600 else 556) else 556
        }
    }
}

class PdfPage(val width: Float, val height: Float) {
    private val ops = StringBuilder()
    private val raw = ByteArrayOutputStream()

    private fun flush() { if (ops.isNotEmpty()) { raw.write(ops.toString().toByteArray(Charsets.ISO_8859_1)); ops.setLength(0) } }

    fun color(r: Int, g: Int, b: Int, stroke: Boolean = false) {
        ops.append("${c(r)} ${c(g)} ${c(b)} ${if (stroke) "RG" else "rg"}\n")
    }

    private fun c(v: Int) = PdfDocumentBuilder.fmt(v / 255f)

    fun rect(x: Float, y: Float, w: Float, h: Float, r: Int, g: Int, b: Int) {
        color(r, g, b)
        ops.append("${PdfDocumentBuilder.fmt(x)} ${PdfDocumentBuilder.fmt(y)} ${PdfDocumentBuilder.fmt(w)} ${PdfDocumentBuilder.fmt(h)} re f\n")
    }

    fun line(x1: Float, y1: Float, x2: Float, y2: Float, r: Int = 210, g: Int = 216, b: Int = 228, width: Float = 0.6f) {
        color(r, g, b, stroke = true)
        ops.append("${PdfDocumentBuilder.fmt(width)} w ${PdfDocumentBuilder.fmt(x1)} ${PdfDocumentBuilder.fmt(y1)} m ${PdfDocumentBuilder.fmt(x2)} ${PdfDocumentBuilder.fmt(y2)} l S\n")
    }

    /** Texte ; [align] : 0 gauche, 1 centre, 2 droite par rapport à [x]. */
    fun text(x: Float, y: Float, text: String, size: Float = 10f, bold: Boolean = false, r: Int = 20, g: Int = 28, b: Int = 48, align: Int = 0) {
        val w = PdfDocumentBuilder.textWidth(text, size, bold)
        val tx = when (align) { 1 -> x - w / 2; 2 -> x - w; else -> x }
        color(r, g, b)
        ops.append("BT /${if (bold) "F2" else "F1"} ${PdfDocumentBuilder.fmt(size)} Tf ${PdfDocumentBuilder.fmt(tx)} ${PdfDocumentBuilder.fmt(y)} Td ")
        flush()
        raw.write(PdfDocumentBuilder.pdfString(text))
        ops.append(" Tj ET\n")
    }

    /** Coupe un texte trop long pour tenir dans [maxWidth]. */
    fun ellipsize(text: String, size: Float, maxWidth: Float, bold: Boolean = false): String {
        if (PdfDocumentBuilder.textWidth(text, size, bold) <= maxWidth) return text
        var t = text
        while (t.isNotEmpty() && PdfDocumentBuilder.textWidth("$t...", size, bold) > maxWidth) t = t.dropLast(1)
        return "$t..."
    }

    /** Filigrane diagonal semi-transparent. */
    fun watermark(text: String, size: Float = 64f) {
        ops.append("q /GS1 gs 0.10 0.25 0.85 rg BT /F2 ${PdfDocumentBuilder.fmt(size)} Tf 0.7071 0.7071 -0.7071 0.7071 ")
        val w = PdfDocumentBuilder.textWidth(text, size, true)
        // Centre la diagonale dans la page.
        val cx = width / 2 - w * 0.7071f / 2 + size * 0.35f
        val cy = height / 2 - w * 0.7071f / 2 - size * 0.35f
        ops.append("${PdfDocumentBuilder.fmt(cx)} ${PdfDocumentBuilder.fmt(cy)} Tm ")
        flush()
        raw.write(PdfDocumentBuilder.pdfString(text))
        ops.append(" Tj ET Q\n")
    }

    internal fun content(): ByteArray { flush(); return raw.toByteArray() }
}
