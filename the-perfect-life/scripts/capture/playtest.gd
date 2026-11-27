extends Node
## Test de jeu automatisé : déplacement, interactions, téléphone, pause, nuit, sauvegarde.
## Affiche PASS/FAIL et enregistre des captures dans captures/playtest/.

var main: Node
var out := ""
var fails := 0

func run(m: Node) -> void:
	main = m
	out = ProjectSettings.globalize_path("res://captures/playtest")
	DirAccess.make_dir_recursive_absolute(out)
	var p: Player = main.player
	var hud: HUD = main.hud
	await _frames(10)
	await _shot("01_reveil_chambre")

	# 1. déplacement : le joystick fait avancer le joueur, les murs bloquent
	var start := p.global_position
	p.cam_yaw = PI * 0.5
	p.joystick = Vector2(0, -1)
	await _frames(12)
	p.joystick = Vector2.ZERO
	_check("le joueur se déplace", p.global_position.distance_to(start) > 0.3, "%.2f m" % p.global_position.distance_to(start))
	_check("le joueur reste dans la maison (collisions)", p.global_position.x > 8.0 and p.global_position.x < 16.0, str(p.global_position))

	# 2. cuisine : focus sur la bouilloire + effet sur l'énergie
	p.place(Vector3(1.25, 0.06, 1.35), -PI * 0.5)
	await _frames(4)
	_check("focus sur la bouilloire", p.focus != null and p.focus.display_name == "Bouilloire", p.focus.display_name if p.focus else "aucun")
	var e0 := float(Game.needs.energie)
	p.interact()
	_check("le café donne de l'énergie", float(Game.needs.energie) > e0, "%.0f -> %.0f" % [e0, float(Game.needs.energie)])
	await _frames(6)
	await _shot("02_cuisine_action")

	# 3. frigo : ouvrir puis manger
	p.place(Vector3(1.45, 0.06, 4.35), -PI * 0.5)
	await _frames(4)
	_check("focus sur le réfrigérateur", p.focus != null and p.focus.display_name == "Réfrigérateur", p.focus.display_name if p.focus else "aucun")
	p.interact()
	await _frames(8)
	await _shot("03_frigo_ouvert")
	var f0 := float(Game.needs.faim)
	p.interact()
	_check("manger réduit la faim", float(Game.needs.faim) > f0, "%.0f -> %.0f" % [f0, float(Game.needs.faim)])

	# 4. téléphone
	hud.phone.toggle(p)
	await _frames(6)
	await _shot("04_telephone_accueil")
	hud.phone.open_app("Messages")
	await _frames(2)
	hud.phone._thread("Claire")
	await _frames(4)
	await _shot("05_telephone_messages")
	hud.phone.open_app("Carte")
	await _frames(6)
	await _shot("06_telephone_carte")
	hud.phone.open_app("Calendrier")
	await _frames(3)
	await _shot("07_telephone_calendrier")
	hud.phone.close()
	await _frames(4)

	# 5. salon : télé + canapé
	p.place(Vector3(11.4, 0.06, 6.9), -PI * 0.5)
	await _frames(4)
	_check("focus sur le canapé", p.focus != null and p.focus.display_name == "Canapé", p.focus.display_name if p.focus else "aucun")
	p.interact()
	await _frames(8)
	await _shot("08_canape_tele")
	p.interact()
	GameClock.speed = 1.0

	# 6. sauvegarde / chargement
	Game.save_game(2)
	var saved_min := GameClock.minute
	GameClock.advance(90)
	Game.load_game(2)
	_check("sauvegarde puis chargement", GameClock.minute == saved_min, "%d vs %d" % [GameClock.minute, saved_min])

	# 7. menu pause
	hud.pause_menu.open()
	await _frames(3)
	await _shot("09_pause")
	hud.pause_menu.close()

	# 8. nuit dehors
	GameClock.skip_to(21 * 60 + 30)
	p.place(Vector3(9.5, 0.06, 19.5), PI)
	p.cam_pitch = -0.12
	await _frames(14)
	await _shot("10_nuit_facade")
	print("PLAYTEST %s (%d échec(s))" % ["OK" if fails == 0 else "ÉCHEC", fails])
	get_tree().quit()

func _frames(n: int) -> void:
	for i in n:
		await get_tree().process_frame

func _shot(name: String) -> void:
	await RenderingServer.frame_post_draw
	get_viewport().get_texture().get_image().save_png("%s/%s.png" % [out, name])
	print("SHOT ", name)

func _check(what: String, ok: bool, detail := "") -> void:
	if not ok:
		fails += 1
	print("%s  %s  (%s)" % ["PASS" if ok else "FAIL", what, detail])
