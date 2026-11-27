class_name UIIcon
extends Control
## Icônes vectorielles dessinées (nettes à toute résolution, aucune image).

var kind := "phone"
var color := UIKit.INK
var value := 1.0   # batterie : niveau 0..1

func _init(k := "phone", c := UIKit.INK, size_px := 28.0) -> void:
	kind = k
	color = c
	custom_minimum_size = Vector2(size_px, size_px)
	mouse_filter = Control.MOUSE_FILTER_IGNORE

func _draw() -> void:
	var s := size
	var w := maxf(2.0, s.x * 0.08)
	match kind:
		"phone":
			draw_rect(Rect2(s * Vector2(0.28, 0.08), s * Vector2(0.44, 0.84)), color, false, w)
			draw_line(s * Vector2(0.42, 0.8), s * Vector2(0.58, 0.8), color, w)
		"pause":
			draw_rect(Rect2(s * Vector2(0.28, 0.2), s * Vector2(0.14, 0.6)), color)
			draw_rect(Rect2(s * Vector2(0.58, 0.2), s * Vector2(0.14, 0.6)), color)
		"run":
			draw_circle(s * Vector2(0.6, 0.18), s.x * 0.1, color)
			draw_polyline(PackedVector2Array([s * Vector2(0.3, 0.45), s * Vector2(0.52, 0.36), s * Vector2(0.48, 0.62), s * Vector2(0.66, 0.78), s * Vector2(0.62, 0.95)]), color, w)
			draw_polyline(PackedVector2Array([s * Vector2(0.48, 0.62), s * Vector2(0.3, 0.8), s * Vector2(0.15, 0.78)]), color, w)
			draw_polyline(PackedVector2Array([s * Vector2(0.52, 0.36), s * Vector2(0.68, 0.5), s * Vector2(0.82, 0.44)]), color, w)
		"hand":
			draw_arc(s * 0.5, s.x * 0.36, 0, TAU, 32, color, w)
			draw_circle(s * 0.5, s.x * 0.12, color)
		"sun":
			draw_circle(s * 0.5, s.x * 0.2, color)
			for i in 8:
				var a := TAU * i / 8.0
				draw_line(s * 0.5 + Vector2(cos(a), sin(a)) * s.x * 0.3, s * 0.5 + Vector2(cos(a), sin(a)) * s.x * 0.44, color, w * 0.8)
		"moon":
			draw_circle(s * 0.5, s.x * 0.32, color)
			draw_circle(s * Vector2(0.64, 0.4), s.x * 0.28, UIKit.GLASS_STRONG)
		"battery":
			draw_rect(Rect2(s * Vector2(0.08, 0.3), s * Vector2(0.76, 0.4)), color, false, w * 0.7)
			draw_rect(Rect2(s * Vector2(0.86, 0.42), s * Vector2(0.06, 0.16)), color)
			draw_rect(Rect2(s * Vector2(0.14, 0.38), s * Vector2(0.64 * value, 0.24)), UIKit.GOOD if value > 0.2 else UIKit.ACCENT)
		"back":
			draw_polyline(PackedVector2Array([s * Vector2(0.62, 0.2), s * Vector2(0.32, 0.5), s * Vector2(0.62, 0.8)]), color, w)
		"close":
			draw_line(s * 0.25, s * 0.75, color, w)
			draw_line(s * Vector2(0.75, 0.25), s * Vector2(0.25, 0.75), color, w)
		"dot":
			draw_circle(s * 0.5, s.x * 0.22, color)
