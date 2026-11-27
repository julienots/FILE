package com.superessence.bodymotion.ui.components

import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.automirrored.filled.KeyboardArrowRight
import androidx.compose.material3.*
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.drawBehind
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.StrokeJoin
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import com.superessence.bodymotion.ui.theme.C
import com.superessence.bodymotion.ui.theme.T

val Shape6 = RoundedCornerShape(6.dp)
val Shape4 = RoundedCornerShape(4.dp)

@Composable
fun TopBar(title: String, onBack: (() -> Unit)?, subtitle: String? = null, actions: @Composable RowScope.() -> Unit = {}) {
    Row(
        Modifier.fillMaxWidth().background(C.Bg).statusBarsPadding().height(56.dp).padding(horizontal = 4.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        if (onBack != null) IconButton(onClick = onBack) { Icon(Icons.AutoMirrored.Filled.ArrowBack, "Retour", tint = C.Text) }
        else Spacer(Modifier.width(12.dp))
        Column(Modifier.weight(1f)) {
            Text(title, style = T.h2, maxLines = 1, overflow = TextOverflow.Ellipsis)
            if (subtitle != null) Text(subtitle, style = T.small, maxLines = 1, overflow = TextOverflow.Ellipsis)
        }
        actions()
    }
    HorizontalDivider(color = C.Border)
}

@Composable
fun SectionLabel(text: String, modifier: Modifier = Modifier) = Text(text.uppercase(), style = T.label, modifier = modifier)

@Composable
fun Group(modifier: Modifier = Modifier, content: @Composable ColumnScope.() -> Unit) {
    Column(modifier.fillMaxWidth().clip(Shape6).background(C.Panel).border(1.dp, C.Border, Shape6), content = content)
}

@Composable
fun GroupDivider() = HorizontalDivider(color = C.Border)

@Composable
fun ListRow(
    title: String, icon: ImageVector? = null, subtitle: String? = null, accent: Boolean = false,
    trailing: (@Composable () -> Unit)? = null, onClick: (() -> Unit)? = null, iconTint: Color = if (accent) C.Cyan else C.Text2,
) {
    Row(
        Modifier.fillMaxWidth().heightIn(min = 56.dp)
            .then(if (onClick != null) Modifier.clickable(role = Role.Button, onClick = onClick) else Modifier)
            .then(if (accent) Modifier.drawLeftAccent() else Modifier)
            .padding(horizontal = 14.dp, vertical = 8.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        if (icon != null) { Icon(icon, null, tint = iconTint, modifier = Modifier.size(20.dp)); Spacer(Modifier.width(14.dp)) }
        Column(Modifier.weight(1f)) {
            Text(title, style = if (accent) T.bodyM else T.body, maxLines = 1, overflow = TextOverflow.Ellipsis)
            if (subtitle != null) Text(subtitle, style = T.small, maxLines = 5, overflow = TextOverflow.Ellipsis)
        }
        if (trailing != null) trailing()
        else if (onClick != null) Icon(Icons.AutoMirrored.Filled.KeyboardArrowRight, null, tint = C.Faint)
    }
}

private fun Modifier.drawLeftAccent() = this.drawBehind { drawRect(C.Cyan, size = androidx.compose.ui.geometry.Size(2.dp.toPx(), size.height)) }

@Composable
fun PrimaryButton(text: String, modifier: Modifier = Modifier, enabled: Boolean = true, icon: ImageVector? = null, onClick: () -> Unit) {
    Button(onClick, modifier.heightIn(min = 48.dp), enabled = enabled, shape = Shape6,
        colors = ButtonDefaults.buttonColors(containerColor = C.Cyan, contentColor = C.Black, disabledContainerColor = C.Panel2, disabledContentColor = C.Faint)) {
        if (icon != null) { Icon(icon, null, Modifier.size(18.dp)); Spacer(Modifier.width(8.dp)) }
        Text(text, style = T.bodyM.copy(color = Color.Unspecified))
    }
}

@Composable
fun SecondaryButton(text: String, modifier: Modifier = Modifier, enabled: Boolean = true, icon: ImageVector? = null, danger: Boolean = false, onClick: () -> Unit) {
    OutlinedButton(onClick, modifier.heightIn(min = 48.dp), enabled = enabled, shape = Shape6, border = BorderStroke(1.dp, if (danger) C.Danger.copy(alpha = 0.6f) else C.Border2),
        colors = ButtonDefaults.outlinedButtonColors(contentColor = if (danger) C.Danger else C.Text)) {
        if (icon != null) { Icon(icon, null, Modifier.size(18.dp)); Spacer(Modifier.width(8.dp)) }
        Text(text, style = T.body.copy(color = Color.Unspecified))
    }
}

@Composable
fun <T> Segmented(options: List<T>, selected: T, label: (T) -> String, modifier: Modifier = Modifier, onSelect: (T) -> Unit) {
    Row(modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
        options.forEach { o ->
            val sel = o == selected
            Box(
                Modifier.weight(1f).heightIn(min = 44.dp).clip(Shape4)
                    .background(if (sel) C.Cyan.copy(alpha = 0.12f) else C.Panel)
                    .border(1.dp, if (sel) C.Cyan else C.Border2, Shape4)
                    .clickable(role = Role.RadioButton) { onSelect(o) },
                contentAlignment = Alignment.Center,
            ) { Text(label(o), style = T.bodyM.copy(color = if (sel) C.Text else C.Text2), maxLines = 1) }
        }
    }
}

@Composable
fun ToggleRow(title: String, checked: Boolean, subtitle: String? = null, enabled: Boolean = true, onChange: (Boolean) -> Unit) {
    ListRow(title, subtitle = subtitle, trailing = {
        Switch(checked, onChange, enabled = enabled, colors = SwitchDefaults.colors(checkedTrackColor = C.Cyan, checkedThumbColor = C.Black, uncheckedTrackColor = C.Panel2, uncheckedBorderColor = C.Border2))
    }, onClick = if (enabled) ({ onChange(!checked) }) else null)
}

@Composable
fun Chip(text: String, color: Color = C.Text2, bg: Color = C.Panel2) {
    Text(text, style = T.label.copy(color = color), modifier = Modifier.clip(Shape4).background(bg).padding(horizontal = 6.dp, vertical = 3.dp))
}

/** Logo BodyMotion : squelette stylisé en course + trajectoires. */
@Composable
fun BmLogo(size: Dp, gradient: Boolean = true, trails: Boolean = true, modifier: Modifier = Modifier) {
    Canvas(modifier.size(size)) {
        val s = this.size.width / 64f
        fun o(x: Float, y: Float) = Offset(x * s, y * s)
        val brush = if (gradient) Brush.linearGradient(listOf(C.Violet, C.Cyan), o(6f, 54f), o(54f, 8f)) else Brush.linearGradient(listOf(C.Cyan, C.Cyan))
        if (trails) {
            val p1 = Path().apply { moveTo(3 * s, 50 * s); cubicTo(9 * s, 44 * s, 14 * s, 42 * s, 21 * s, 41 * s) }
            val p2 = Path().apply { moveTo(10 * s, 28 * s); cubicTo(13 * s, 24 * s, 17 * s, 22 * s, 22 * s, 21 * s) }
            drawPath(p1, brush, alpha = 0.55f, style = Stroke(2.2f * s, cap = StrokeCap.Round))
            drawPath(p2, brush, alpha = 0.45f, style = Stroke(2.2f * s, cap = StrokeCap.Round))
        }
        val bones = Path().apply {
            moveTo(36 * s, 19 * s); lineTo(30 * s, 36 * s)
            moveTo(35 * s, 21 * s); lineTo(42 * s, 28 * s); lineTo(49 * s, 23 * s)
            moveTo(35 * s, 21 * s); lineTo(27 * s, 26 * s); lineTo(21 * s, 21 * s)
            moveTo(30 * s, 36 * s); lineTo(39 * s, 43 * s); lineTo(37 * s, 55 * s)
            moveTo(30 * s, 36 * s); lineTo(24 * s, 46 * s); lineTo(14 * s, 49 * s)
        }
        drawPath(bones, brush, style = Stroke((if (trails) 3.8f else 5f) * s, cap = StrokeCap.Round, join = StrokeJoin.Round))
        drawCircle(brush, 5.2f * s, o(39f, 11.5f))
        if (trails) for (p in listOf(o(35f, 21f), o(42f, 28f), o(30f, 36f), o(39f, 43f), o(27f, 26f), o(24f, 46f))) {
            drawCircle(C.Black, 1.9f * s, p); drawCircle(Color(0xFFE9FBFF), 1.9f * s, p, style = Stroke(1.4f * s))
        }
    }
}

@Composable
fun EmptyState(title: String, text: String, action: (@Composable () -> Unit)? = null) {
    Column(Modifier.fillMaxWidth().padding(32.dp), horizontalAlignment = Alignment.CenterHorizontally, verticalArrangement = Arrangement.spacedBy(10.dp)) {
        Text(title, style = T.h2); Text(text, style = T.small.copy(color = C.Muted)); action?.invoke()
    }
}

@Composable
fun InfoBanner(text: String, color: Color = C.Warn) {
    Row(Modifier.fillMaxWidth().clip(Shape6).background(color.copy(alpha = 0.08f)).border(1.dp, color.copy(alpha = 0.35f), Shape6).padding(12.dp)) {
        Text(text, style = T.small.copy(color = color.copy(alpha = 0.95f)))
    }
}
