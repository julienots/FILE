class_name WifeAI
extends CharacterBody3D
## Claire : IA autonome à routine horaire + priorités.
## États : ALLER (navigation) → ACTIVITÉ (posée, orientée) ; DISCUTER interrompt tout ;
## ABSENTE quand elle sort faire les courses (porte d'entrée, réapparition au retour).

enum State { GOING, DOING, TALKING, AWAY }

const SPEED := 1.25
const F2 := HouseBuilder.FLOOR2

## [heure de début en minutes, activité, position, direction du regard (yaw), réplique d'ambiance]
var schedule := [
	[0, "Dort", Vector3(11.4, F2 + 0.05, 6.3), 0.0, "Chut… il est tard."],
	[6 * 60 + 45, "Prépare le café", Vector3(2.4, 0.05, 1.45), PI, "Le café est presque prêt !"],
	[7 * 60 + 35, "Cuisine le petit-déjeuner", Vector3(5.15, 0.05, 1.05), PI, "Des pancakes, comme tous les lundis."],
	[8 * 60 + 10, "Lit les nouvelles", Vector3(12.0, 0.05, 1.3), 0.0, "Tu as vu ? On a encore gagné le prix de la ville la plus heureuse."],
	[9 * 60 + 15, "Arrose les plantes", Vector3(15.2, 0.05, 5.9), PI * 0.5, "Les plantes adorent ce soleil."],
	[10 * 60 + 30, "Fait les courses", Vector3(6.5, 0.05, 11.4), 0.0, ""],
	[15 * 60, "Lit sur le canapé", Vector3(12.3, 0.05, 8.5), PI, "Ce livre est parfait. Tout est toujours parfait."],
	[18 * 60, "Prépare le dîner", Vector3(5.15, 0.05, 1.05), PI, "Ce soir : rôti et purée."],
	[19 * 60 + 45, "Regarde la télé", Vector3(10.6, 0.05, 6.2), PI * 0.5, "Ton émission va commencer."],
	[22 * 60 + 30, "Se prépare à dormir", Vector3(13.0, F2 + 0.05, 9.6), -PI * 0.5, "Bonne nuit, mon chéri."],
	[22 * 60 + 50, "Dort", Vector3(11.4, F2 + 0.05, 6.3), 0.0, "Chut… il est tard."],
]

var state := State.GOING
var model: Node3D
var anim: AnimationPlayer
var agent: NavigationAgent3D
var talk_it: Interactable
var current := -1
var activity := ""
var _anim := ""
var _talk_timer := 0.0
var _away_until := -1

func _ready() -> void:
	name = "Claire"
	collision_layer = 4
	collision_mask = 1
	var cs := CollisionShape3D.new()
	var cap := CapsuleShape3D.new()
	cap.radius = 0.25
	cap.height = 1.65
	cs.shape = cap
	cs.position.y = 0.83
	add_child(cs)
	model = Geo.prop(self, "res://assets/thirdparty/Michelle.glb", Vector3.ZERO, 0.0)
	var aps := model.find_children("*", "AnimationPlayer", true, false)
	if aps.size() > 0:
		anim = aps[0]
		_borrow_locomotion()
	agent = NavigationAgent3D.new()
	agent.radius = 0.3
	agent.height = 1.7
	agent.path_desired_distance = 0.35
	agent.target_desired_distance = 0.3
	agent.avoidance_enabled = false
	add_child(agent)
	talk_it = Interactable.attach(self, Vector3(0, 1.2, 0), "Claire", "Parler", _on_talk, 1.9)
	GameClock.minute_changed.connect(func(_d: int, _m: int) -> void: _check_schedule())
	call_deferred("_check_schedule", true)

## Récupère Idle/Walk/Run du rig Mixamo du Soldier (mêmes noms d'os) : reciblage direct.
func _borrow_locomotion() -> void:
	var src: Node = load("res://assets/thirdparty/Soldier.glb").instantiate()
	var sap: AnimationPlayer = src.find_children("*", "AnimationPlayer", true, false)[0]
	var ssk: Skeleton3D = src.find_children("*", "Skeleton3D", true, false)[0]
	var tsk: Skeleton3D = model.find_children("*", "Skeleton3D", true, false)[0]
	var lib := AnimationLibrary.new()
	for n in ["Idle", "Walk", "Run"]:
		var a: Animation = sap.get_animation(n).duplicate(true)
		a.loop_mode = Animation.LOOP_LINEAR
		for t in range(a.get_track_count() - 1, -1, -1):
			var path := a.track_get_path(t)
			var bone := String(path.get_concatenated_subnames())
			var sb := ssk.find_bone(bone)
			var tb := tsk.find_bone(bone)
			if a.track_get_type(t) != Animation.TYPE_ROTATION_3D or sb < 0 or tb < 0:
				a.remove_track(t)      # positions/échelles : on garde les proportions de Claire
				continue
			# reciblage : rotation relative au repos source, réappliquée sur le repos cible
			var rs := ssk.get_bone_rest(sb).basis.get_rotation_quaternion()
			var rt := tsk.get_bone_rest(tb).basis.get_rotation_quaternion()
			for k in a.track_get_key_count(t):
				var q: Quaternion = a.track_get_key_value(t, k)
				a.track_set_key_value(t, k, rt * (rs.inverse() * q))
		lib.add_animation(n, a)
	anim.add_animation_library("loco", lib)
	src.free()
	_play("loco/Idle")

func _play(n: String) -> void:
	if n != _anim and anim and anim.has_animation(n):
		anim.play(n, 0.3)
		_anim = n

func _check_schedule(force := false) -> void:
	if state == State.TALKING:
		return
	var idx := 0
	for i in schedule.size():
		if GameClock.minute >= int(schedule[i][0]):
			idx = i
	if idx == current and not force:
		return
	current = idx
	var entry: Array = schedule[idx]
	activity = String(entry[1])
	if activity == "Fait les courses":
		_go_to(entry[2])
		_away_until = 15 * 60
	else:
		if state == State.AWAY:
			global_position = Vector3(6.5, 0.05, 12.6)
			visible = true
			talk_it.enabled = true
			Game.notify("Claire est rentrée des courses.")
		_go_to(entry[2])
	if force:
		# au chargement : elle est déjà en place
		global_position = entry[2]
		_arrive()

func _go_to(target: Vector3) -> void:
	state = State.GOING
	agent.target_position = target

func _arrive() -> void:
	var entry: Array = schedule[current]
	if activity == "Fait les courses":
		state = State.AWAY
		visible = false
		talk_it.enabled = false
		Game.notify("Claire est partie faire les courses.")
		return
	state = State.DOING
	model.rotation.y = float(entry[3])
	_play("loco/Idle")

func _physics_process(delta: float) -> void:
	match state:
		State.GOING:
			if agent.is_navigation_finished() or global_position.distance_to(agent.target_position) < 0.35:
				velocity = Vector3.ZERO
				_arrive()
				return
			var next := agent.get_next_path_position()
			var dir := next - global_position
			dir.y = 0.0
			if dir.length() > 0.01:
				dir = dir.normalized()
				model.rotation.y = lerp_angle(model.rotation.y, atan2(dir.x, dir.z), 1.0 - exp(-10.0 * delta))
			velocity = dir * SPEED
			velocity.y = -3.0
			# escalier : on monte quand le chemin s'élève nettement (marches de 20 cm)
			if next.y - global_position.y > 0.15:
				velocity.y = 1.4
			move_and_slide()
			_play("loco/Walk")
		State.TALKING:
			_talk_timer -= delta
			if Game.player:
				var to := Game.player.global_position - global_position
				model.rotation.y = lerp_angle(model.rotation.y, atan2(to.x, to.z), 1.0 - exp(-8.0 * delta))
			if _talk_timer <= 0.0:
				state = State.GOING
				_check_schedule(false)
				agent.target_position = schedule[current][2]
		_:
			velocity = Vector3.ZERO

func _on_talk(_p: Node, it: Interactable) -> void:
	state = State.TALKING
	_talk_timer = 6.0
	_play("loco/Idle")
	var line := String(schedule[current][4])
	var talks := int(Game.flags.get("talks_claire", 0))
	if talks == 0 and GameClock.hour() < 11:
		line = "Bonjour toi. Tu as bien dormi ? On a une si belle vie, tu ne trouves pas ?"
	Game.flags["talks_claire"] = talks + 1
	Game.flags["parle_claire"] = true
	Game.relations.claire = minf(100.0, float(Game.relations.claire) + 2.0)
	Game.apply({humeur = 4.0})
	Game.notify("Claire (" + activity.to_lower() + ") : « " + line + " »")
	it.action_text = "Parler"
