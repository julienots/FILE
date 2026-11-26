extends Node
## Captures de prévisualisation : positionne la caméra, attend la convergence de
## l'éclairage global (SDFGI) et enregistre un PNG du rendu réel du moteur.

const FRAMES := 24
const SHOTS := [
	{name = "1_exterieur_maison", pos = Vector3(20.5, 1.65, 31.2), look = Vector3(8.0, 3.6, 10.0), fov = 52.0},
	{name = "2_salon", pos = Vector3(8.9, 1.55, 11.3), look = Vector3(13.5, 0.9, 6.0), fov = 70.0},
	{name = "3_cuisine", pos = Vector3(7.2, 1.62, 4.6), look = Vector3(2.2, 0.9, 1.2), fov = 68.0},
	{name = "4_chambre", pos = Vector3(8.9, 4.8, 11.3), look = Vector3(13.2, 3.75, 5.9), fov = 64.0},
	{name = "5_personnage", pos = Vector3(5.6, 1.6, 3.2), look = Vector3(4.2, 1.15, 1.25), fov = 50.0},
	{name = "6_quartier", pos = Vector3(-34.0, 7.5, 40.0), look = Vector3(4.0, 1.5, 22.0), fov = 55.0},
]

func run(cam: Camera3D, only := "") -> void:
	var out := ProjectSettings.globalize_path("res://captures")
	DirAccess.make_dir_recursive_absolute(out)
	print("BUILD DONE %.1fs" % (Time.get_ticks_msec() / 1000.0))
	await get_tree().process_frame
	for s in SHOTS:
		if only != "" and not String(s.name).begins_with(only):
			continue
		cam.position = s.pos
		cam.look_at(s.look)
		cam.fov = s.fov
		var t0 := Time.get_ticks_msec()
		for i in FRAMES:
			await get_tree().process_frame
			if i % 8 == 0:
				print("  frame %d  %.1fs" % [i, (Time.get_ticks_msec() - t0) / 1000.0])
		var img := get_viewport().get_texture().get_image()
		img.save_png("%s/%s.png" % [out, s.name])
		print("CAPTURE %s (%.1fs)" % [s.name, (Time.get_ticks_msec() - t0) / 1000.0])
	get_tree().quit()
