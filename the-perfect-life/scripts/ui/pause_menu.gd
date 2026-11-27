class_name PauseMenu
extends Control
## Menu pause : reprendre, sauvegarder/charger (3 emplacements), paramètres, commandes.

var panel: PanelContainer
var body: VBoxContainer
var is_open := false

func _ready() -> void:
	set_anchors_preset(Control.PRESET_FULL_RECT)
	visible = false
	process_mode = Node.PROCESS_MODE_ALWAYS
	var dim := ColorRect.new()
	dim.color = Color(0.02, 0.03, 0.05, 0.62)
	dim.set_anchors_preset(Control.PRESET_FULL_RECT)
	add_child(dim)
	panel = PanelContainer.new()
	panel.add_theme_stylebox_override("panel", UIKit.panel_style(UIKit.GLASS_STRONG, 26))
	panel.set_anchors_preset(Control.PRESET_CENTER)
	panel.custom_minimum_size = Vector2(560, 0)
	add_child(panel)
	var m := MarginContainer.new()
	for side in ["left", "right", "top", "bottom"]:
		m.add_theme_constant_override("margin_" + side, 22)
	panel.add_child(m)
	body = VBoxContainer.new()
	body.add_theme_constant_override("separation", 12)
	m.add_child(body)

func open() -> void:
	is_open = true
	visible = true
	get_tree().paused = true
	_main()

func close() -> void:
	is_open = false
	visible = false
	get_tree().paused = false

func _unhandled_input(event: InputEvent) -> void:
	if is_open and event.is_action_pressed("pause"):
		close()
		get_viewport().set_input_as_handled()

func _clear(title: String) -> void:
	for c in body.get_children():
		c.queue_free()
	var t := UIKit.label(title, 40)
	t.add_theme_font_override("font", UIKit.display_font())
	body.add_child(t)

func _btn(text: String, cb: Callable) -> Button:
	var b := Button.new()
	b.text = text
	b.custom_minimum_size = Vector2(0, 64)
	b.add_theme_font_size_override("font_size", 24)
	b.pressed.connect(cb)
	body.add_child(b)
	return b

func _recenter() -> void:
	await get_tree().process_frame
	panel.reset_size()
	panel.position = (get_viewport_rect().size - panel.size) * 0.5

func _main() -> void:
	_clear("Pause")
	_btn("Reprendre", close)
	_btn("Sauvegarder", func() -> void: _slots(true))
	_btn("Charger une partie", func() -> void: _slots(false))
	_btn("Paramètres", _settings)
	_btn("Commandes", _controls)
	_recenter()

func _slots(saving: bool) -> void:
	_clear("Sauvegarder" if saving else "Charger")
	for i in 3:
		var slot := i
		_btn("Emplacement %d · %s" % [i + 1, Game.save_info(i)], func() -> void:
			if saving:
				Game.save_game(slot)
				Game.notify("Partie sauvegardée (emplacement %d)" % (slot + 1))
				_slots(true)
			elif Game.has_save(slot):
				Game.load_game(slot)
				close()
				Game.notify("Partie chargée")
		)
	_btn("Retour", _main)
	_recenter()

func _settings() -> void:
	_clear("Paramètres")
	var q := OptionButton.new()
	for n in Quality.NAMES:
		q.add_item("Qualité graphique : " + n)
	q.selected = int(Game.settings.quality)
	q.custom_minimum_size.y = 60
	q.item_selected.connect(func(i: int) -> void:
		Game.settings.quality = i
		Game.save_settings()
		var w := get_tree().get_first_node_in_group("world") as World
		if w:
			w.set_quality(i)
	)
	body.add_child(q)
	for s in [["Sensibilité caméra", "sensitivity", 0.3, 2.5], ["Volume général", "master", 0.0, 1.0], ["Musique", "music", 0.0, 1.0], ["Effets sonores", "sfx", 0.0, 1.0]]:
		var row := HBoxContainer.new()
		var l := UIKit.label(String(s[0]), 20)
		l.custom_minimum_size.x = 230
		row.add_child(l)
		var sl := HSlider.new()
		sl.min_value = float(s[2])
		sl.max_value = float(s[3])
		sl.step = 0.05
		sl.value = float(Game.settings[s[1]])
		sl.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		sl.custom_minimum_size.y = 44
		var key: String = s[1]
		sl.value_changed.connect(func(v: float) -> void:
			Game.settings[key] = v
			Game.save_settings()
			if key == "master":
				AudioServer.set_bus_volume_db(0, linear_to_db(maxf(v, 0.001)))
		)
		row.add_child(sl)
		body.add_child(row)
	var fps := OptionButton.new()
	for f in [30, 60]:
		fps.add_item("Limite d'images : %d i/s" % f)
	fps.selected = 0 if int(Game.settings.fps_cap) == 30 else 1
	fps.custom_minimum_size.y = 60
	fps.item_selected.connect(func(i: int) -> void:
		Game.settings.fps_cap = 30 if i == 0 else 60
		Engine.max_fps = int(Game.settings.fps_cap)
		Game.save_settings()
	)
	body.add_child(fps)
	_btn("Retour", _main)
	_recenter()

func _controls() -> void:
	_clear("Commandes")
	for line in [
		"Joystick gauche : se déplacer",
		"Glisser à droite : tourner la caméra",
		"Bouton blanc : action sur l'objet proche",
		"Bouton course : marcher / courir",
		"Téléphone : messages, carte, photos…",
		"Clavier : ZQSD ou WASD, Maj, E, Tab, Échap",
	]:
		var l := UIKit.label(line, 20, UIKit.INK_DIM)
		body.add_child(l)
	_btn("Retour", _main)
	_recenter()
