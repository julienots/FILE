class_name HUD
extends CanvasLayer
## Interface en jeu : heure/jour/météo, besoins, objectif, bouton d'action contextuel,
## course, téléphone, pause, joystick, rotation caméra au doigt, fondu, notifications.

var player: Player
var root: Control
var time_lbl: Label
var day_lbl: Label
var weather_icon: UIIcon
var battery_icon: UIIcon
var battery_lbl: Label
var objective_lbl: Label
var need_bars := {}
var action_btn: Button
var action_name: Label
var action_verb: Label
var run_btn: Button
var toast_box: VBoxContainer
var fade_rect: ColorRect
var joystick: VirtualJoystick
var phone: Phone
var pause_menu: PauseMenu
var battery := 0.87
var _look_touch := -1

func _init(p: Player) -> void:
	player = p
	layer = 10

func _ready() -> void:
	root = Control.new()
	root.set_anchors_preset(Control.PRESET_FULL_RECT)
	root.theme = UIKit.theme()
	root.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(root)
	_look_area()
	_build_top_left()
	_build_top_right()
	_build_joystick()
	_build_actions()
	_build_toasts()
	phone = Phone.new()
	root.add_child(phone)
	pause_menu = PauseMenu.new()
	root.add_child(pause_menu)
	fade_rect = ColorRect.new()
	fade_rect.color = Color(0, 0, 0, 0)
	fade_rect.set_anchors_preset(Control.PRESET_FULL_RECT)
	fade_rect.mouse_filter = Control.MOUSE_FILTER_IGNORE
	root.add_child(fade_rect)
	player.focus_changed.connect(_on_focus)
	GameClock.minute_changed.connect(_on_minute)
	Game.needs_changed.connect(_refresh_needs)
	Game.objective_changed.connect(func(t: String) -> void: objective_lbl.text = t; objective_lbl.get_parent().visible = t != "")
	Game.toast.connect(show_toast)
	Game.fade_requested.connect(_fade)
	_on_minute(GameClock.day, GameClock.minute)
	_refresh_needs()
	_on_focus(null)

# ---------------------------------------------------------------- construction

func _look_area() -> void:
	# moitié droite de l'écran : glisser le doigt fait tourner la caméra
	var area := Control.new()
	area.set_anchors_preset(Control.PRESET_FULL_RECT)
	area.anchor_left = 0.4
	area.mouse_filter = Control.MOUSE_FILTER_STOP
	area.gui_input.connect(func(e: InputEvent) -> void:
		if e is InputEventScreenTouch:
			var t := e as InputEventScreenTouch
			if t.pressed and _look_touch == -1:
				_look_touch = t.index
			elif not t.pressed and t.index == _look_touch:
				_look_touch = -1
		elif e is InputEventScreenDrag:
			var d := e as InputEventScreenDrag
			if d.index == _look_touch:
				player.look(d.relative * 1.4)
		elif e is InputEventMouseMotion and e.device != InputEvent.DEVICE_ID_EMULATION and Input.is_mouse_button_pressed(MOUSE_BUTTON_LEFT):
			player.look((e as InputEventMouseMotion).relative)
	)
	root.add_child(area)

func _build_top_left() -> void:
	var box := VBoxContainer.new()
	box.position = Vector2(36, 28)
	box.add_theme_constant_override("separation", 6)
	root.add_child(box)
	var row := HBoxContainer.new()
	row.add_theme_constant_override("separation", 14)
	box.add_child(row)
	time_lbl = UIKit.label("07:00", 54)
	time_lbl.add_theme_font_override("font", UIKit.display_font())
	row.add_child(time_lbl)
	var col := VBoxContainer.new()
	col.add_theme_constant_override("separation", 0)
	col.alignment = BoxContainer.ALIGNMENT_CENTER
	row.add_child(col)
	day_lbl = UIKit.label("Lundi", 20, UIKit.INK)
	col.add_child(day_lbl)
	var wrow := HBoxContainer.new()
	wrow.add_theme_constant_override("separation", 6)
	weather_icon = UIIcon.new("sun", UIKit.WARN, 22)
	wrow.add_child(weather_icon)
	wrow.add_child(UIKit.label("Ensoleillé · 22 °C", 18, UIKit.INK_DIM))
	col.add_child(wrow)
	# besoins : 5 jauges fines
	var needs := HBoxContainer.new()
	needs.add_theme_constant_override("separation", 10)
	box.add_child(needs)
	for k in Game.NEED_KEYS:
		var v := VBoxContainer.new()
		v.add_theme_constant_override("separation", 2)
		var l := UIKit.label(String(Game.NEED_NAMES[k]), 13, UIKit.INK_DIM)
		v.add_child(l)
		var bar := ProgressBar.new()
		bar.custom_minimum_size = Vector2(74, 5)
		bar.show_percentage = false
		bar.add_theme_stylebox_override("background", UIKit.panel_style(Color(0, 0, 0, 0.45), 3))
		var fill := UIKit.panel_style(UIKit.INK, 3)
		fill.content_margin_top = 0
		fill.content_margin_bottom = 0
		bar.add_theme_stylebox_override("fill", fill)
		v.add_child(bar)
		needs.add_child(v)
		need_bars[k] = bar
	# objectif facultatif
	var obj := PanelContainer.new()
	obj.add_theme_stylebox_override("panel", UIKit.panel_style(UIKit.GLASS, 12))
	var orow := HBoxContainer.new()
	orow.add_theme_constant_override("separation", 10)
	orow.add_child(UIIcon.new("dot", UIKit.ACCENT, 16))
	objective_lbl = UIKit.label("", 18)
	orow.add_child(objective_lbl)
	obj.add_child(orow)
	obj.visible = false
	box.add_child(obj)

func _build_top_right() -> void:
	var row := HBoxContainer.new()
	row.anchor_left = 1.0
	row.anchor_right = 1.0
	row.offset_left = -330
	row.offset_right = -32
	row.offset_top = 28
	row.alignment = BoxContainer.ALIGNMENT_END
	row.add_theme_constant_override("separation", 14)
	root.add_child(row)
	battery_icon = UIIcon.new("battery", UIKit.INK, 30)
	battery_icon.value = battery
	row.add_child(battery_icon)
	battery_lbl = UIKit.label("87 %", 18, UIKit.INK_DIM)
	row.add_child(battery_lbl)
	var phone_btn := _round_button("phone", 68)
	phone_btn.pressed.connect(func() -> void: phone.toggle(player))
	row.add_child(phone_btn)
	var pause_btn := _round_button("pause", 68)
	pause_btn.pressed.connect(func() -> void: pause_menu.open())
	row.add_child(pause_btn)

func _round_button(icon: String, size_px: float) -> Button:
	var b := Button.new()
	b.custom_minimum_size = Vector2(size_px, size_px)
	b.add_theme_stylebox_override("normal", UIKit.panel_style(UIKit.GLASS, int(size_px * 0.5)))
	b.add_theme_stylebox_override("hover", UIKit.panel_style(Color(0.15, 0.18, 0.22, 0.7), int(size_px * 0.5)))
	b.add_theme_stylebox_override("pressed", UIKit.panel_style(Color(0.25, 0.28, 0.32, 0.8), int(size_px * 0.5)))
	var cc := CenterContainer.new()
	cc.set_anchors_preset(Control.PRESET_FULL_RECT)
	cc.mouse_filter = Control.MOUSE_FILTER_IGNORE
	cc.add_child(UIIcon.new(icon, UIKit.INK, size_px * 0.5))
	b.add_child(cc)
	b.focus_mode = Control.FOCUS_NONE
	return b

func _build_joystick() -> void:
	joystick = VirtualJoystick.new()
	joystick.anchor_top = 0.35
	joystick.anchor_bottom = 1.0
	joystick.anchor_right = 0.4
	joystick.moved.connect(func(v: Vector2) -> void: player.joystick = v)
	root.add_child(joystick)

func _build_actions() -> void:
	# bouton d'interaction contextuel : verbe + nom de l'objet
	action_btn = Button.new()
	action_btn.anchor_left = 1.0
	action_btn.anchor_right = 1.0
	action_btn.anchor_top = 1.0
	action_btn.anchor_bottom = 1.0
	action_btn.offset_left = -430
	action_btn.offset_right = -190
	action_btn.offset_top = -200
	action_btn.offset_bottom = -110
	action_btn.focus_mode = Control.FOCUS_NONE
	action_btn.add_theme_stylebox_override("normal", UIKit.panel_style(Color(0.98, 0.98, 0.97, 0.92), 22))
	action_btn.add_theme_stylebox_override("hover", UIKit.panel_style(Color(1, 1, 1, 1), 22))
	action_btn.add_theme_stylebox_override("pressed", UIKit.panel_style(Color(0.85, 0.86, 0.88, 1), 22))
	var v := VBoxContainer.new()
	v.set_anchors_preset(Control.PRESET_FULL_RECT)
	v.alignment = BoxContainer.ALIGNMENT_CENTER
	v.add_theme_constant_override("separation", 0)
	v.mouse_filter = Control.MOUSE_FILTER_IGNORE
	action_verb = UIKit.label("Ouvrir", 26, Color(0.08, 0.1, 0.13), true)
	action_verb.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	action_verb.add_theme_color_override("font_shadow_color", Color(0, 0, 0, 0))
	action_name = UIKit.label("Réfrigérateur", 16, Color(0.3, 0.33, 0.38))
	action_name.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	action_name.add_theme_color_override("font_shadow_color", Color(0, 0, 0, 0))
	v.add_child(action_verb)
	v.add_child(action_name)
	action_btn.add_child(v)
	action_btn.pressed.connect(func() -> void: player.interact())
	root.add_child(action_btn)
	run_btn = _round_button("run", 110)
	run_btn.anchor_left = 1.0
	run_btn.anchor_right = 1.0
	run_btn.anchor_top = 1.0
	run_btn.anchor_bottom = 1.0
	run_btn.offset_left = -160
	run_btn.offset_right = -50
	run_btn.offset_top = -210
	run_btn.offset_bottom = -100
	run_btn.toggle_mode = true
	run_btn.toggled.connect(func(on: bool) -> void:
		player.run_held = on
		run_btn.add_theme_stylebox_override("normal", UIKit.panel_style(UIKit.ACCENT if on else UIKit.GLASS, 55))
	)
	root.add_child(run_btn)

func _build_toasts() -> void:
	toast_box = VBoxContainer.new()
	toast_box.anchor_left = 0.5
	toast_box.anchor_right = 0.5
	toast_box.offset_left = -300
	toast_box.offset_right = 300
	toast_box.offset_top = 36
	toast_box.alignment = BoxContainer.ALIGNMENT_BEGIN
	toast_box.add_theme_constant_override("separation", 8)
	toast_box.mouse_filter = Control.MOUSE_FILTER_IGNORE
	root.add_child(toast_box)

# ---------------------------------------------------------------- mises à jour

func _unhandled_input(event: InputEvent) -> void:
	if event.is_action_pressed("interact"):
		player.interact()
	elif event.is_action_pressed("phone"):
		phone.toggle(player)
	elif event.is_action_pressed("pause"):
		pause_menu.open()

func _on_focus(it: Interactable) -> void:
	action_btn.visible = it != null
	if it:
		action_verb.text = it.action_text
		action_name.text = it.display_name
		action_btn.scale = Vector2(0.92, 0.92)
		action_btn.pivot_offset = action_btn.size * 0.5
		create_tween().tween_property(action_btn, "scale", Vector2.ONE, 0.18).set_trans(Tween.TRANS_BACK).set_ease(Tween.EASE_OUT)

func _process(_d: float) -> void:
	if player.focus and action_btn.visible:
		action_verb.text = player.focus.action_text

func _on_minute(_day: int, _m: int) -> void:
	time_lbl.text = GameClock.time_text()
	day_lbl.text = GameClock.day_name() + " · jour %d" % (GameClock.day + 1)
	weather_icon.kind = "moon" if GameClock.is_night() else "sun"
	weather_icon.color = UIKit.INK if GameClock.is_night() else UIKit.WARN
	weather_icon.queue_redraw()
	if _m % 12 == 0:
		battery = maxf(0.05, battery - 0.01)
		battery_icon.value = battery
		battery_icon.queue_redraw()
		battery_lbl.text = "%d %%" % int(battery * 100)

func _refresh_needs() -> void:
	for k in need_bars:
		var bar: ProgressBar = need_bars[k]
		var v := float(Game.needs[k])
		bar.value = v
		var fill := bar.get_theme_stylebox("fill") as StyleBoxFlat
		fill.bg_color = UIKit.INK if v > 35.0 else UIKit.WARN

func show_toast(text: String) -> void:
	var p := PanelContainer.new()
	p.add_theme_stylebox_override("panel", UIKit.panel_style(UIKit.GLASS_STRONG, 14))
	var l := UIKit.label(text, 19)
	l.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	l.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	p.add_child(l)
	p.modulate.a = 0.0
	toast_box.add_child(p)
	var tw := p.create_tween()
	tw.tween_property(p, "modulate:a", 1.0, 0.25)
	tw.tween_interval(3.0)
	tw.tween_property(p, "modulate:a", 0.0, 0.5)
	tw.tween_callback(p.queue_free)

func _fade(mid: Callable) -> void:
	player.locked = true
	fade_rect.mouse_filter = Control.MOUSE_FILTER_STOP
	var tw := create_tween()
	tw.tween_property(fade_rect, "color:a", 1.0, 0.6)
	tw.tween_callback(mid)
	tw.tween_interval(0.5)
	tw.tween_property(fade_rect, "color:a", 0.0, 0.8)
	tw.tween_callback(func() -> void:
		player.locked = false
		fade_rect.mouse_filter = Control.MOUSE_FILTER_IGNORE
	)
