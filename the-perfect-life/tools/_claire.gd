extends Node
## Vérification visuelle rapide : Claire au repos puis en marche (reciblage d'animation).
func run(main: Node) -> void:
	var w: WifeAI = main.wife
	var p: Player = main.player
	await get_tree().create_timer(0.1).timeout
	GameClock.skip_to(9 * 60 + 20)
	for i in 4:
		await get_tree().physics_frame
		print("F", i, " pos=", w.global_position.snapped(Vector3.ONE * 0.01), " vel=", w.velocity.snapped(Vector3.ONE * 0.01), " floor=", w.is_on_floor(), " state=", w.state, " coll=", w.get_slide_collision_count(), " ", (w.get_slide_collision(0).get_collider() as Node).get_parent().name if w.get_slide_collision_count() > 0 else "")
	p.place(w.global_position + Vector3(1.6, 0.0, 1.6), -PI * 0.75)
	p.cam_yaw = PI * 0.25 + PI
	for i in 6: await get_tree().process_frame
	await RenderingServer.frame_post_draw
	get_viewport().get_texture().get_image().save_png(ProjectSettings.globalize_path("res://captures/claire_marche.png"))
	print("CLAIRE ", w.activity, " ", w.state, " ", w.global_position)
	print("PATH ", w.agent.get_current_navigation_path())
	print("NEXT ", w.agent.get_next_path_position(), " finished=", w.agent.is_navigation_finished(), " target=", w.agent.target_position)
	for i in w.get_slide_collision_count():
		var c := w.get_slide_collision(i)
		print("COLL ", c.get_collider(), " ", (c.get_collider() as Node).get_parent().name if c.get_collider() else "", " n=", c.get_normal())
	get_tree().quit()
