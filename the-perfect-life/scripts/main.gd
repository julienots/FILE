extends Node3D
## Point d'entrée. En mode « --capture », génère les captures de prévisualisation (Étape A).

var world: World
var camera: Camera3D
var wife: Node3D

func _ready() -> void:
	world = World.new()
	world.name = "World"
	add_child(world)
	camera = Camera3D.new()
	camera.current = true
	camera.near = 0.05
	camera.far = 900.0
	add_child(camera)
	_characters()
	var args := OS.get_cmdline_user_args()
	if "--capture" in args:
		var only := ""
		for a in args:
			if a.begins_with("--shot="):
				only = a.substr(7)
		var cap := preload("res://scripts/capture/capture.gd").new()
		add_child(cap)
		cap.run(camera, only)

func _characters() -> void:
	wife = Geo.prop(world.main_house, "res://assets/thirdparty/Michelle.glb", Vector3(4.2, 0.045, 1.25), 200, 0.0, 1.68)
	wife.name = "Wife"
	var ap: AnimationPlayer = wife.find_children("*", "AnimationPlayer", true, false).front() if wife.find_children("*", "AnimationPlayer", true, false).size() > 0 else null
	if ap:
		var anim := ap.get_animation_list()[0]
		ap.play(anim)
		ap.seek(0.0, true)
		ap.pause()
