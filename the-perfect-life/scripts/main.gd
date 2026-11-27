extends Node3D
## Point d'entrée.
##   (défaut)        : partie jouable (joueur, HUD, interactions, horloge)
##   --capture       : captures de prévisualisation (Étape A), --shot=1,3 pour filtrer
##   --playtest      : test de jeu automatisé avec captures d'écran

var world: World
var camera: Camera3D
var wife: Node3D
var player: Player
var hud: HUD
var interactions: HouseInteractions

func _ready() -> void:
	var args := OS.get_cmdline_user_args()
	var capture := "--capture" in args
	world = World.new()
	world.name = "World"
	world.quality = Quality.Level.ULTRA if capture else int(Game.settings.quality)
	add_child(world)
	if capture:
		_characters()
		camera = Camera3D.new()
		camera.current = true
		camera.near = 0.05
		camera.far = 900.0
		add_child(camera)
		var only := ""
		for a in args:
			if a.begins_with("--shot="):
				only = a.substr(7)
		var cap := preload("res://scripts/capture/capture.gd").new()
		add_child(cap)
		cap.run(camera, only)
		return
	Engine.max_fps = int(Game.settings.fps_cap)
	_start_game()
	if "--playtest" in args:
		var pt := preload("res://scripts/capture/playtest.gd").new()
		add_child(pt)
		pt.run(self)

func _start_game() -> void:
	interactions = HouseInteractions.new(world.main_house)
	interactions.setup()
	NavBaker.bake(world.main_house.get_parent())
	wife = WifeAI.new()
	world.main_house.get_parent().add_child(wife)
	player = Player.new()
	add_child(player)
	player.place(Vector3(13.6, HouseBuilder.FLOOR2 + 0.05, 6.6), -PI * 0.5)
	player.camera.current = true
	Game.player = player
	hud = HUD.new(player)
	add_child(hud)
	GameClock.minute = 7 * 60
	GameClock.minute_changed.emit(GameClock.day, GameClock.minute)
	Game.set_objective("Descendre prendre un café")
	Game.notify("Lundi matin. Il fait beau, comme toujours.")
	if "--continue" in OS.get_cmdline_user_args() and Game.has_save(0):
		Game.load_game(0)

func _characters() -> void:
	wife = Geo.prop(world.main_house, "res://assets/thirdparty/Michelle.glb", Vector3(4.2, 0.045, 1.3), 36, 0.0, 1.68)
	wife.name = "Wife"
	var aps := wife.find_children("*", "AnimationPlayer", true, false)
	if aps.size() > 0:
		var ap := aps[0] as AnimationPlayer
		ap.play(ap.get_animation_list()[0])
		ap.seek(0.0, true)
		ap.pause()
