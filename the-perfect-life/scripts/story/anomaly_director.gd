class_name AnomalyDirector
extends Node
## Le malaise, dosé. Phase 1 : vie parfaite. Phase 2 : incohérences. Phase 3 : soupçons.
## Phase 4 : la vérité affleure (le bout de la rue). Chaque événement est réel et persistant.

signal phase_changed(p: int)

var main: Node
var player: Player
var hud: HUD
var wife: WifeAI
var world: World
var house: Node3D
var phase := 1
var watcher: Node3D          # le voisin qui ne bouge que quand on ne le regarde pas
var _watch_step := 0
var _watch_cooldown := 0.0
var cam_node: Node3D         # caméra du lampadaire
var cam_led: OmniLight3D
var sky_panel: MeshInstance3D
var tv_vp: SubViewport
var tv_ticker: Label
var _flicker_t := 25.0
var _sky_cd := 20.0
var _sms_done := {}
var _time_skip_done := false

const WATCH_PATH := [Vector3(18.0, 0.05, 40.0), Vector3(14.0, 0.18, 31.2), Vector3(11.5, 0.05, 26.2), Vector3(11.0, 0.18, 21.3), Vector3(11.8, 0.05, 15.6), Vector3(12.9, 0.05, 13.4)]

func setup(m: Node) -> void:
	main = m
	player = m.player
	hud = m.hud
	wife = m.wife
	world = m.world
	house = world.main_house
	add_to_group("anomaly")
	_tv_live()
	_street_camera()
	_watcher()
	_sky_panel()
	_backdrop_walls()
	Game.action_logged.connect(_on_action)
	GameClock.minute_changed.connect(func(_d: int, _m: int) -> void: _evaluate())
	hud.phone.reply_sent.connect(_on_reply)
	_evaluate()

# ---------------------------------------------------------------- phases

func _evaluate() -> void:
	var m := GameClock.minute + GameClock.day * 1440
	var actions := Game.action_log.size()
	var talks := int(Game.flags.get("talks_claire", 0))
	var outside := player.global_position.z > 12.8 or player.global_position.z < -0.5
	var target := phase
	if phase == 1 and (actions >= 2 or m >= 7 * 60 + 40):
		target = 2
	if phase == 2 and (talks >= 2 or outside or m >= 8 * 60 + 30):
		target = 3
	if phase == 3 and (Game.suspicion >= 0.45 or m >= 10 * 60):
		target = 4
	if target != phase:
		set_phase(target)
	_scheduled_sms(m)
	if phase >= 3 and not _time_skip_done and m >= 9 * 60 and not player.locked:
		_time_skip()

func set_phase(p: int) -> void:
	phase = p
	wife.phase = p
	Game.flags["phase"] = p
	phase_changed.emit(p)
	match p:
		2:
			Game.set_objective("Profiter de la matinée")
		3:
			Game.set_objective("Quelque chose ne va pas")
			Game.add_suspicion(0.12)
		4:
			Game.set_objective("Aller au bout de la rue, vers l'est")
			_sms("Numéro inconnu", "Va au bout de la rue. Vers l'est. Ne cours pas, ils remarquent quand on court.", ["J'y vais.", "Laissez-moi tranquille."])

func _scheduled_sms(_m: int) -> void:
	if phase >= 2:
		var awake := maxi(1, GameClock.minute - 7 * 60)
		_sms("Numéro inconnu", "Bonjour. Tu es réveillé depuis %d minutes." % awake, ["Qui êtes-vous ?", "Mauvais numéro."], "s1")
	if phase >= 2 and Game.action_log.size() >= 3:
		_sms("Numéro inconnu", "Ne regarde pas le lampadaire. Il te regarde déjà.", [], "s2")
	if phase >= 3:
		_sms("Numéro inconnu", "Elle ne s'appelle pas Claire.", [], "s3")
	if phase >= 3 and Game.suspicion > 0.3:
		_sms("Numéro inconnu", "Il fait beau depuis 3 412 jours. Tu ne trouves pas ça bizarre ?", [], "s4")

func _sms(who: String, text: String, opts: Array = [], key := "") -> void:
	var k := key if key != "" else text
	if _sms_done.has(k):
		return
	_sms_done[k] = true
	hud.phone.receive(who, text, opts)
	Game.add_suspicion(0.05)
	get_tree().call_group("anomaly", "on_stinger", 0.6)

func _on_reply(who: String, text: String) -> void:
	if who != "Numéro inconnu":
		return
	await get_tree().create_timer(4.0).timeout
	var answers := {
		"Qui êtes-vous ?": "Quelqu'un qui a vu l'émission depuis le début.",
		"Mauvais numéro.": "Il n'y a pas de mauvais numéro à Harmony Hills. Il n'y a qu'un seul numéro.",
		"J'y vais.": "Bien. Ne parle à personne.",
		"Laissez-moi tranquille.": "Eux ne te laisseront jamais tranquille.",
	}
	hud.phone.receive("Numéro inconnu", String(answers.get(text, "…")))

# ---------------------------------------------------------------- la télé en direct

func _tv_live() -> void:
	var it: HouseInteractions = main.interactions
	if it == null or it.tv_screen_mat == null:
		return
	tv_vp = SubViewport.new()
	tv_vp.size = Vector2i(640, 360)
	tv_vp.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	var bg := TextureRect.new()
	bg.texture = load("res://assets/ui/tv_show.png")
	bg.size = Vector2(640, 360)
	bg.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	tv_vp.add_child(bg)
	var band := ColorRect.new()
	band.color = Color(0.05, 0.05, 0.06)
	band.position = Vector2(0, 318)
	band.size = Vector2(640, 42)
	tv_vp.add_child(band)
	tv_ticker = Label.new()
	tv_ticker.position = Vector2(12, 322)
	tv_ticker.size = Vector2(620, 34)
	tv_ticker.add_theme_font_size_override("font_size", 20)
	tv_ticker.add_theme_font_override("font", UIKit.theme().default_font)
	tv_ticker.add_theme_color_override("font_color", Color(1, 1, 1))
	tv_ticker.text = "Harmony Hills · Il fait beau · 22 °C"
	tv_vp.add_child(tv_ticker)
	add_child(tv_vp)
	it.tv_screen_mat.emission_texture = tv_vp.get_texture()

func _on_action(t: String, text: String) -> void:
	if tv_ticker and phase >= 2:
		tv_ticker.text = "EN DIRECT · %s · %s" % [t, text]
		tv_ticker.add_theme_color_override("font_color", Color(1.0, 0.85, 0.3))
	_evaluate()

# ---------------------------------------------------------------- la caméra du lampadaire

func _street_camera() -> void:
	for n in world.find_children("security_camera_01*", "", true, false):
		cam_node = n as Node3D
		break
	if cam_node == null:
		return
	cam_led = OmniLight3D.new()
	cam_led.light_color = Color(1, 0.1, 0.08)
	cam_led.light_energy = 0.0
	cam_led.omni_range = 0.6
	cam_led.position = Vector3(0, 0.05, 0.12)
	cam_node.add_child(cam_led)

# ---------------------------------------------------------------- le voisin qui fixe

func _watcher() -> void:
	watcher = Geo.prop(world, "res://assets/thirdparty/Soldier.glb", WATCH_PATH[0], 0.0)
	watcher.name = "Voisin"
	(watcher.get_child(0) as Node3D).rotation.y = PI
	var aps := watcher.find_children("*", "AnimationPlayer", true, false)
	if aps.size() > 0:
		var ap := aps[0] as AnimationPlayer
		ap.play("Idle")
		ap.speed_scale = 0.0          # immobile. Parfaitement immobile.
	watcher.visible = false
	var it := Interactable.attach(watcher, Vector3(0, 1.4, 0), "Voisin", "Saluer", func(_p: Node, _it: Interactable) -> void:
		Game.notify("Le voisin ne répond pas. Il sourit. Il n'a pas cligné des yeux.")
		Game.add_suspicion(0.08)
		Game.log_action("Il a parlé au figurant.")
	, 2.2)
	it.priority = 0.5

# ---------------------------------------------------------------- le ciel

func _sky_panel() -> void:
	sky_panel = MeshInstance3D.new()
	var q := QuadMesh.new()
	q.size = Vector2(90, 60)
	sky_panel.mesh = q
	var sh := ShaderMaterial.new()
	sh.shader = load("res://shaders/sky_rig.gdshader")
	sky_panel.material_override = sh
	sky_panel.visible = false
	sky_panel.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	world.add_child(sky_panel)

func _flash_sky() -> void:
	var cam := player.camera
	var fwd := -cam.global_transform.basis.z
	fwd.y = 0.0
	fwd = fwd.normalized()
	sky_panel.global_position = cam.global_position + fwd * 160.0 + Vector3(0, 70, 0)
	sky_panel.look_at(cam.global_position, Vector3.UP)
	sky_panel.rotate_object_local(Vector3.UP, PI)
	sky_panel.visible = true
	on_stinger(1.0)
	Game.add_suspicion(0.1)
	Game.log_action("Il a vu le plafond.")
	await get_tree().create_timer(0.35).timeout
	sky_panel.visible = false
	await get_tree().create_timer(0.12).timeout
	sky_panel.visible = true
	await get_tree().create_timer(0.2).timeout
	sky_panel.visible = false

# ---------------------------------------------------------------- les murs du plateau

func _backdrop_walls() -> void:
	var tex: Texture2D = load("res://assets/ui/backdrop.jpg") if ResourceLoader.exists("res://assets/ui/backdrop.jpg") else null
	for side in [1.0, -1.0]:
		var wall := MeshInstance3D.new()
		var q := QuadMesh.new()
		q.size = Vector2(140, 46)
		wall.mesh = q
		var m := StandardMaterial3D.new()
		m.albedo_texture = tex
		m.albedo_color = Color(0.92, 0.93, 0.95)
		m.roughness = 0.9
		m.uv1_scale = Vector3(3, 1, 1)
		wall.material_override = m
		wall.position = Vector3(side * 104.0, 23.0, 26.0)
		wall.rotation.y = -side * PI * 0.5
		world.add_child(wall)
		# structure métallique visible derrière (coutures du décor)
		for k in 7:
			Geo.box(world, Vector3(0.3, 46, 0.3), Vector3(side * 104.2, 23.0, -44.0 + k * 23.3), MatLib.flat(Color(0.2, 0.2, 0.22), 0.6, 0.8), 0.02)
		var body := Geo.box(world, Vector3(1.0, 46.0, 140.0), Vector3(side * 104.6, 23.0, 26.0), MatLib.flat(Color(0.15, 0.15, 0.16), 0.9), 0.0, true)
		body.visible = false
	# porte de service dans le mur est
	var door := Geo.box(world, Vector3(0.12, 2.6, 1.4), Vector3(103.9, 1.3, 26.3), MatLib.flat(Color(0.28, 0.3, 0.32), 0.45, 0.7), 0.01, true)
	var sign_lbl := Label3D.new()
	sign_lbl.text = "PLATEAU 4\nACCÈS RÉSERVÉ AU PERSONNEL"
	sign_lbl.font_size = 64
	sign_lbl.pixel_size = 0.004
	sign_lbl.position = Vector3(103.8, 3.0, 26.3)
	sign_lbl.rotation.y = -PI * 0.5
	sign_lbl.modulate = Color(0.95, 0.9, 0.4)
	world.add_child(sign_lbl)
	Interactable.attach(world, Vector3(103.4, 1.2, 26.3), "Porte de service", "Ouvrir", func(_p: Node, it: Interactable) -> void:
		if phase < 4:
			Game.notify("Verrouillée. Le mur est froid. Il sonne creux.")
			Game.add_suspicion(0.1)
			return
		on_stinger(1.0)
		Game.flags["fin_demo"] = true
		Game.notify("Une voix, derrière la porte : « Retourne te coucher. Le soleil se lève à 6 h 30. »")
		Game.set_objective("Fin de la démo · À suivre")
		it.action_text = "Écouter"
	, 2.5)

# ---------------------------------------------------------------- boucle

func _process(delta: float) -> void:
	if player == null:
		return
	var cam := player.camera
	# caméra du lampadaire : suit le joueur dès la phase 2, voyant rouge
	if cam_node and phase >= 2:
		var target := player.global_position + Vector3(0, 1.5, 0)
		var t := cam_node.global_transform.looking_at(target, Vector3.UP)
		cam_node.global_transform = cam_node.global_transform.interpolate_with(t, 1.0 - exp(-3.0 * delta))
		cam_led.light_energy = 1.2 if int(Time.get_ticks_msec() / 600) % 2 == 0 else 0.0
	# voisin : visible dès la phase 2, avance d'un pas quand il est hors champ
	if watcher and phase >= 2:
		watcher.visible = true
		var to := player.global_position - watcher.global_position
		watcher.rotation.y = atan2(to.x, to.z)
		_watch_cooldown -= delta
		var seen := cam.is_position_in_frustum(watcher.global_position + Vector3.UP)
		var max_step := mini(WATCH_PATH.size() - 1, phase + 1)
		if not seen and _watch_cooldown <= 0.0 and _watch_step < max_step:
			_watch_step += 1
			watcher.global_position = WATCH_PATH[_watch_step]
			_watch_cooldown = 12.0
			Game.add_suspicion(0.03)
	# lumières qui vacillent (phase 3+)
	if phase >= 3:
		_flicker_t -= delta
		if _flicker_t <= 0.0:
			_flicker_t = randf_range(35.0, 70.0)
			_flicker()
	# le ciel se fissure quand on est dehors (phase 3+)
	if phase >= 3 and player.global_position.z > 12.8:
		_sky_cd -= delta
		if _sky_cd <= 0.0:
			_sky_cd = randf_range(50.0, 90.0)
			_flash_sky()
	if int(Engine.get_process_frames()) % 30 == 0:
		_evaluate()

func _flicker() -> void:
	var lights := get_tree().get_nodes_in_group("lights_rdc") + get_tree().get_nodes_in_group("lights_chambre")
	on_hum(1.6)
	for i in 9:
		for l in lights:
			(l as Light3D).visible = randf() > 0.5
		await get_tree().create_timer(randf_range(0.05, 0.18)).timeout
	for l in lights:
		(l as Light3D).visible = true
	Game.add_suspicion(0.04)

func _time_skip() -> void:
	_time_skip_done = true
	on_glitch(0.8)
	Game.fade(func() -> void:
		GameClock.advance(23)
		Game.add_suspicion(0.12)
		Game.log_action("Pause publicitaire.")
	)
	Game.notify("Il est %s ? Vous ne vous souvenez pas des 23 dernières minutes." % GameClock.time_text())

# rappels de groupe (son et image), relayés vers l'audio et le post-process
func on_stinger(strength: float) -> void:
	get_tree().call_group("audio_director", "stinger", strength)
	get_tree().call_group("unease_fx", "pulse", strength * 0.6)

func on_glitch(strength: float) -> void:
	get_tree().call_group("audio_director", "stinger", strength)
	get_tree().call_group("unease_fx", "glitch", strength)

func on_hum(seconds: float) -> void:
	get_tree().call_group("audio_director", "hum", seconds)
