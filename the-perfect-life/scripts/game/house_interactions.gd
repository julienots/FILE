class_name HouseInteractions
extends RefCounted
## Branche les interactions de la maison principale : portes, frigo, cuisine, TV, canapé,
## lit, interrupteurs, dressing, garage. Chaque action a un effet réel (besoins, temps, état).

const F2 := HouseBuilder.FLOOR2

var h: Node3D
var tv_on := true
var seated := false
var _tv_screen: MeshInstance3D
var _tv_light: OmniLight3D
var _tv_on_mat: Material
var _tv_off_mat: Material

func _init(house: Node3D) -> void:
	h = house

func setup() -> void:
	_doors()
	_fridge()
	_kitchen()
	_tv_and_sofa()
	_bedroom()
	_switches()
	_garage()

# ---------------------------------------------------------------- portes

func _doors() -> void:
	for leaf in h.find_children("DoorLeaf", "", true, false):
		var l := leaf as Node3D
		_door(l, l.to_global(Vector3(0.4, 1.0, 0.0)) - h.global_position, "Porte", deg_to_rad(95.0), true)
	var front := h.find_children("FrontDoor", "", true, false)
	if front.size() > 0:
		var fd := front[0] as Node3D
		_door(fd, fd.to_global(Vector3(0.5, 1.0, 0.0)) - h.global_position, "Porte d'entrée", deg_to_rad(100.0), false)

## Porte pivotante : bascule ouverte/fermée avec une animation amortie.
func _door(leaf: Node3D, anchor: Vector3, label: String, angle: float, starts_open: bool) -> void:
	var closed_rot := leaf.rotation.y - (angle if starts_open else 0.0)
	var open_rot := closed_rot + angle
	var state := {open = starts_open}
	Interactable.attach(h, anchor, label, "Fermer" if starts_open else "Ouvrir", func(_p: Node, it: Interactable) -> void:
		state.open = not state.open
		var tw := leaf.create_tween()
		tw.tween_property(leaf, "rotation:y", open_rot if state.open else closed_rot, 0.7).set_trans(Tween.TRANS_CUBIC).set_ease(Tween.EASE_OUT)
		it.action_text = "Fermer" if state.open else "Ouvrir"
	, 1.6)

func _fridge() -> void:
	var fr := h.find_children("Fridge", "", true, false)
	if fr.is_empty():
		return
	var fridge := fr[0] as Node3D
	var hinges: Array[Node3D] = []
	for side in ["L", "R"]:
		var door := fridge.get_node_or_null("FridgeDoor" + side) as Node3D
		if door == null:
			continue
		var sx := -1.0 if side == "L" else 1.0
		var hinge := Node3D.new()
		hinge.position = door.position + Vector3(sx * 0.2225, 0, 0)
		fridge.add_child(hinge)
		door.reparent(hinge, true)
		hinge.set_meta("dir", -sx)
		hinges.append(hinge)
	var state := {step = 0}
	Interactable.attach(h, fridge.to_global(Vector3(0, 1.1, 0.6)) - h.global_position, "Réfrigérateur", "Ouvrir", func(_p: Node, it: Interactable) -> void:
		match int(state.step):
			0:
				for hg in hinges:
					hg.create_tween().tween_property(hg, "rotation:y", float(hg.get_meta("dir")) * deg_to_rad(105.0), 0.6).set_trans(Tween.TRANS_CUBIC)
				state.step = 1
				it.action_text = "Prendre à manger"
			1:
				Game.apply({faim = 30.0, humeur = 3.0})
				GameClock.advance(10)
				Game.notify("Un yaourt et des fruits. Faim +30")
				Game.flags["a_mange"] = true
				state.step = 2
				it.action_text = "Fermer"
			_:
				for hg in hinges:
					hg.create_tween().tween_property(hg, "rotation:y", 0.0, 0.5).set_trans(Tween.TRANS_CUBIC)
				state.step = 0
				it.action_text = "Ouvrir"
	)

func _kitchen() -> void:
	Interactable.attach(h, Vector3(0.45, 1.0, 1.3), "Bouilloire", "Préparer un café", func(_p: Node, it: Interactable) -> void:
		Game.apply({energie = 15.0, humeur = 4.0})
		GameClock.advance(5)
		Game.flags["cafe"] = true
		Game.notify("Café chaud. Énergie +15")
	, 1.4)
	Interactable.attach(h, Vector3(5.15, 1.0, 0.5), "Plaque de cuisson", "Préparer le petit-déjeuner", func(_p: Node, it: Interactable) -> void:
		Game.fade(func() -> void:
			GameClock.advance(20)
			Game.apply({faim = 45.0, humeur = 6.0})
			Game.flags["petit_dejeuner"] = true
		)
		Game.notify("Œufs brouillés et toasts. Faim +45")
	, 1.4)
	Interactable.attach(h, Vector3(3.0, 1.0, 0.5), "Évier", "Se laver les mains", func(_p: Node, it: Interactable) -> void:
		Game.apply({hygiene = 8.0})
		GameClock.advance(2)
		Game.notify("Hygiène +8")
	, 1.3)

func _tv_and_sofa() -> void:
	var tvs := h.find_children("Television", "", true, false)
	if tvs.size() > 0:
		var tv := tvs[0] as Node3D
		for c in tv.get_children():
			if c is MeshInstance3D and (c as MeshInstance3D).mesh is QuadMesh:
				_tv_screen = c
			if c is OmniLight3D:
				_tv_light = c
		if _tv_screen:
			_tv_on_mat = _tv_screen.material_override
			_tv_off_mat = MatLib.flat(Color(0.02, 0.02, 0.025), 0.06, 0.0, 0.8)
		Interactable.attach(h, tv.to_global(Vector3(0, -0.6, 0.5)) - h.global_position, "Télévision", "Éteindre", func(_p: Node, it: Interactable) -> void:
			set_tv(not tv_on)
			it.action_text = "Éteindre" if tv_on else "Allumer"
		, 1.8)
	Interactable.attach(h, Vector3(10.4, 0.6, 6.9), "Canapé", "Regarder la télé", func(p: Node, it: Interactable) -> void:
		var pl := p as Player
		seated = not seated
		if seated:
			set_tv(true)
			pl.place(Vector3(10.35, 0.05, 6.9), PI * 0.5)
			pl.model.visible = false
			pl.spring.spring_length = 0.0
			pl.pivot.position.y = 1.12
			GameClock.speed = 10.0
			Game.apply({humeur = 5.0})
			it.action_text = "Se lever"
			it.radius = 3.0
			Game.flags["tele"] = true
		else:
			pl.model.visible = true
			pl.spring.spring_length = 2.4
			pl.pivot.position.y = 1.5
			GameClock.speed = 1.0
			pl.place(Vector3(11.3, 0.05, 7.9), PI * 0.5)
			pl.cam_yaw = -PI * 0.25
			it.action_text = "Regarder la télé"
			it.radius = 1.7
	)

func set_tv(on: bool) -> void:
	tv_on = on
	if _tv_screen:
		_tv_screen.material_override = _tv_on_mat if on else _tv_off_mat
	if _tv_light:
		_tv_light.visible = on

func _bedroom() -> void:
	Interactable.attach(h, Vector3(12.0, F2 + 0.6, 6.6), "Lit", "Dormir", func(p: Node, it: Interactable) -> void:
		var pl := p as Player
		var night := GameClock.hour() >= 20 or GameClock.hour() < 5
		Game.fade(func() -> void:
			if night or float(Game.needs.sommeil) < 45.0:
				GameClock.skip_to(7 * 60)
				Game.apply({sommeil = 100.0, energie = 60.0, humeur = 5.0})
				Game.flags["a_dormi"] = int(Game.flags.get("a_dormi", 0)) + 1
				Game.save_game(0)
				Game.notify("Nouvelle journée. Partie sauvegardée.")
			else:
				GameClock.advance(60)
				Game.apply({sommeil = 25.0, energie = 15.0})
				Game.notify("Petite sieste. Sommeil +25")
			pl.place(Vector3(13.6, F2 + 0.05, 6.6), -PI * 0.5)
		)
	, 2.0)
	Interactable.attach(h, Vector3(8.95, F2 + 1.0, 10.6), "Dressing", "S'habiller", func(_p: Node, it: Interactable) -> void:
		Game.flags["habille"] = true
		Game.apply({humeur = 4.0})
		GameClock.advance(5)
		Game.notify("Chemise repassée, prêt pour la journée.")
	, 1.5)

func _switches() -> void:
	for sw in [[Vector3(7.88, 1.25, 11.2), "lights_rdc", "Interrupteur du salon"], [Vector3(8.15, F2 + 1.25, 6.75), "lights_chambre", "Interrupteur de la chambre"]]:
		var group: String = sw[1]
		var plate := Geo.box(h, Vector3(0.08, 0.12, 0.015), sw[0], MatLib.flat(Color(0.95, 0.95, 0.93), 0.4), 0.004)
		plate.rotation.y = PI * 0.5
		var state := {on = true}
		Interactable.attach(h, sw[0], String(sw[2]), "Éteindre", func(_p: Node, it: Interactable) -> void:
			state.on = not state.on
			for l in h.get_tree().get_nodes_in_group(group):
				(l as Light3D).visible = state.on
			it.action_text = "Éteindre" if state.on else "Allumer"
		, 1.3)

func _garage() -> void:
	var gd := h.find_children("GarageDoor", "", true, false)
	if gd.is_empty():
		return
	var door := gd[0] as Node3D
	var base := door.position
	var state := {open = false}
	Interactable.attach(h, Vector3(2.5, 1.0, 12.6), "Porte de garage", "Ouvrir", func(_p: Node, it: Interactable) -> void:
		state.open = not state.open
		var tw := door.create_tween().set_parallel(true)
		tw.tween_property(door, "position", base + (Vector3(0, 2.2, -0.35) if state.open else Vector3.ZERO), 2.2).set_trans(Tween.TRANS_SINE)
		tw.tween_property(door, "rotation:x", -PI * 0.5 if state.open else 0.0, 2.2).set_trans(Tween.TRANS_SINE)
		it.action_text = "Fermer" if state.open else "Ouvrir"
	, 2.5)
