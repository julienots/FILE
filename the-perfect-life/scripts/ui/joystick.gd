class_name VirtualJoystick
extends Control
## Joystick virtuel flottant : apparaît là où le pouce touche (moitié gauche de l'écran).

signal moved(v: Vector2)

const RADIUS := 90.0
var _touch := -1
var _center := Vector2.ZERO
var _knob := Vector2.ZERO
var value := Vector2.ZERO

func _ready() -> void:
	mouse_filter = Control.MOUSE_FILTER_STOP

func _gui_input(event: InputEvent) -> void:
	if event is InputEventScreenTouch:
		var t := event as InputEventScreenTouch
		if t.pressed and _touch == -1:
			_touch = t.index
			_center = t.position
			_set_knob(t.position)
		elif not t.pressed and t.index == _touch:
			_release()
		accept_event()
	elif event is InputEventScreenDrag:
		var d := event as InputEventScreenDrag
		if d.index == _touch:
			_set_knob(d.position)
			accept_event()
	elif event.device == InputEvent.DEVICE_ID_EMULATION:
		return
	elif event is InputEventMouseButton:
		var mb := event as InputEventMouseButton
		if mb.button_index == MOUSE_BUTTON_LEFT:
			if mb.pressed:
				_touch = 99
				_center = mb.position
				_set_knob(mb.position)
			else:
				_release()
	elif event is InputEventMouseMotion and _touch == 99:
		_set_knob((event as InputEventMouseMotion).position)

func _set_knob(p: Vector2) -> void:
	var off := (p - _center).limit_length(RADIUS)
	_knob = off
	value = off / RADIUS
	moved.emit(value)
	queue_redraw()

func _release() -> void:
	_touch = -1
	_knob = Vector2.ZERO
	value = Vector2.ZERO
	moved.emit(value)
	queue_redraw()

func _draw() -> void:
	var c := _center if _touch != -1 else Vector2(RADIUS + 70.0, size.y - RADIUS - 60.0)
	var alpha := 0.9 if _touch != -1 else 0.45
	draw_circle(c, RADIUS, Color(0.05, 0.07, 0.1, 0.35 * alpha))
	draw_arc(c, RADIUS, 0, TAU, 64, Color(1, 1, 1, 0.35 * alpha), 3.0, true)
	draw_circle(c + _knob, 38.0, Color(1, 1, 1, 0.55 * alpha))
