package com.superessence.essabank.ui.components

import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.animateColorAsState
import androidx.compose.animation.core.Spring
import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.core.spring
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.slideInVertically
import androidx.compose.animation.slideOutVertically
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.interaction.collectIsPressedAsState
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxScope
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.RowScope
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.navigationBarsPadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.LazyListScope
import androidx.compose.foundation.lazy.LazyRow
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.rounded.ArrowBack
import androidx.compose.material.icons.rounded.CheckCircle
import androidx.compose.material.icons.rounded.ChevronRight
import androidx.compose.material.icons.rounded.ErrorOutline
import androidx.compose.material.icons.rounded.Info
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.ModalBottomSheet
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.OutlinedTextFieldDefaults
import androidx.compose.material3.Switch
import androidx.compose.material3.SwitchDefaults
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.rememberModalBottomSheetState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.shadow
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.superessence.essabank.ui.UiMessage
import com.superessence.essabank.ui.i18n.tr
import com.superessence.essabank.ui.theme.Dimens
import com.superessence.essabank.ui.theme.Essa
import com.superessence.essabank.ui.theme.EssaType
import kotlinx.coroutines.delay

// ---------------------------------------------------------------- micro-interactions

/** Légère réduction d'échelle à l'appui (retour tactile visuel). */
@Composable
fun Modifier.pressable(enabled: Boolean = true, onClick: () -> Unit): Modifier {
    val source = remember { MutableInteractionSource() }
    val pressed by source.collectIsPressedAsState()
    val scale by animateFloatAsState(if (pressed) 0.96f else 1f, spring(dampingRatio = Spring.DampingRatioMediumBouncy, stiffness = Spring.StiffnessMedium), label = "press")
    return this
        .graphicsLayer { scaleX = scale; scaleY = scale }
        .clickable(interactionSource = source, indication = null, enabled = enabled, role = Role.Button, onClick = onClick)
}

// ---------------------------------------------------------------- structure d'écran

@Composable
fun ScreenScaffold(
    title: String,
    onBack: (() -> Unit)?,
    actions: @Composable RowScope.() -> Unit = {},
    bottomBar: (@Composable () -> Unit)? = null,
    contentPadding: PaddingValues = PaddingValues(horizontal = Dimens.screen, vertical = 8.dp),
    content: LazyListScope.() -> Unit,
) {
    val c = Essa.colors
    Column(Modifier.fillMaxSize().background(c.background)) {
        TopBar(title, onBack, actions)
        Box(Modifier.weight(1f)) {
            LazyColumn(
                Modifier.fillMaxSize(), contentPadding = contentPadding,
                verticalArrangement = Arrangement.spacedBy(Dimens.gap),
            ) {
                content()
                item { Spacer(Modifier.height(24.dp)) }
            }
        }
        if (bottomBar != null) {
            Box(Modifier.fillMaxWidth().background(c.background).navigationBarsPadding().padding(horizontal = Dimens.screen, vertical = 12.dp)) { bottomBar() }
        }
    }
}

@Composable
fun TopBar(title: String, onBack: (() -> Unit)?, actions: @Composable RowScope.() -> Unit = {}) {
    val c = Essa.colors
    Row(
        Modifier.fillMaxWidth().statusBarsPadding().height(60.dp).padding(horizontal = 8.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        if (onBack != null) {
            RoundIconButton(Icons.AutoMirrored.Rounded.ArrowBack, tr("Retour", "Back"), onBack)
        } else Spacer(Modifier.width(12.dp))
        Text(
            title, style = EssaType.h3, color = c.text, maxLines = 1, overflow = TextOverflow.Ellipsis,
            modifier = Modifier.weight(1f).padding(horizontal = 8.dp),
        )
        actions()
    }
}

@Composable
fun RoundIconButton(icon: ImageVector, description: String, onClick: () -> Unit, tint: Color = Essa.colors.text, background: Color = Essa.colors.surface, size: Dp = 42.dp, badge: Int = 0) {
    Box {
        Box(
            Modifier.size(size).clip(CircleShape).background(background).border(1.dp, Essa.colors.border, CircleShape).pressable(onClick = onClick),
            contentAlignment = Alignment.Center,
        ) { Icon(icon, description, tint = tint, modifier = Modifier.size(size * 0.5f)) }
        if (badge > 0) {
            Box(
                Modifier.align(Alignment.TopEnd).size(18.dp).clip(CircleShape).background(Essa.colors.negative).border(2.dp, Essa.colors.background, CircleShape),
                contentAlignment = Alignment.Center,
            ) { Text(if (badge > 9) "9+" else badge.toString(), color = Color.White, fontSize = 9.sp, fontWeight = FontWeight.Bold) }
        }
    }
}

// ---------------------------------------------------------------- surfaces

@Composable
fun Panel(
    modifier: Modifier = Modifier,
    padding: Dp = 16.dp,
    onClick: (() -> Unit)? = null,
    background: Color = Essa.colors.surface,
    content: @Composable ColumnScope.() -> Unit,
) {
    val c = Essa.colors
    val base = modifier
        .fillMaxWidth()
        .shadow(if (c.isDark) 0.dp else 6.dp, RoundedCornerShape(Dimens.radius), ambientColor = Color(0x140A1230), spotColor = Color(0x140A1230))
        .clip(RoundedCornerShape(Dimens.radius))
        .background(background)
        .border(1.dp, c.border, RoundedCornerShape(Dimens.radius))
    Column(
        (if (onClick != null) base.pressable(onClick = onClick) else base).padding(padding),
        content = content,
    )
}

@Composable
fun IconBadge(icon: ImageVector, tint: Color, size: Dp = 42.dp, background: Color = tint.copy(alpha = 0.13f), shape: RoundedCornerShape = RoundedCornerShape(size * 0.32f)) {
    Box(Modifier.size(size).clip(shape).background(background), contentAlignment = Alignment.Center) {
        Icon(icon, null, tint = tint, modifier = Modifier.size(size * 0.5f))
    }
}

@Composable
fun SectionHeader(title: String, action: String? = null, onAction: (() -> Unit)? = null) {
    val c = Essa.colors
    Row(Modifier.fillMaxWidth().padding(top = 10.dp, bottom = 2.dp), verticalAlignment = Alignment.CenterVertically) {
        Text(title, style = EssaType.h3, color = c.text, modifier = Modifier.weight(1f))
        if (action != null && onAction != null) {
            Text(action, style = EssaType.bodyStrong, color = c.accent, modifier = Modifier.clip(RoundedCornerShape(8.dp)).clickable(onClick = onAction).padding(6.dp))
        }
    }
}

@Composable
fun ListRow(
    title: String,
    subtitle: String? = null,
    leading: (@Composable () -> Unit)? = null,
    trailing: (@Composable () -> Unit)? = null,
    onClick: (() -> Unit)? = null,
    chevron: Boolean = onClick != null && trailing == null,
    titleColor: Color = Essa.colors.text,
) {
    val c = Essa.colors
    Row(
        Modifier.fillMaxWidth().clip(RoundedCornerShape(Dimens.radiusSmall))
            .then(if (onClick != null) Modifier.clickable(onClick = onClick) else Modifier)
            .padding(vertical = 10.dp, horizontal = 4.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        if (leading != null) { leading(); Spacer(Modifier.width(14.dp)) }
        Column(Modifier.weight(1f)) {
            Text(title, style = EssaType.title, color = titleColor, maxLines = 1, overflow = TextOverflow.Ellipsis)
            if (subtitle != null) Text(subtitle, style = EssaType.small, color = c.textMuted, maxLines = 2, overflow = TextOverflow.Ellipsis)
        }
        if (trailing != null) { Spacer(Modifier.width(10.dp)); trailing() }
        if (chevron) Icon(Icons.Rounded.ChevronRight, null, tint = c.textMuted)
    }
}

@Composable
fun Divider(modifier: Modifier = Modifier) {
    Box(modifier.fillMaxWidth().height(1.dp).background(Essa.colors.border))
}

// ---------------------------------------------------------------- boutons

@Composable
fun PrimaryButton(text: String, onClick: () -> Unit, modifier: Modifier = Modifier, enabled: Boolean = true, loading: Boolean = false, icon: ImageVector? = null) {
    val c = Essa.colors
    val alpha by animateFloatAsState(if (enabled) 1f else 0.4f, label = "btn")
    Box(
        modifier.fillMaxWidth().height(56.dp).graphicsLayer { this.alpha = alpha }
            .clip(RoundedCornerShape(18.dp))
            .background(Brush.horizontalGradient(listOf(c.accent, c.accent.copy(alpha = 0.82f))))
            .pressable(enabled = enabled && !loading, onClick = onClick),
        contentAlignment = Alignment.Center,
    ) {
        if (loading) CircularProgressIndicator(color = c.onAccent, strokeWidth = 2.5.dp, modifier = Modifier.size(22.dp))
        else Row(verticalAlignment = Alignment.CenterVertically) {
            if (icon != null) { Icon(icon, null, tint = c.onAccent, modifier = Modifier.size(20.dp)); Spacer(Modifier.width(8.dp)) }
            Text(text, style = EssaType.title.copy(fontSize = 16.sp), color = c.onAccent)
        }
    }
}

@Composable
fun SecondaryButton(text: String, onClick: () -> Unit, modifier: Modifier = Modifier, icon: ImageVector? = null, danger: Boolean = false, enabled: Boolean = true) {
    val c = Essa.colors
    val color = if (danger) c.negative else c.text
    Row(
        modifier.height(52.dp).clip(RoundedCornerShape(16.dp)).background(c.surfaceAlt)
            .border(1.dp, c.border, RoundedCornerShape(16.dp)).graphicsLayer { alpha = if (enabled) 1f else 0.4f }
            .pressable(enabled = enabled, onClick = onClick).padding(horizontal = 16.dp),
        verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.Center,
    ) {
        if (icon != null) { Icon(icon, null, tint = color, modifier = Modifier.size(19.dp)); Spacer(Modifier.width(8.dp)) }
        Text(text, style = EssaType.bodyStrong, color = color, maxLines = 1)
    }
}

/** Bouton d'action rapide (icône dans un carré arrondi + libellé). */
@Composable
fun QuickAction(icon: ImageVector, label: String, onClick: () -> Unit, modifier: Modifier = Modifier, highlight: Boolean = false) {
    val c = Essa.colors
    Column(modifier.pressable(onClick = onClick), horizontalAlignment = Alignment.CenterHorizontally) {
        Box(
            Modifier.size(56.dp).clip(RoundedCornerShape(18.dp))
                .background(if (highlight) Brush.linearGradient(listOf(c.accent, c.accent.copy(alpha = 0.7f))) else Brush.linearGradient(listOf(c.surface, c.surface)))
                .border(1.dp, if (highlight) Color.Transparent else c.border, RoundedCornerShape(18.dp)),
            contentAlignment = Alignment.Center,
        ) { Icon(icon, null, tint = if (highlight) c.onAccent else c.accent, modifier = Modifier.size(25.dp)) }
        Spacer(Modifier.height(7.dp))
        Text(label, style = EssaType.small, color = c.textSecondary, textAlign = TextAlign.Center, maxLines = 1)
    }
}

// ---------------------------------------------------------------- sélecteurs

@Composable
fun ChipRow(labels: List<String>, selected: Int, onSelect: (Int) -> Unit, modifier: Modifier = Modifier) {
    LazyRow(modifier, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        items(labels.size) { i -> Chip(labels[i], i == selected) { onSelect(i) } }
    }
}

@Composable
fun Chip(text: String, selected: Boolean, icon: ImageVector? = null, onClick: () -> Unit) {
    val c = Essa.colors
    val bg by animateColorAsState(if (selected) c.accent else c.surface, label = "chip")
    val fg by animateColorAsState(if (selected) c.onAccent else c.textSecondary, label = "chipText")
    Row(
        Modifier.height(36.dp).clip(RoundedCornerShape(12.dp)).background(bg)
            .border(1.dp, if (selected) Color.Transparent else c.border, RoundedCornerShape(12.dp))
            .clickable(onClick = onClick).padding(horizontal = 14.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        if (icon != null) { Icon(icon, null, tint = fg, modifier = Modifier.size(16.dp)); Spacer(Modifier.width(6.dp)) }
        Text(text, style = EssaType.small.copy(fontWeight = FontWeight.SemiBold), color = fg, maxLines = 1)
    }
}

@Composable
fun SegmentedControl(labels: List<String>, selected: Int, onSelect: (Int) -> Unit, modifier: Modifier = Modifier) {
    val c = Essa.colors
    Row(
        modifier.fillMaxWidth().height(42.dp).clip(RoundedCornerShape(14.dp)).background(c.surfaceAlt).padding(4.dp),
    ) {
        labels.forEachIndexed { i, label ->
            val sel = i == selected
            val bg by animateColorAsState(if (sel) c.surface else Color.Transparent, label = "seg")
            Box(
                Modifier.weight(1f).fillMaxSize().clip(RoundedCornerShape(11.dp)).background(bg).clickable { onSelect(i) },
                contentAlignment = Alignment.Center,
            ) { Text(label, style = EssaType.small.copy(fontWeight = FontWeight.SemiBold), color = if (sel) c.text else c.textMuted, maxLines = 1) }
        }
    }
}

@Composable
fun ToggleRow(title: String, subtitle: String?, checked: Boolean, icon: ImageVector? = null, enabled: Boolean = true, onChange: (Boolean) -> Unit) {
    val c = Essa.colors
    ListRow(
        title = title, subtitle = subtitle,
        leading = icon?.let { { IconBadge(it, c.accent, 38.dp) } },
        trailing = {
            Switch(
                checked = checked, onCheckedChange = onChange, enabled = enabled,
                colors = SwitchDefaults.colors(checkedTrackColor = c.accent, checkedThumbColor = Color.White, uncheckedTrackColor = c.surfaceAlt, uncheckedBorderColor = c.border),
            )
        },
        onClick = if (enabled) ({ onChange(!checked) }) else null, chevron = false,
    )
}

// ---------------------------------------------------------------- champs

@Composable
fun EssaTextField(
    value: String, onChange: (String) -> Unit, label: String, modifier: Modifier = Modifier,
    keyboard: KeyboardType = KeyboardType.Text, singleLine: Boolean = true, supporting: String? = null, isError: Boolean = false,
    maxLength: Int = 60, leading: ImageVector? = null, password: Boolean = false,
) {
    val c = Essa.colors
    OutlinedTextField(
        value = value, onValueChange = { if (it.length <= maxLength) onChange(it) }, label = { Text(label) },
        modifier = modifier.fillMaxWidth(), singleLine = singleLine, isError = isError,
        keyboardOptions = KeyboardOptions(keyboardType = keyboard),
        visualTransformation = if (password) androidx.compose.ui.text.input.PasswordVisualTransformation() else androidx.compose.ui.text.input.VisualTransformation.None,
        supportingText = supporting?.let { { Text(it) } },
        leadingIcon = leading?.let { { Icon(it, null) } },
        shape = RoundedCornerShape(16.dp), textStyle = EssaType.body.copy(fontSize = 15.sp, color = c.text),
        colors = OutlinedTextFieldDefaults.colors(
            focusedBorderColor = c.accent, unfocusedBorderColor = c.border, focusedLabelColor = c.accent, unfocusedLabelColor = c.textMuted,
            cursorColor = c.accent, focusedContainerColor = c.surface, unfocusedContainerColor = c.surface,
            focusedLeadingIconColor = c.accent, unfocusedLeadingIconColor = c.textMuted,
        ),
    )
}

// ---------------------------------------------------------------- dialogues et feuilles

@Composable
fun ConfirmDialog(
    title: String, text: String, confirm: String, onConfirm: () -> Unit, onDismiss: () -> Unit, danger: Boolean = false,
) {
    val c = Essa.colors
    AlertDialog(
        onDismissRequest = onDismiss,
        containerColor = c.elevated,
        title = { Text(title, style = EssaType.h3, color = c.text) },
        text = { Text(text, style = EssaType.body, color = c.textSecondary) },
        confirmButton = { TextButton(onClick = { onConfirm(); onDismiss() }) { Text(confirm, color = if (danger) c.negative else c.accent, style = EssaType.bodyStrong) } },
        dismissButton = { TextButton(onClick = onDismiss) { Text(tr("Annuler", "Cancel"), color = c.textSecondary, style = EssaType.bodyStrong) } },
    )
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun EssaSheet(onDismiss: () -> Unit, content: @Composable ColumnScope.() -> Unit) {
    val c = Essa.colors
    ModalBottomSheet(
        onDismissRequest = onDismiss,
        sheetState = rememberModalBottomSheetState(skipPartiallyExpanded = true),
        containerColor = c.elevated,
        shape = RoundedCornerShape(topStart = 28.dp, topEnd = 28.dp),
    ) {
        Column(Modifier.fillMaxWidth().padding(horizontal = Dimens.screen).padding(bottom = 28.dp), content = content)
    }
}

// ---------------------------------------------------------------- mentions et états

/** Pastille « SIMULATION » présente sur les écrans sensibles. */
@Composable
fun SimulationPill(modifier: Modifier = Modifier, text: String = tr("SIMULATION", "SIMULATION")) {
    val c = Essa.colors
    Box(modifier.clip(RoundedCornerShape(8.dp)).background(c.accentSoft).padding(horizontal = 8.dp, vertical = 3.dp)) {
        Text(text, style = EssaType.caption.copy(fontSize = 10.sp), color = c.accent)
    }
}

@Composable
fun DisclaimerFooter(modifier: Modifier = Modifier) {
    val c = Essa.colors
    Row(modifier.fillMaxWidth().padding(vertical = 8.dp), verticalAlignment = Alignment.Top) {
        Icon(Icons.Rounded.Info, null, tint = c.textMuted, modifier = Modifier.size(16.dp).padding(top = 1.dp))
        Spacer(Modifier.width(8.dp))
        Text(
            tr("ESSABANK est une application de simulation éditée par Superessence Studio. Ce n'est pas une banque : les montants sont des unités virtuelles sans valeur réelle, aucune opération n'est transmise à un établissement financier.",
                "ESSABANK is a simulation app published by Superessence Studio. It is not a bank: amounts are virtual units with no real value, and no operation is ever sent to a financial institution."),
            style = EssaType.small, color = c.textMuted,
        )
    }
}

@Composable
fun EmptyState(icon: ImageVector, title: String, text: String, action: String? = null, onAction: (() -> Unit)? = null) {
    val c = Essa.colors
    Column(Modifier.fillMaxWidth().padding(vertical = 36.dp, horizontal = 12.dp), horizontalAlignment = Alignment.CenterHorizontally) {
        IconBadge(icon, c.accent, 64.dp)
        Spacer(Modifier.height(16.dp))
        Text(title, style = EssaType.h3, color = c.text, textAlign = TextAlign.Center)
        Spacer(Modifier.height(6.dp))
        Text(text, style = EssaType.body, color = c.textMuted, textAlign = TextAlign.Center)
        if (action != null && onAction != null) {
            Spacer(Modifier.height(18.dp))
            SecondaryButton(action, onAction)
        }
    }
}

/** Petit libellé + valeur, pour les grilles de statistiques. */
@Composable
fun StatTile(label: String, value: String, modifier: Modifier = Modifier, valueColor: Color = Essa.colors.text, icon: ImageVector? = null, iconTint: Color = Essa.colors.accent) {
    val c = Essa.colors
    Panel(modifier, padding = 14.dp) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            if (icon != null) { Icon(icon, null, tint = iconTint, modifier = Modifier.size(16.dp)); Spacer(Modifier.width(6.dp)) }
            Text(label, style = EssaType.small, color = c.textMuted, maxLines = 1)
        }
        Spacer(Modifier.height(6.dp))
        Text(value, style = EssaType.amount.copy(fontSize = 16.sp, fontWeight = FontWeight.Bold), color = valueColor, maxLines = 1, overflow = TextOverflow.Ellipsis)
    }
}

@Composable
fun KeyValue(label: String, value: String, valueColor: Color = Essa.colors.text, bold: Boolean = true) {
    val c = Essa.colors
    Row(Modifier.fillMaxWidth().padding(vertical = 9.dp), verticalAlignment = Alignment.CenterVertically) {
        Text(label, style = EssaType.body, color = c.textMuted, modifier = Modifier.weight(0.45f))
        Text(value, style = if (bold) EssaType.bodyStrong else EssaType.body, color = valueColor, textAlign = TextAlign.End, modifier = Modifier.weight(0.55f))
    }
}

/** Bandeau de message (succès / erreur), affiché brièvement en bas de l'écran. */
@Composable
fun BoxScope.MessageHost(message: UiMessage?, onShown: (Long) -> Unit) {
    val c = Essa.colors
    var visible by remember { mutableStateOf<UiMessage?>(null) }
    LaunchedEffect(message?.id) {
        if (message != null) {
            visible = message
            delay(if (message.error) 3600 else 2400)
            visible = null
            onShown(message.id)
        }
    }
    AnimatedVisibility(
        visible = visible != null,
        enter = slideInVertically { it } + fadeIn(), exit = slideOutVertically { it } + fadeOut(),
        modifier = Modifier.align(Alignment.BottomCenter).navigationBarsPadding().padding(start = 16.dp, end = 16.dp, bottom = 92.dp),
    ) {
        val m = visible
        if (m != null) {
            Row(
                Modifier.fillMaxWidth().shadow(12.dp, RoundedCornerShape(16.dp)).clip(RoundedCornerShape(16.dp))
                    .background(if (c.isDark) c.elevated else Color(0xFF0A1230))
                    .border(BorderStroke(1.dp, (if (m.error) c.negative else c.positive).copy(alpha = 0.5f)), RoundedCornerShape(16.dp))
                    .padding(14.dp),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                Icon(if (m.error) Icons.Rounded.ErrorOutline else Icons.Rounded.CheckCircle, null, tint = if (m.error) c.negative else c.positive)
                Spacer(Modifier.width(10.dp))
                Text(m.text, style = EssaType.bodyStrong, color = Color.White)
            }
        }
    }
}

@Composable
fun ProgressBar(progress: Float, color: Color, modifier: Modifier = Modifier, height: Dp = 8.dp, track: Color = Essa.colors.surfaceAlt) {
    val animated by animateFloatAsState(progress.coerceIn(0f, 1f), spring(stiffness = Spring.StiffnessLow), label = "progress")
    Box(modifier.fillMaxWidth().height(height).clip(RoundedCornerShape(height)).background(track)) {
        Box(Modifier.fillMaxWidth(animated).height(height).clip(RoundedCornerShape(height)).background(Brush.horizontalGradient(listOf(color.copy(alpha = 0.75f), color))))
    }
}

val TextStyle.tabular: TextStyle get() = copy(fontFeatureSettings = "tnum")
