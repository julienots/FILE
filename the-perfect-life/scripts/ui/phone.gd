class_name Phone
extends Control
## Téléphone du personnage : 9 applications fonctionnelles (aucun bouton décoratif).
## Messages · Contacts · Appels · Appareil photo · Galerie · Calendrier · Carte · Actualités · Entre Voisins

const W := 430.0
const H := 860.0
const APPS := [
	["Messages", Color(0.27, 0.75, 0.45)], ["Contacts", Color(0.55, 0.58, 0.62)], ["Appels", Color(0.25, 0.68, 0.38)],
	["Appareil photo", Color(0.2, 0.22, 0.25)], ["Galerie", Color(0.95, 0.6, 0.25)], ["Calendrier", Color(0.9, 0.3, 0.28)],
	["Carte", Color(0.3, 0.6, 0.85)], ["Actualités", Color(0.85, 0.2, 0.3)], ["Entre Voisins", Color(0.45, 0.4, 0.85)],
]

var is_open := false
var player: Player
var frame: Panel
var screen: Control
var title_lbl: Label
var back_btn: Button
var body: VBoxContainer
var scroll: ScrollContainer
var clock_lbl: Label
var _stack: Array[Callable] = []
var map_vp: SubViewport
var map_cam: Camera3D
var call_log: Array[Dictionary] = []
var likes := {}

## Conversations : [auteur, texte] ; "moi" = le joueur.
var threads := {
	"Claire": [["Claire", "Bonjour mon chéri. Tu as bien dormi ?"], ["Claire", "Il reste du café, je t'ai laissé une tasse."]],
	"Maman": [["Maman", "Tu viens toujours dimanche ? J'ai fait une tarte aux pommes."]],
	"Bureau Harmony Hills": [["Bureau Harmony Hills", "Rappel : réunion d'équipe à 10 h. Tenue correcte exigée."]],
}
var replies := {
	"Claire": ["Très bien, merci !", "Pas terrible… j'ai fait un drôle de rêve."],
	"Maman": ["Bien sûr, à dimanche !", "Je ne suis pas sûr de pouvoir."],
	"Bureau Harmony Hills": ["Bien reçu.", "J'arriverai un peu en retard."],
}
const CONTACTS := [["Claire", "Ma femme"], ["Maman", "Famille"], ["Bureau Harmony Hills", "Travail"], ["Docteur Lane", "Santé"], ["Bob (voisin)", "Voisin"]]
const NEWS := [
	["Harmony Hills élue « ville la plus heureuse » pour la 9e année", "Le maire a remercié les habitants pour leur sourire constant. Aucun incident n'a été signalé depuis neuf ans."],
	["Grand soleil prévu toute la semaine", "Météo stable : 22 °C, ciel bleu, vent faible. Comme la semaine dernière."],
	["Le pont de Bay Road fermé pour travaux", "Les travaux, commencés il y a trois ans, n'ont pas de date de fin. La municipalité recommande de rester en ville."],
	["Concours du plus beau jardin : inscriptions ouvertes", "Les pelouses seront inspectées samedi à 9 h précises."],
]
var POSTS := [
	["Bob", "Belle journée pour tondre la pelouse ! Comme tous les jours."],
	["Martha", "Mon gâteau aux carottes a encore gagné. Merci à tous !"],
	["Kevin", "Quelqu'un a vu un camion de déménagement ? Je n'en ai jamais vu ici."],
	["Claire", "Petit-déjeuner en amoureux ce matin."],
]

func _ready() -> void:
	set_anchors_preset(Control.PRESET_FULL_RECT)
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	visible = false
	frame = Panel.new()
	frame.size = Vector2(W, H)
	var fsb := UIKit.panel_style(Color(0.04, 0.04, 0.05), 54)
	fsb.set_border_width_all(10)
	fsb.border_color = Color(0.12, 0.12, 0.13)
	frame.add_theme_stylebox_override("panel", fsb)
	add_child(frame)
	screen = Control.new()
	screen.position = Vector2(18, 18)
	screen.size = Vector2(W - 36, H - 36)
	screen.clip_contents = true
	frame.add_child(screen)
	var bg := ColorRect.new()
	bg.color = Color(0.97, 0.97, 0.98)
	bg.set_anchors_preset(Control.PRESET_FULL_RECT)
	screen.add_child(bg)
	# barre d'état
	var status := HBoxContainer.new()
	status.position = Vector2(26, 10)
	status.size = Vector2(screen.size.x - 52, 30)
	clock_lbl = _dark("07:00", 17, true)
	status.add_child(clock_lbl)
	var sp := Control.new()
	sp.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	status.add_child(sp)
	var bat := UIIcon.new("battery", Color(0.1, 0.1, 0.12), 26)
	bat.value = 0.87
	status.add_child(bat)
	screen.add_child(status)
	# en-tête d'application
	back_btn = Button.new()
	back_btn.position = Vector2(12, 46)
	back_btn.size = Vector2(48, 48)
	back_btn.focus_mode = Control.FOCUS_NONE
	back_btn.add_theme_stylebox_override("normal", UIKit.panel_style(Color(0, 0, 0, 0), 24))
	back_btn.add_theme_stylebox_override("hover", UIKit.panel_style(Color(0, 0, 0, 0.06), 24))
	back_btn.add_theme_stylebox_override("pressed", UIKit.panel_style(Color(0, 0, 0, 0.12), 24))
	var bic := UIIcon.new("back", Color(0.1, 0.1, 0.12), 28)
	bic.position = Vector2(10, 10)
	back_btn.add_child(bic)
	back_btn.pressed.connect(_back)
	screen.add_child(back_btn)
	title_lbl = _dark("", 26, true)
	title_lbl.position = Vector2(66, 50)
	screen.add_child(title_lbl)
	scroll = ScrollContainer.new()
	scroll.position = Vector2(0, 104)
	scroll.size = Vector2(screen.size.x, screen.size.y - 150)
	scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
	screen.add_child(scroll)
	body = VBoxContainer.new()
	body.custom_minimum_size.x = screen.size.x
	body.add_theme_constant_override("separation", 10)
	scroll.add_child(body)
	# barre d'accueil (retour à l'écran d'accueil)
	var home := Button.new()
	home.position = Vector2(screen.size.x * 0.5 - 70, screen.size.y - 34)
	home.size = Vector2(140, 22)
	home.focus_mode = Control.FOCUS_NONE
	var hsb := UIKit.panel_style(Color(0.1, 0.1, 0.12), 6)
	hsb.content_margin_top = 0
	hsb.content_margin_bottom = 0
	home.add_theme_stylebox_override("normal", hsb)
	home.add_theme_stylebox_override("hover", hsb)
	home.add_theme_stylebox_override("pressed", hsb)
	home.custom_minimum_size = Vector2(140, 8)
	home.size = Vector2(140, 8)
	home.position.y = screen.size.y - 20
	home.pressed.connect(func() -> void: _stack.clear(); _home())
	screen.add_child(home)
	GameClock.minute_changed.connect(func(_d: int, _m: int) -> void: clock_lbl.text = GameClock.time_text())
	_home()

func _dark(t: String, size := 20, bold := false) -> Label:
	var l := UIKit.label(t, size, Color(0.08, 0.09, 0.11), bold)
	l.add_theme_color_override("font_shadow_color", Color(0, 0, 0, 0))
	return l

func _muted(t: String, size := 17) -> Label:
	var l := UIKit.label(t, size, Color(0.42, 0.45, 0.5))
	l.add_theme_color_override("font_shadow_color", Color(0, 0, 0, 0))
	l.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	return l

func _layout() -> void:
	var vs := get_viewport_rect().size
	var s := clampf(vs.y / (H + 80.0), 0.5, 1.2)
	frame.scale = Vector2(s, s)
	frame.position = Vector2(vs.x - W * s - 60.0, (vs.y - H * s) * 0.5)

func toggle(p: Player) -> void:
	player = p
	if is_open:
		close()
	else:
		open()

func open() -> void:
	is_open = true
	visible = true
	_layout()
	_home()
	if player:
		player.locked = true
	var end_y := frame.position.y
	frame.position.y = get_viewport_rect().size.y
	create_tween().tween_property(frame, "position:y", end_y, 0.35).set_trans(Tween.TRANS_CUBIC).set_ease(Tween.EASE_OUT)
	mouse_filter = Control.MOUSE_FILTER_STOP

func close() -> void:
	is_open = false
	if player:
		player.locked = false
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	var tw := create_tween()
	tw.tween_property(frame, "position:y", get_viewport_rect().size.y, 0.3).set_trans(Tween.TRANS_CUBIC).set_ease(Tween.EASE_IN)
	tw.tween_callback(func() -> void: visible = false; _stack.clear(); _home())

func _gui_input(event: InputEvent) -> void:
	# toucher hors du téléphone : le ranger
	if event is InputEventMouseButton and (event as InputEventMouseButton).pressed:
		if not frame.get_global_rect().has_point((event as InputEventMouseButton).global_position):
			close()

# ---------------------------------------------------------------- navigation

func _show(title: String, builder: Callable, push := true) -> void:
	if push:
		_stack.append(builder)
	for c in body.get_children():
		c.queue_free()
	title_lbl.text = title
	back_btn.visible = title != ""
	title_lbl.position.x = 66 if back_btn.visible else 26
	builder.call()
	scroll.scroll_vertical = 0
	if map_vp:
		map_vp.render_target_update_mode = SubViewport.UPDATE_WHEN_VISIBLE if title == "Carte" else SubViewport.UPDATE_DISABLED

func _back() -> void:
	if _stack.size() > 1:
		_stack.pop_back()
		var prev: Callable = _stack.back()
		_stack.pop_back()
		prev.call()
	else:
		_stack.clear()
		_home()

func _home() -> void:
	_show("", func() -> void:
		var hello := _dark(GameClock.time_text(), 64, true)
		hello.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
		body.add_child(hello)
		var d := _muted(GameClock.day_name() + " · Harmony Hills · 22 °C")
		d.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
		body.add_child(d)
		var grid := GridContainer.new()
		grid.columns = 3
		grid.add_theme_constant_override("h_separation", 22)
		grid.add_theme_constant_override("v_separation", 22)
		var wrap := MarginContainer.new()
		wrap.add_theme_constant_override("margin_left", 30)
		wrap.add_theme_constant_override("margin_top", 30)
		wrap.add_child(grid)
		body.add_child(wrap)
		for a in APPS:
			grid.add_child(_app_icon(String(a[0]), a[1]))
	, false)
	_stack.clear()

func _app_icon(app: String, col: Color) -> Control:
	var v := VBoxContainer.new()
	v.add_theme_constant_override("separation", 6)
	var b := Button.new()
	b.custom_minimum_size = Vector2(96, 96)
	b.focus_mode = Control.FOCUS_NONE
	b.add_theme_stylebox_override("normal", UIKit.panel_style(col, 26))
	b.add_theme_stylebox_override("hover", UIKit.panel_style(col.lightened(0.1), 26))
	b.add_theme_stylebox_override("pressed", UIKit.panel_style(col.darkened(0.15), 26))
	var initial := UIKit.label(app.left(1), 40, Color.WHITE, true)
	initial.set_anchors_preset(Control.PRESET_FULL_RECT)
	initial.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	initial.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
	initial.mouse_filter = Control.MOUSE_FILTER_IGNORE
	b.add_child(initial)
	b.pressed.connect(func() -> void: open_app(app))
	v.add_child(b)
	var l := _dark(app, 14)
	l.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	l.custom_minimum_size.x = 96
	l.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	v.add_child(l)
	return v

func open_app(app: String) -> void:
	match app:
		"Messages": _messages()
		"Contacts": _contacts()
		"Appels": _calls()
		"Appareil photo": _camera()
		"Galerie": _gallery()
		"Calendrier": _calendar()
		"Carte": _map()
		"Actualités": _news()
		"Entre Voisins": _social()

func _row(title: String, sub: String, cb: Callable) -> Button:
	var b := Button.new()
	b.custom_minimum_size = Vector2(screen.size.x - 32, 76)
	b.focus_mode = Control.FOCUS_NONE
	b.add_theme_stylebox_override("normal", UIKit.panel_style(Color(1, 1, 1), 16))
	b.add_theme_stylebox_override("hover", UIKit.panel_style(Color(0.93, 0.94, 0.96), 16))
	b.add_theme_stylebox_override("pressed", UIKit.panel_style(Color(0.88, 0.89, 0.92), 16))
	var v := VBoxContainer.new()
	v.position = Vector2(18, 10)
	v.add_theme_constant_override("separation", 2)
	v.mouse_filter = Control.MOUSE_FILTER_IGNORE
	v.add_child(_dark(title, 19, true))
	var s := _muted(sub, 15)
	s.autowrap_mode = TextServer.AUTOWRAP_OFF
	s.text_overrun_behavior = TextServer.OVERRUN_TRIM_ELLIPSIS
	s.custom_minimum_size.x = screen.size.x - 80
	v.add_child(s)
	b.add_child(v)
	b.pressed.connect(cb)
	var m := MarginContainer.new()
	m.add_theme_constant_override("margin_left", 16)
	m.add_child(b)
	body.add_child(m)
	return b

# ---------------------------------------------------------------- applications

func _messages() -> void:
	_show("Messages", func() -> void:
		for who in threads:
			var conv: Array = threads[who]
			var last: Array = conv.back()
			_row(String(who), String(last[1]), func() -> void: _thread(String(who)))
	)

func _thread(who: String) -> void:
	_show(who, func() -> void:
		for msg in threads[who]:
			var mine: bool = msg[0] == "moi"
			var p := PanelContainer.new()
			p.add_theme_stylebox_override("panel", UIKit.panel_style(Color(0.2, 0.5, 0.95) if mine else Color(0.9, 0.91, 0.93), 18))
			var l := UIKit.label(String(msg[1]), 18, Color.WHITE if mine else Color(0.08, 0.09, 0.11))
			l.add_theme_color_override("font_shadow_color", Color(0, 0, 0, 0))
			l.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
			l.custom_minimum_size.x = 260
			p.add_child(l)
			var m := MarginContainer.new()
			m.add_theme_constant_override("margin_left", 110 if mine else 16)
			m.add_theme_constant_override("margin_right", 16 if mine else 110)
			m.add_child(p)
			body.add_child(m)
		if replies.has(who):
			body.add_child(_muted("  Répondre :"))
			for r in replies[who]:
				_row(String(r), "Envoyer", func() -> void:
					threads[who].append(["moi", String(r)])
					replies.erase(who)
					if who == "Claire":
						Game.relations.claire = float(Game.relations.claire) + (4.0 if String(r).begins_with("Très") else 1.0)
						Game.flags["repondu_claire"] = true
					_stack.pop_back()
					_thread(who)
				)
	)

func _contacts() -> void:
	_show("Contacts", func() -> void:
		for c in CONTACTS:
			_row(String(c[0]), String(c[1]) + " · Appeler", func() -> void: _call(String(c[0])))
	)

func _call(who: String) -> void:
	call_log.push_front({who = who, at = GameClock.time_text()})
	var lines := {
		"Claire": "« Je suis juste à côté, tu sais. Tu m'appelles pour rien ? » Elle rit.",
		"Maman": "« Mon grand ! Dimanche, n'oublie pas la tarte. »",
		"Bureau Harmony Hills": "Messagerie : « Le bureau est ouvert de 9 h à 17 h. Tout va bien. »",
		"Docteur Lane": "« Vous allez très bien. Continuez votre routine. »",
		"Bob (voisin)": "« Salut voisin ! Belle journée pour tondre, hein ? »",
	}
	_show("Appel", func() -> void:
		var n := _dark(who, 30, true)
		n.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
		body.add_child(n)
		var t := _muted("Appel terminé · " + GameClock.time_text())
		t.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
		body.add_child(t)
		var m := MarginContainer.new()
		m.add_theme_constant_override("margin_left", 24)
		m.add_theme_constant_override("margin_right", 24)
		var l := _dark(String(lines.get(who, "…")), 19)
		l.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
		m.add_child(l)
		body.add_child(m)
	)
	GameClock.advance(2)

func _calls() -> void:
	_show("Appels", func() -> void:
		if call_log.is_empty():
			body.add_child(_muted("   Aucun appel récent. Appelez depuis Contacts."))
		for c in call_log:
			_row(String(c.who), "Sortant · " + String(c.at), func() -> void: _call(String(c.who)))
	)

func _camera() -> void:
	_show("Appareil photo", func() -> void:
		body.add_child(_muted("   Cadrez avec la caméra du jeu, puis déclenchez."))
		_row("Prendre une photo", "La photo est rangée dans la Galerie", func() -> void: _shoot())
	)

func _shoot() -> void:
	visible = false
	var hud := get_parent() as Control
	hud.visible = false
	await RenderingServer.frame_post_draw
	var img := get_viewport().get_texture().get_image()
	hud.visible = true
	visible = true
	img.resize(img.get_width() / 3, img.get_height() / 3, Image.INTERPOLATE_BILINEAR)
	Game.photos.append(img)
	Game.flags["photos"] = Game.photos.size()
	Game.notify("Photo enregistrée dans la Galerie")

func _gallery() -> void:
	_show("Galerie", func() -> void:
		if Game.photos.is_empty():
			body.add_child(_muted("   Aucune photo. Utilisez l'Appareil photo."))
			return
		var grid := GridContainer.new()
		grid.columns = 2
		grid.add_theme_constant_override("h_separation", 8)
		grid.add_theme_constant_override("v_separation", 8)
		var m := MarginContainer.new()
		m.add_theme_constant_override("margin_left", 12)
		m.add_child(grid)
		body.add_child(m)
		for img in Game.photos:
			var tr := TextureRect.new()
			tr.texture = ImageTexture.create_from_image(img)
			tr.custom_minimum_size = Vector2(180, 101)
			tr.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
			tr.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_COVERED
			grid.add_child(tr)
	)

func _calendar() -> void:
	_show("Calendrier", func() -> void:
		var items := [
			["07:00", "Réveil", "a_dormi"], ["07:30", "Café et petit-déjeuner", "petit_dejeuner"],
			["08:00", "Discuter avec Claire", "parle_claire"], ["09:00", "Sortir de la maison", "sorti"],
			["12:00", "Déjeuner", "dejeuner"], ["15:00", "Promenade au parc", "promenade"],
			["20:00", "Télévision", "tele"], ["22:30", "Se coucher", "couche"],
		]
		var now := GameClock.minute
		for it in items:
			var hm := String(it[0]).split(":")
			var m := int(hm[0]) * 60 + int(hm[1])
			var done: bool = Game.flags.get(String(it[2]), false) != false
			var state := "Fait" if done else ("Maintenant" if absi(now - m) < 30 else ("Plus tard" if m > now else "Passé"))
			_row(String(it[0]) + "  " + String(it[1]), state, func() -> void: Game.notify(String(it[1]) + " · " + String(it[0])))
	)

func _map() -> void:
	if map_vp == null:
		map_vp = SubViewport.new()
		map_vp.size = Vector2i(380, 560)
		map_vp.render_target_update_mode = SubViewport.UPDATE_WHEN_VISIBLE
		map_cam = Camera3D.new()
		map_cam.projection = Camera3D.PROJECTION_ORTHOGONAL
		map_cam.size = 70.0
		map_cam.far = 300.0
		map_cam.rotation_degrees = Vector3(-90, 0, 0)
		map_vp.add_child(map_cam)
		add_child(map_vp)
	_show("Carte", func() -> void:
		if player:
			map_cam.position = Vector3(player.global_position.x, 120.0, player.global_position.z)
		var tr := TextureRect.new()
		tr.texture = map_vp.get_texture()
		tr.custom_minimum_size = Vector2(380, 560)
		var m := MarginContainer.new()
		m.add_theme_constant_override("margin_left", 8)
		var holder := Control.new()
		holder.custom_minimum_size = Vector2(380, 560)
		holder.add_child(tr)
		var dot := UIIcon.new("dot", UIKit.ACCENT, 26)
		dot.position = Vector2(190 - 13, 280 - 13)
		holder.add_child(dot)
		m.add_child(holder)
		body.add_child(m)
		body.add_child(_muted("   Vous êtes ici · 1 Maple Drive, Harmony Hills"))
	)

func _news() -> void:
	_show("Actualités", func() -> void:
		for n in NEWS:
			_row(String(n[0]), "Harmony Hills Gazette", func() -> void:
				_show("Article", func() -> void:
					var t := _dark(String(n[0]), 24, true)
					t.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
					t.custom_minimum_size.x = screen.size.x - 40
					var m := MarginContainer.new()
					m.add_theme_constant_override("margin_left", 20)
					var v := VBoxContainer.new()
					v.add_theme_constant_override("separation", 14)
					v.add_child(t)
					var b := _dark(String(n[1]), 19)
					b.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
					b.custom_minimum_size.x = screen.size.x - 40
					v.add_child(b)
					m.add_child(v)
					body.add_child(m)
				)
			)
	)

func _social() -> void:
	_show("Entre Voisins", func() -> void:
		for i in POSTS.size():
			var post: Array = POSTS[i]
			var key := str(i)
			var liked: bool = likes.get(key, false)
			var count := 12 + i * 7 + (1 if liked else 0)
			var toggle_like := func() -> void:
				likes[key] = not liked
				Game.apply({humeur = 1.0})
				_stack.pop_back()
				_social()
			var b := _row(String(post[0]), String(post[1]), toggle_like)
			b.custom_minimum_size.y = 96
			var like := Button.new()
			like.text = ("Aimé · " if liked else "J'aime · ") + str(count)
			like.focus_mode = Control.FOCUS_NONE
			like.add_theme_color_override("font_color", UIKit.ACCENT if liked else Color(0.35, 0.38, 0.42))
			like.add_theme_stylebox_override("normal", UIKit.panel_style(Color(0, 0, 0, 0), 12))
			like.position = Vector2(screen.size.x - 140, 52)
			like.pressed.connect(toggle_like)
			b.add_child(like)
	)
