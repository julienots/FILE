extends Node
## Ciel dehors en phase 3 : avec et sans le post-process de malaise.
func run(main: Node) -> void:
	var p: Player = main.player
	await get_tree().process_frame
	Game.suspicion = 0.7
	p.place(Vector3(11.0, 0.06, 18.5), 0.0)
	p.cam_yaw = PI
	p.cam_pitch = 0.2
	for i in 8: await get_tree().process_frame
	var fx: CanvasLayer = get_tree().get_first_node_in_group("unease_fx")
	fx.visible = false
	for v in [7 * 60 + 5, 9 * 60 + 50]:
		GameClock.skip_to(v)
		for i in 3: await get_tree().process_frame
		await RenderingServer.frame_post_draw
		get_viewport().get_texture().get_image().save_png(ProjectSettings.globalize_path("res://captures/sky_fx_%s.png" % str(v)))
	print("SKY done sunrot=", main.world.sun.rotation_degrees, " sun=", main.world.sun.light_energy, " sky=", (main.world.env.sky.sky_material as PanoramaSkyMaterial).energy_multiplier)
	get_tree().quit()
