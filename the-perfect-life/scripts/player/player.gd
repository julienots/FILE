class_name Player
extends CharacterBody3D
## Joueur en troisième personne : déplacement relatif à la caméra, course, caméra à bras
## élastique (anti-traversée des murs), détection de l'objet interactif le plus pertinent.

signal focus_changed(target: Interactable)

const WALK_SPEED := 1.7
const RUN_SPEED := 4.2
const ACCEL := 10.0
const GRAVITY := 9.8
const REACH := 1.7

var model: Node3D
var anim: AnimationPlayer
var pivot: Node3D
var spring: SpringArm3D
var camera: Camera3D
var cam_yaw := 0.0
var cam_pitch := -0.22
var joystick := Vector2.ZERO        # fourni par le HUD (joystick tactile)
var run_held := false
var locked := false                 # interaction en cours, téléphone, menus
var focus: Interactable
var _anim_name := ""

func _ready() -> void:
	name = "Player"
	collision_layer = 2
	collision_mask = 1
	var cs := CollisionShape3D.new()
	var cap := CapsuleShape3D.new()
	cap.radius = 0.28
	cap.height = 1.75
	cs.shape = cap
	cs.position.y = 0.875
	add_child(cs)
	model = Geo.prop(self, "res://assets/thirdparty/Soldier.glb", Vector3.ZERO, 180.0, 0.0, 1.8)
	var aps := model.find_children("*", "AnimationPlayer", true, false)
	if aps.size() > 0:
		anim = aps[0]
		for a in ["Idle", "Walk", "Run"]:
			if anim.has_animation(a):
				anim.get_animation(a).loop_mode = Animation.LOOP_LINEAR
		_play("Idle")
	pivot = Node3D.new()
	pivot.position.y = 1.5
	add_child(pivot)
	spring = SpringArm3D.new()
	spring.spring_length = 2.4
	spring.margin = 0.18
	spring.collision_mask = 1
	var sph := SphereShape3D.new()
	sph.radius = 0.2
	spring.shape = sph
	spring.add_excluded_object(get_rid())
	pivot.add_child(spring)
	camera = Camera3D.new()
	camera.fov = 68.0
	camera.near = 0.05
	camera.far = 900.0
	spring.add_child(camera)
	cam_yaw = rotation.y
	rotation.y = 0.0

## Rotation caméra (glissement du doigt / souris).
func look(delta: Vector2) -> void:
	var s: float = float(Game.settings.get("sensitivity", 1.0)) * 0.005
	cam_yaw -= delta.x * s
	cam_pitch = clampf(cam_pitch - delta.y * s, -1.1, 0.55)

func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventMouseMotion and (Input.is_mouse_button_pressed(MOUSE_BUTTON_RIGHT) or Input.mouse_mode == Input.MOUSE_MODE_CAPTURED):
		look((event as InputEventMouseMotion).relative)

func _physics_process(delta: float) -> void:
	pivot.rotation = Vector3(cam_pitch, cam_yaw, 0.0)
	var input := Vector2.ZERO
	if not locked:
		input = Input.get_vector("move_left", "move_right", "move_forward", "move_back") + joystick
		input = input.limit_length(1.0)
	var basis_y := Basis(Vector3.UP, cam_yaw)
	var dir := basis_y * Vector3(input.x, 0.0, input.y)
	var running := run_held or Input.is_action_pressed("run")
	var speed := (RUN_SPEED if running else WALK_SPEED) * minf(1.0, input.length() * 1.2)
	var target := dir.normalized() * speed if dir.length() > 0.05 else Vector3.ZERO
	velocity.x = move_toward(velocity.x, target.x, ACCEL * delta * maxf(speed, 1.0))
	velocity.z = move_toward(velocity.z, target.z, ACCEL * delta * maxf(speed, 1.0))
	if not is_on_floor():
		velocity.y -= GRAVITY * delta
	else:
		velocity.y = -0.1
	move_and_slide()
	var flat := Vector2(velocity.x, velocity.z)
	if flat.length() > 0.2:
		var want := atan2(velocity.x, velocity.z)
		model.rotation.y = lerp_angle(model.rotation.y, want, 1.0 - exp(-12.0 * delta))
	if anim:
		if flat.length() > 2.6:
			_play("Run")
		elif flat.length() > 0.25:
			_play("Walk")
		else:
			_play("Idle")
	_update_focus()

func _play(n: String) -> void:
	if n != _anim_name and anim and anim.has_animation(n):
		anim.play(n, 0.25)
		_anim_name = n

## Choisit l'objet interactif le plus proche, devant le joueur.
func _update_focus() -> void:
	var best: Interactable = null
	var best_score := INF
	if not locked:
		var fwd := Vector3(sin(model.rotation.y), 0, cos(model.rotation.y))
		var origin := global_position + Vector3.UP * 1.0
		for n in get_tree().get_nodes_in_group("interactable"):
			var it := n as Interactable
			if it == null or not it.enabled or not it.is_visible_in_tree():
				continue
			var to := it.global_position - origin
			var d := to.length()
			if d > it.radius:
				continue
			var flat_to := Vector3(to.x, 0, to.z).normalized()
			var facing := fwd.dot(flat_to)
			if facing < -0.2 and d > 0.7:
				continue
			var score := d - facing * 0.6
			if score < best_score:
				best_score = score
				best = it
	if best != focus:
		focus = best
		focus_changed.emit(focus)

func interact() -> void:
	if focus and not locked:
		focus.interact(self)
		focus_changed.emit(focus)

## Téléporte le joueur (sommeil, scripts) en regardant vers `look_dir`.
func place(pos: Vector3, yaw: float) -> void:
	global_position = pos
	velocity = Vector3.ZERO
	model.rotation.y = yaw
	cam_yaw = yaw + PI
