extends Node
## État de partie (autoload « Game ») : besoins, relations, progression, sauvegardes, réglages.

signal needs_changed
signal objective_changed(text: String)
signal toast(text: String)
## Fondu au noir : le HUD noircit l'écran, appelle `mid`, puis revient.
signal fade_requested(mid: Callable)
## Chaque action du joueur est « observée » (la télé la commente en phase 2+).
signal action_logged(time_text: String, text: String)
signal suspicion_changed(v: float)

const NEED_KEYS := ["energie", "faim", "hygiene", "sommeil", "humeur"]
const NEED_NAMES := {energie = "Énergie", faim = "Faim", hygiene = "Hygiène", sommeil = "Sommeil", humeur = "Humeur"}
## Perte par heure de jeu, volontairement faible (jamais frustrant).
const DECAY := {energie = 2.5, faim = 4.0, hygiene = 2.0, sommeil = 3.0, humeur = 1.0}
const FLOOR := 15.0

var needs := {energie = 85.0, faim = 60.0, hygiene = 70.0, sommeil = 80.0, humeur = 75.0}
var relations := {claire = 70.0}
var flags := {}
var objective := ""
var settings := {quality = 1, sensitivity = 1.0, master = 0.8, music = 0.7, sfx = 0.9, fps_cap = 60}
var player: Node3D
var suspicion := 0.0
var action_log: Array[String] = []
var photos: Array[Image] = []

func _ready() -> void:
	_setup_input()
	load_settings()
	GameClock.hour_changed.connect(_on_hour)

func _on_hour(_h: int) -> void:
	for k in NEED_KEYS:
		needs[k] = maxf(FLOOR, float(needs[k]) - float(DECAY[k]))
	needs_changed.emit()

func apply(effects: Dictionary) -> void:
	for k in effects:
		if needs.has(k):
			needs[k] = clampf(float(needs[k]) + float(effects[k]), 0.0, 100.0)
	needs_changed.emit()

func set_objective(text: String) -> void:
	objective = text
	objective_changed.emit(text)

func fade(mid: Callable) -> void:
	fade_requested.emit(mid)

func log_action(text: String) -> void:
	action_log.append(GameClock.time_text() + " · " + text)
	action_logged.emit(GameClock.time_text(), text)

func add_suspicion(v: float) -> void:
	suspicion = clampf(suspicion + v, 0.0, 1.0)
	suspicion_changed.emit(suspicion)

func notify(text: String) -> void:
	toast.emit(text)

# ---------------------------------------------------------------- entrées

func _setup_input() -> void:
	var keys := {
		move_forward = [KEY_W, KEY_Z, KEY_UP], move_back = [KEY_S, KEY_DOWN],
		move_left = [KEY_A, KEY_Q, KEY_LEFT], move_right = [KEY_D, KEY_RIGHT],
		run = [KEY_SHIFT], interact = [KEY_E, KEY_ENTER], phone = [KEY_P, KEY_TAB], pause = [KEY_ESCAPE],
	}
	for action in keys:
		if not InputMap.has_action(action):
			InputMap.add_action(action)
		for k in keys[action]:
			var ev := InputEventKey.new()
			ev.physical_keycode = k
			InputMap.action_add_event(action, ev)

# ---------------------------------------------------------------- sauvegardes

func save_path(slot: int) -> String:
	return "user://sauvegarde_%d.json" % slot

func has_save(slot: int) -> bool:
	return FileAccess.file_exists(save_path(slot))

func save_game(slot: int, extra: Dictionary = {}) -> bool:
	var data := {
		version = 1,
		saved_at = Time.get_datetime_string_from_system(),
		clock = GameClock.to_dict(),
		needs = needs, relations = relations, flags = flags, objective = objective, suspicion = suspicion,
	}
	if player:
		data.player = {pos = [player.global_position.x, player.global_position.y, player.global_position.z], rot = player.rotation.y}
	data.merge(extra)
	var f := FileAccess.open(save_path(slot), FileAccess.WRITE)
	if f == null:
		return false
	f.store_string(JSON.stringify(data, "\t"))
	return true

func load_game(slot: int) -> Dictionary:
	if not has_save(slot):
		return {}
	var txt := FileAccess.get_file_as_string(save_path(slot))
	var parsed: Variant = JSON.parse_string(txt)
	if typeof(parsed) != TYPE_DICTIONARY:
		return {}
	var data: Dictionary = parsed
	GameClock.from_dict(data.get("clock", {}))
	needs.merge(data.get("needs", {}), true)
	relations.merge(data.get("relations", {}), true)
	flags = data.get("flags", {})
	suspicion = float(data.get("suspicion", 0.0))
	suspicion_changed.emit(suspicion)
	set_objective(String(data.get("objective", "")))
	if player and data.has("player"):
		var p: Array = data.player.pos
		player.global_position = Vector3(float(p[0]), float(p[1]), float(p[2]))
		player.rotation.y = float(data.player.rot)
	needs_changed.emit()
	return data

func save_info(slot: int) -> String:
	if not has_save(slot):
		return "Emplacement vide"
	var parsed: Variant = JSON.parse_string(FileAccess.get_file_as_string(save_path(slot)))
	if typeof(parsed) != TYPE_DICTIONARY:
		return "Sauvegarde illisible"
	var d: Dictionary = parsed
	var c: Dictionary = d.get("clock", {})
	var dn: String = GameClock.DAY_NAMES[int(c.get("day", 0)) % 7]
	var m := int(c.get("minute", 0))
	return "%s %02d:%02d · jour %d" % [dn, m / 60, m % 60, int(c.get("day", 0)) + 1]

func save_settings() -> void:
	var f := FileAccess.open("user://reglages.json", FileAccess.WRITE)
	if f:
		f.store_string(JSON.stringify(settings))

func load_settings() -> void:
	if not FileAccess.file_exists("user://reglages.json"):
		# premier lancement sur téléphone : réglages sûrs (le joueur peut monter en qualité)
		if OS.has_feature("mobile"):
			settings.quality = 2
			settings.fps_cap = 30
		return
	var parsed: Variant = JSON.parse_string(FileAccess.get_file_as_string("user://reglages.json"))
	if typeof(parsed) == TYPE_DICTIONARY:
		settings.merge(parsed, true)
