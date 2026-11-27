class_name UIKit
extends RefCounted
## Charte de l'interface : couleurs, police, panneaux translucides, icônes vectorielles.

const INK := Color(0.96, 0.97, 0.98)
const INK_DIM := Color(0.78, 0.82, 0.86)
const GLASS := Color(0.06, 0.08, 0.11, 0.55)
const GLASS_STRONG := Color(0.05, 0.07, 0.1, 0.82)
const ACCENT := Color(0.86, 0.26, 0.22)        # le rouge de la porte d'entrée
const SKY := Color(0.55, 0.74, 0.95)
const GOOD := Color(0.45, 0.82, 0.58)
const WARN := Color(0.98, 0.72, 0.32)

static var _theme: Theme

static func theme() -> Theme:
	if _theme:
		return _theme
	_theme = Theme.new()
	var f := FontFile.new()
	f.load_dynamic_font("res://assets/fonts/Inter.woff2")
	_theme.default_font = f
	_theme.default_font_size = 22
	_theme.set_color("font_color", "Label", INK)
	_theme.set_color("font_color", "Button", INK)
	_theme.set_color("font_hover_color", "Button", Color.WHITE)
	_theme.set_color("font_pressed_color", "Button", Color.WHITE)
	_theme.set_stylebox("normal", "Button", panel_style(Color(1, 1, 1, 0.1), 14))
	_theme.set_stylebox("hover", "Button", panel_style(Color(1, 1, 1, 0.18), 14))
	_theme.set_stylebox("pressed", "Button", panel_style(Color(1, 1, 1, 0.28), 14))
	_theme.set_stylebox("focus", "Button", panel_style(Color(0, 0, 0, 0), 14, ACCENT))
	_theme.set_stylebox("panel", "PanelContainer", panel_style(GLASS, 18))
	return _theme

static func display_font() -> FontFile:
	var f := FontFile.new()
	f.load_dynamic_font("res://assets/fonts/Bricolage.woff2")
	return f

static func panel_style(color: Color, radius := 18, border := Color(0, 0, 0, 0)) -> StyleBoxFlat:
	var sb := StyleBoxFlat.new()
	sb.bg_color = color
	sb.set_corner_radius_all(radius)
	sb.content_margin_left = 16
	sb.content_margin_right = 16
	sb.content_margin_top = 10
	sb.content_margin_bottom = 10
	if border.a > 0.0:
		sb.set_border_width_all(2)
		sb.border_color = border
	sb.anti_aliasing = true
	return sb

static func label(text: String, size := 22, color := INK, bold := false) -> Label:
	var l := Label.new()
	l.text = text
	l.add_theme_font_size_override("font_size", size)
	l.add_theme_color_override("font_color", color)
	if bold:
		var fv := FontVariation.new()
		fv.base_font = theme().default_font
		fv.variation_embolden = 0.6
		l.add_theme_font_override("font", fv)
	l.add_theme_color_override("font_shadow_color", Color(0, 0, 0, 0.35))
	l.add_theme_constant_override("shadow_offset_y", 1)
	return l
