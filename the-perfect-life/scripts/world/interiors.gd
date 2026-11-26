class_name Interiors
extends RefCounted
## Aménagement intérieur de la maison principale (repère local de HouseBuilder).
## Meubles scannés Poly Haven (CC0) + éléments sur mesure (cuisine, lit, dressing, TV, tapis).

const F2 := HouseBuilder.FLOOR2

var h: Node3D
var m_white_paint: Material
var m_marble: Material
var m_brass: Material
var m_black: Material
var m_steel: Material

func _init(house: Node3D) -> void:
	h = house
	m_white_paint = MatLib.flat(Color(0.94, 0.94, 0.92), 0.38)
	m_marble = MatLib.flat(Color(0.94, 0.93, 0.91), 0.12, 0.0, 0.5)
	m_brass = MatLib.flat(Color(0.83, 0.66, 0.4), 0.28, 1.0)
	m_black = MatLib.flat(Color(0.03, 0.03, 0.035), 0.3, 0.6)
	m_steel = MatLib.brushed_metal()

func build_all() -> void:
	living()
	dining()
	kitchen()
	bedroom()
	hall()
	lights()

# ------------------------------------------------------------------ salon

func living() -> void:
	var rug := Geo.box(h, Vector3(3.6, 0.012, 2.8), Vector3(12.3, 0.046, 6.9), MatLib.fabric(Color(0.8, 0.76, 0.7), "quatrefoil_jacquard_fabric", 0.6), 0.004)
	Geo.prop(h, "sofa_02", Vector3(10.15, 0.04, 6.9), 90)
	Geo.prop(h, "throw_pillows_01", Vector3(10.2, 0.47, 6.2), 90)
	Geo.prop(h, "modern_coffee_table_01", Vector3(12.2, 0.05, 6.9), 90)
	Geo.prop(h, "sofa_03", Vector3(12.3, 0.04, 9.15), 180)
	Geo.prop(h, "modern_arm_chair_01", Vector3(12.2, 0.04, 4.9), 0)
	Geo.prop(h, "mid_century_lounge_chair", Vector3(14.7, 0.04, 10.9), -140)
	Geo.prop(h, "side_table_01", Vector3(10.1, 0.04, 8.35), 0)
	Geo.prop(h, "ceramic_vase_01", Vector3(10.1, 0.6, 8.35), 0)
	Geo.prop(h, "potted_plant_02", Vector3(15.35, 0.04, 11.35), 0)
	Geo.prop(h, "potted_plant_02", Vector3(15.35, 0.04, 5.45), 60)
	Geo.prop(h, "book_encyclopedia_set_01", Vector3(12.3, 0.45, 6.75), 90)
	# meuble TV + téléviseur sur mur droit
	Geo.prop(h, "modern_wooden_cabinet", Vector3(15.55, 0.04, 6.9), -90, 1.9)
	_tv(Vector3(15.6, 1.45, 6.9), -90, 1.45)
	Geo.prop(h, "gaming_console", Vector3(15.5, 0.62, 6.1), -90)
	# cadres entre les fenêtres de façade
	Geo.prop(h, "hanging_picture_frame_02", Vector3(12.3, 1.2, 11.82), 180)
	Geo.prop(h, "hanging_picture_frame_01", Vector3(8.09, 1.25, 10.9), 90, 0.0, 0.9)
	for wx in [10.6, 14.0]:
		curtains(Vector3(wx, 2.6, 11.83), 2.3, 2.55, 0.0)
	curtains(Vector3(15.83, 2.6, 9.0), 2.0, 2.55, 90.0)
	# lampadaire d'angle
	_floor_lamp(Vector3(8.6, 0.04, 6.0))

func _tv(pos: Vector3, rot_deg: float, width: float) -> void:
	var root := Node3D.new()
	root.name = "Television"
	h.add_child(root)
	root.position = pos
	root.rotation_degrees.y = rot_deg
	var hh := width * 9.0 / 16.0
	Geo.box(root, Vector3(width + 0.02, hh + 0.02, 0.035), Vector3.ZERO, m_black, 0.006)
	var scr := MeshInstance3D.new()
	var q := QuadMesh.new()
	q.size = Vector2(width - 0.02, hh - 0.02)
	scr.mesh = q
	var tex: Texture2D = load("res://assets/ui/tv_show.png") if ResourceLoader.exists("res://assets/ui/tv_show.png") else null
	scr.material_override = MatLib.screen(tex, 0.55)
	scr.position = Vector3(0, 0, 0.019)
	root.add_child(scr)
	var glow := OmniLight3D.new()
	glow.light_color = Color(0.55, 0.65, 0.9)
	glow.light_energy = 0.25
	glow.omni_range = 2.5
	glow.position = Vector3(0, 0, 0.5)
	root.add_child(glow)

func _floor_lamp(pos: Vector3) -> void:
	var root := Node3D.new()
	h.add_child(root)
	root.position = pos
	Geo.cylinder(root, 0.17, 0.03, Vector3(0, 0.015, 0), m_black)
	Geo.cylinder(root, 0.012, 1.45, Vector3(0, 0.75, 0), m_brass, 12)
	var shade := MeshInstance3D.new()
	var c := CylinderMesh.new()
	c.top_radius = 0.17; c.bottom_radius = 0.24; c.height = 0.32
	shade.mesh = c
	var sm := StandardMaterial3D.new()
	sm.albedo_color = Color(0.95, 0.91, 0.83)
	sm.emission_enabled = true
	sm.emission = Color(1.0, 0.78, 0.5)
	sm.emission_energy_multiplier = 0.8
	sm.cull_mode = BaseMaterial3D.CULL_DISABLED
	shade.material_override = sm
	shade.position = Vector3(0, 1.55, 0)
	root.add_child(shade)
	var l := OmniLight3D.new()
	l.light_color = Color(1.0, 0.78, 0.52)
	l.light_energy = 1.2
	l.omni_range = 4.0
	l.shadow_enabled = true
	l.position = Vector3(0, 1.5, 0)
	root.add_child(l)

## Paire de rideaux plissés (voilages lin) + tringle, de part et d'autre d'une fenêtre.
## pos : centre haut de la fenêtre, côté intérieur du mur ; rot_deg : orientation du mur.
func curtains(pos: Vector3, win_w: float, height: float, rot_deg: float) -> void:
	var root := Node3D.new()
	h.add_child(root)
	root.position = pos
	root.rotation_degrees.y = rot_deg
	var mat := MatLib.fabric(Color(0.93, 0.9, 0.84), "fabric_pattern_07", 0.15)
	var rod := Geo.cylinder(root, 0.012, win_w + 0.8, Vector3(0, 0.05, -0.12), m_brass, 12)
	rod.rotation.z = PI * 0.5
	for sx in [-1, 1]:
		var panel_w := 0.55
		var st := SurfaceTool.new()
		st.begin(Mesh.PRIMITIVE_TRIANGLES)
		var cols := 28
		var rows := 10
		for c in cols:
			for r in rows:
				var pts := []
				for d in [[0, 0], [1, 0], [1, 1], [0, 1]]:
					var u := float(c + d[0]) / cols
					var v := float(r + d[1]) / rows
					var x := u * panel_w
					var z := sin(u * PI * 14.0) * 0.035 * (0.7 + 0.3 * v)
					pts.append(Vector3(x, -v * height, z))
				for i in [0, 1, 2, 0, 2, 3]:
					st.add_vertex(pts[i])
		st.generate_normals()
		var mi := MeshInstance3D.new()
		mi.mesh = st.commit()
		mi.material_override = mat
		var cx := win_w * 0.5 + 0.05
		mi.position = Vector3(cx - panel_w * 0.3 if sx > 0 else -cx - panel_w * 0.7, 0.03, -0.12)
		root.add_child(mi)

# ------------------------------------------------------------------ salle à manger

func dining() -> void:
	var t := Geo.prop(h, "wooden_table_02", Vector3(12.0, 0.04, 2.5), 0, 2.0)
	for i in 3:
		var x := 11.25 + i * 0.75
		Geo.prop(h, "dining_chair_02", Vector3(x, 0.04, 1.75), 0)
		Geo.prop(h, "dining_chair_02", Vector3(x, 0.04, 3.25), 180)
	Geo.prop(h, "jug_01", Vector3(11.7, 0.8, 2.5), 0)
	Geo.prop(h, "wooden_bowl_01", Vector3(12.3, 0.8, 2.45), 0)
	Geo.prop(h, "food_apple_01", Vector3(12.28, 0.84, 2.42), 0)
	Geo.prop(h, "lemon", Vector3(12.36, 0.84, 2.5), 0)
	Geo.prop(h, "potted_plant_04", Vector3(15.4, 0.04, 0.6), 0)
	Geo.prop(h, "outdoor_table_chair_set_01", Vector3(12.0, 0.25, -2.2), 0)

# ------------------------------------------------------------------ cuisine

func kitchen() -> void:
	var y0 := 0.045
	# rangée basse contre le mur arrière (z = 0)
	var x := 0.65
	for w in [0.6, 0.6, 0.9, 0.9, 0.6, 0.8, 0.6]:
		_base_cabinet(Vector3(x + w * 0.5, y0, 0.12 + 0.31), w, 0.0, w > 0.85)
		x += w
	_counter(Vector3((0.13 + x) * 0.5, y0 + 0.88, 0.12 + 0.33), Vector2(x - 0.13, 0.66), 0.0)
	# évier double sous la fenêtre + robinet col de cygne
	_sink(Vector3(3.0, y0 + 0.9, 0.42))
	# plaque de cuisson + hotte inox
	_cooktop(Vector3(5.15, y0 + 0.905, 0.43))
	_hood(Vector3(5.15, 0, 0.3))
	# rangée gauche (x = 0)
	var z := 0.79
	for w in [0.6, 0.9, 0.6, 0.6]:
		_base_cabinet(Vector3(0.12 + 0.31, y0, z + w * 0.5), w, 90.0, false)
		z += w
	_counter(Vector3(0.12 + 0.33, y0 + 0.88, (0.13 + z) * 0.5 + 0.06), Vector2(z - 0.13, 0.66), 90.0)
	# placards hauts (mur arrière, sauf fenêtre et hotte)
	for seg in [[0.65, 2.05], [3.95, 4.65], [5.65, 6.45]]:
		var cx: float = seg[0]
		while cx < seg[1] - 0.05:
			var w := minf(0.6, seg[1] - cx)
			_wall_cabinet(Vector3(cx + w * 0.5, 1.5, 0.12 + 0.18), w, 0.0)
			cx += w
	# réfrigérateur américain inox
	_fridge(Vector3(0.5, y0, 4.35), 90.0)
	# îlot central marbre + tabourets
	for i in 4:
		_base_cabinet(Vector3(2.45 + i * 0.75, y0, 2.6), 0.75, 180.0, false)
	_counter(Vector3(3.575, y0 + 0.88, 2.8), Vector2(3.3, 1.05), 0.0)
	# dos et joues de l'îlot (finitions shaker côté tabourets)
	var back := Node3D.new(); h.add_child(back); back.position = Vector3(3.575, y0 + 0.44, 2.915)
	for i in 4:
		var pnl := Node3D.new(); back.add_child(pnl); pnl.position = Vector3(-1.125 + i * 0.75, 0, 0)
		_shaker_door(pnl, 0.74, 0.86, 0.0)
	for sx in [-1, 1]:
		Geo.box(h, Vector3(0.04, 0.88, 0.66), Vector3(3.575 + sx * 1.52, y0 + 0.44, 2.6), m_white_paint, 0.004)
	for i in 3:
		Geo.prop(h, "bar_chair_round_01", Vector3(2.7 + i * 0.9, y0, 3.75), 180, 0.0, 0.0)
	# accessoires
	Geo.prop(h, "wooden_cutting_board", Vector3(4.3, y0 + 0.93, 0.4), 10)
	Geo.prop(h, "bananas", Vector3(3.2, y0 + 0.93, 2.75), 30)
	Geo.prop(h, "croissant", Vector3(3.9, y0 + 0.93, 2.85), 0)
	Geo.prop(h, "tea_set_01", Vector3(1.5, y0 + 0.93, 0.42), 0)
	Geo.prop(h, "vintage_electric_kettle", Vector3(0.45, y0 + 0.93, 1.3), 90)
	Geo.prop(h, "potted_plant_04", Vector3(2.4, y0 + 0.93, 2.55), 0)
	# suspensions au-dessus de l'îlot
	for i in 3:
		_pendant(Vector3(2.5 + i * 1.1, HouseBuilder.CEIL1 - 0.1, 2.8))

func _shaker_door(root: Node3D, w: float, hh: float, zf: float) -> void:
	var rail := 0.07
	Geo.box(root, Vector3(w - 0.004, hh - 0.004, 0.018), Vector3(0, 0, zf), m_white_paint, 0.002)
	Geo.box(root, Vector3(w - 0.004, rail, 0.02), Vector3(0, hh * 0.5 - rail * 0.5, zf + 0.012), m_white_paint, 0.003)
	Geo.box(root, Vector3(w - 0.004, rail, 0.02), Vector3(0, -hh * 0.5 + rail * 0.5, zf + 0.012), m_white_paint, 0.003)
	Geo.box(root, Vector3(rail, hh - 0.004, 0.02), Vector3(-w * 0.5 + rail * 0.5, 0, zf + 0.012), m_white_paint, 0.003)
	Geo.box(root, Vector3(rail, hh - 0.004, 0.02), Vector3(w * 0.5 - rail * 0.5, 0, zf + 0.012), m_white_paint, 0.003)

## Meuble bas shaker 0,9 m (corps + porte(s) ou tiroirs, poignées laiton).
func _base_cabinet(pos: Vector3, w: float, rot_deg: float, drawers: bool) -> void:
	var root := Node3D.new()
	h.add_child(root)
	root.position = pos
	root.rotation_degrees.y = rot_deg
	var depth := 0.6
	Geo.box(root, Vector3(w - 0.004, 0.1, depth - 0.06), Vector3(0, 0.05, -0.03), MatLib.flat(Color(0.12, 0.12, 0.12), 0.7), 0.002)
	Geo.box(root, Vector3(w - 0.004, 0.76, depth - 0.02), Vector3(0, 0.48, -0.01), m_white_paint, 0.002, true)
	if drawers:
		for i in 3:
			var dh := 0.24
			var cy := 0.21 + i * 0.255
			var d := Node3D.new(); root.add_child(d); d.position = Vector3(0, cy, depth * 0.5 - 0.01)
			_shaker_door(d, w - 0.01, dh, 0.0)
			Geo.box(d, Vector3(0.22, 0.014, 0.025), Vector3(0, dh * 0.25, 0.04), m_brass, 0.005)
	else:
		var top := Node3D.new(); root.add_child(top); top.position = Vector3(0, 0.75, depth * 0.5 - 0.01)
		_shaker_door(top, w - 0.01, 0.14, 0.0)
		Geo.box(top, Vector3(0.16, 0.013, 0.025), Vector3(0, 0, 0.04), m_brass, 0.005)
		var n := 1 if w < 0.7 else 2
		for i in n:
			var dw := (w - 0.01) / n
			var dr := Node3D.new(); root.add_child(dr)
			dr.position = Vector3(-w * 0.5 + dw * (i + 0.5) + 0.005, 0.38, depth * 0.5 - 0.01)
			_shaker_door(dr, dw - 0.004, 0.53, 0.0)
			var hx := dw * 0.5 - 0.06 if (i == 0 and n == 2) or n == 1 else -dw * 0.5 + 0.06
			if n == 2 and i == 0: hx = dw * 0.5 - 0.06
			if n == 2 and i == 1: hx = -dw * 0.5 + 0.06
			Geo.box(dr, Vector3(0.013, 0.14, 0.025), Vector3(hx, 0.17, 0.04), m_brass, 0.005)

func _wall_cabinet(pos: Vector3, w: float, rot_deg: float) -> void:
	var root := Node3D.new()
	h.add_child(root)
	root.position = pos
	root.rotation_degrees.y = rot_deg
	Geo.box(root, Vector3(w - 0.004, 0.8, 0.34), Vector3(0, 0.4, 0), m_white_paint, 0.002)
	var d := Node3D.new(); root.add_child(d); d.position = Vector3(0, 0.4, 0.17)
	_shaker_door(d, w - 0.01, 0.78, 0.0)
	Geo.box(d, Vector3(0.013, 0.14, 0.025), Vector3(w * 0.5 - 0.06, -0.26, 0.04), m_brass, 0.005)
	# corniche
	Geo.box(root, Vector3(w, 0.05, 0.38), Vector3(0, 0.83, 0.02), m_white_paint, 0.008)

func _counter(center: Vector3, size: Vector2, rot_deg: float) -> void:
	var c := Geo.box(h, Vector3(size.x, 0.035, size.y), center, m_marble, 0.006, true, deg_to_rad(rot_deg))
	# crédence en carrelage métro sur le mur arrière
	if rot_deg == 0.0 and center.z < 1.0:
		Geo.box(h, Vector3(size.x, 0.58, 0.012), Vector3(center.x, center.y + 0.31, 0.135), MatLib.pbr("long_white_tiles", 0.8, Color(1, 1, 1), 0.5), 0.002)

func _sink(pos: Vector3) -> void:
	Geo.box(h, Vector3(0.82, 0.02, 0.46), pos + Vector3(0, -0.005, 0), m_steel, 0.01)
	for sx in [-0.2, 0.2]:
		Geo.box(h, Vector3(0.36, 0.012, 0.4), pos + Vector3(sx, -0.01, 0), MatLib.flat(Color(0.5, 0.5, 0.52), 0.25, 1.0), 0.04)
	var f := Node3D.new(); h.add_child(f); f.position = pos + Vector3(0, 0, -0.26)
	Geo.cylinder(f, 0.025, 0.06, Vector3(0, 0.03, 0), m_brass)
	Geo.cylinder(f, 0.014, 0.34, Vector3(0, 0.22, 0), m_brass, 16)
	var arc := Geo.box(f, Vector3(0.024, 0.024, 0.22), Vector3(0, 0.39, 0.1), m_brass, 0.01)
	Geo.cylinder(f, 0.012, 0.08, Vector3(0, 0.35, 0.2), m_brass, 16)

func _cooktop(pos: Vector3) -> void:
	Geo.box(h, Vector3(0.6, 0.008, 0.52), pos, MatLib.flat(Color(0.01, 0.01, 0.012), 0.05, 0.0, 1.0), 0.004)
	var ring := MatLib.flat(Color(0.12, 0.12, 0.13), 0.3)
	for p in [Vector2(-0.15, -0.12), Vector2(0.15, -0.12), Vector2(-0.15, 0.13), Vector2(0.15, 0.13)]:
		var t := MeshInstance3D.new()
		var tm := TorusMesh.new()
		tm.inner_radius = 0.085; tm.outer_radius = 0.092
		t.mesh = tm
		t.material_override = ring
		t.position = pos + Vector3(p.x, 0.005, p.y)
		t.scale = Vector3(1, 0.2, 1)
		h.add_child(t)

func _hood(pos: Vector3) -> void:
	var r := Node3D.new(); h.add_child(r); r.position = pos
	Geo.box(r, Vector3(0.9, 0.08, 0.5), Vector3(0, 1.62, 0.15), m_steel, 0.01)
	var body := MeshInstance3D.new()
	var c := CylinderMesh.new()
	c.top_radius = 0.18; c.bottom_radius = 0.45; c.height = 0.35; c.radial_segments = 4
	body.mesh = c
	body.material_override = m_steel
	body.position = Vector3(0, 1.83, 0.12)
	body.rotation.y = PI * 0.25
	body.scale = Vector3(1.0, 1.0, 0.6)
	r.add_child(body)
	Geo.box(r, Vector3(0.3, 0.95, 0.26), Vector3(0, 2.45, 0.02), m_steel, 0.01)

func _fridge(pos: Vector3, rot_deg: float) -> void:
	var root := Node3D.new()
	root.name = "Fridge"
	h.add_child(root)
	root.position = pos
	root.rotation_degrees.y = rot_deg
	Geo.box(root, Vector3(0.9, 1.78, 0.68), Vector3(0, 0.89, 0), m_steel, 0.015, true)
	for sx in [-1, 1]:
		var door := Node3D.new(); root.add_child(door)
		door.name = "FridgeDoor" + ("L" if sx < 0 else "R")
		door.position = Vector3(sx * 0.2245, 1.12, 0.35)
		Geo.box(door, Vector3(0.445, 1.2, 0.04), Vector3.ZERO, m_steel, 0.012)
		Geo.box(door, Vector3(0.02, 0.62, 0.035), Vector3(-sx * 0.19, 0.0, 0.05), m_steel, 0.01)
	var drawer := Node3D.new(); root.add_child(drawer); drawer.position = Vector3(0, 0.32, 0.35)
	Geo.box(drawer, Vector3(0.89, 0.58, 0.04), Vector3.ZERO, m_steel, 0.012)
	Geo.box(drawer, Vector3(0.6, 0.02, 0.035), Vector3(0, 0.22, 0.05), m_steel, 0.01)
	# distributeur d'eau
	Geo.box(root, Vector3(0.16, 0.26, 0.01), Vector3(-0.2245, 1.25, 0.375), MatLib.flat(Color(0.05, 0.05, 0.06), 0.2, 0.5), 0.01)
	# habillage placards au-dessus
	_wall_cabinet(pos + Vector3(-0.12, 1.86, 0), 0.9, rot_deg)

func _pendant(pos: Vector3) -> void:
	var r := Node3D.new(); h.add_child(r); r.position = pos
	Geo.cylinder(r, 0.004, 0.75, Vector3(0, -0.375, 0), m_black, 6)
	var shade := MeshInstance3D.new()
	var c := CylinderMesh.new()
	c.top_radius = 0.04; c.bottom_radius = 0.17; c.height = 0.22
	shade.mesh = c
	shade.material_override = MatLib.flat(Color(0.05, 0.05, 0.05), 0.35, 0.8)
	shade.position = Vector3(0, -0.86, 0)
	r.add_child(shade)
	Geo.cylinder(r, 0.05, 0.01, Vector3(0, -0.97, 0), MatLib.emissive(Color(1.0, 0.85, 0.65), 6.0))
	var l := SpotLight3D.new()
	l.light_color = Color(1.0, 0.82, 0.6)
	l.light_energy = 2.0
	l.spot_range = 3.0
	l.spot_angle = 55
	l.rotation_degrees.x = -90
	l.position = Vector3(0, -0.98, 0)
	l.shadow_enabled = true
	r.add_child(l)

# ------------------------------------------------------------------ chambre parentale

func bedroom() -> void:
	var y := F2 + 0.03
	var rug := Geo.box(h, Vector3(3.0, 0.012, 3.6), Vector3(12.0, y + 0.006, 7.6), MatLib.fabric(Color(0.82, 0.78, 0.72), "quatrefoil_jacquard_fabric", 0.5), 0.004)
	_bed(Vector3(12.0, y, 5.12))
	for sx in [-1, 1]:
		var ns := Geo.prop(h, "ClassicNightstand_01", Vector3(12.0 + sx * 1.35, y, 5.4), 0)
		_table_lamp(Vector3(12.0 + sx * 1.35, y + 0.62, 5.35))
	Geo.prop(h, "alarm_clock_01", Vector3(13.25, y + 0.62, 5.5), -20)
	_wardrobe(Vector3(8.36, y, 10.6), 90)
	Geo.prop(h, "painted_wooden_nightstand", Vector3(15.5, y, 11.2), -90)
	Geo.prop(h, "ornate_mirror_01", Vector3(15.83, y + 1.5, 6.4), -90, 0.0, 1.1)
	Geo.prop(h, "potted_plant_02", Vector3(15.35, y, 7.8), 0)
	Geo.prop(h, "standing_picture_frame_01", Vector3(10.65, y + 0.62, 5.45), 15)
	Geo.prop(h, "mid_century_lounge_chair", Vector3(14.6, y, 10.6), -135)
	Geo.prop(h, "hanging_picture_frame_01", Vector3(12.0, y + 1.75, 5.08), 0)
	Geo.prop(h, "drawer_cabinet", Vector3(15.62, y, 9.2), -90)
	Geo.prop(h, "ceramic_vase_03", Vector3(15.6, y + 0.82, 8.95), 0)
	Geo.prop(h, "book_encyclopedia_set_01", Vector3(15.6, y + 0.82, 9.5), -90)
	for wx in [10.6, 14.0]:
		curtains(Vector3(wx, y + 2.55, 11.83), 1.6, 2.5, 0.0)
	curtains(Vector3(15.83, y + 2.55, 9.0), 1.6, 2.5, 90.0)
	var cl := Geo.prop(h, "modern_ceiling_lamp_01", Vector3(12.0, HouseBuilder.TOP - 0.45, 8.3), 0)
	var l := OmniLight3D.new()
	l.light_color = Color(1.0, 0.85, 0.65); l.light_energy = 0.9; l.omni_range = 6.0
	l.position = Vector3(12.0, HouseBuilder.TOP - 0.6, 8.3)
	h.add_child(l)

func _bed(head_pos: Vector3) -> void:
	var root := Node3D.new()
	root.name = "Bed"
	h.add_child(root)
	root.position = head_pos
	var wood := MatLib.pbr("dark_wood", 1.0, Color(0.85, 0.8, 0.75), 0.8)
	var linen := MatLib.fabric(Color(0.95, 0.94, 0.92), "fabric_pattern_07", 0.18)
	var duvet := MatLib.fabric(Color(0.92, 0.92, 0.9), "quatrefoil_jacquard_fabric", 0.35)
	var head := MatLib.fabric(Color(0.42, 0.48, 0.55), "fabric_pattern_07", 0.2)
	var L := 2.15
	var Wd := 1.9
	# tête de lit capitonnée
	Geo.box(root, Vector3(Wd + 0.16, 1.25, 0.1), Vector3(0, 0.625, 0.05), head, 0.04, true)
	for r in 2:
		for c in 6:
			Geo.box(root, Vector3(0.28, 0.4, 0.05), Vector3(-0.83 + c * 0.332, 0.78 + r * 0.42, 0.11), head, 0.05)
	# cadre bois + pieds
	Geo.box(root, Vector3(Wd + 0.06, 0.3, L), Vector3(0, 0.2, 0.1 + L * 0.5), head, 0.03, true)
	for sx in [-1, 1]:
		for sz in [0.2, L - 0.05]:
			Geo.box(root, Vector3(0.06, 0.06, 0.06), Vector3(sx * (Wd * 0.5 - 0.05), 0.03, sz), wood, 0.01)
	# matelas (bords arrondis) + drap
	Geo.box(root, Vector3(Wd - 0.04, 0.24, L - 0.06), Vector3(0, 0.45, 0.12 + L * 0.5), linen, 0.06)
	# couette repliée
	var dv := Geo.box(root, Vector3(Wd + 0.06, 0.09, L * 0.66), Vector3(0, 0.6, 0.1 + L * 0.64), duvet, 0.045)
	Geo.box(root, Vector3(Wd + 0.07, 0.12, 0.28), Vector3(0, 0.62, 0.1 + L * 0.31), duvet, 0.055)
	# retombées latérales
	for sx in [-1, 1]:
		Geo.box(root, Vector3(0.05, 0.32, L * 0.66), Vector3(sx * (Wd * 0.5 + 0.04), 0.46, 0.1 + L * 0.64), duvet, 0.02)
	# oreillers
	for sx in [-0.45, 0.45]:
		var p := Geo.box(root, Vector3(0.72, 0.16, 0.46), Vector3(sx, 0.66, 0.42), linen, 0.07)
		p.rotation.x = deg_to_rad(-18)
	for sx in [-0.42, 0.42]:
		var p2 := Geo.box(root, Vector3(0.6, 0.14, 0.4), Vector3(sx, 0.7, 0.62), MatLib.fabric(Color(0.55, 0.62, 0.7), "quatrefoil_jacquard_fabric", 0.2), 0.06)
		p2.rotation.x = deg_to_rad(-25)
	# plaid au pied du lit
	Geo.box(root, Vector3(Wd + 0.1, 0.05, 0.5), Vector3(0, 0.67, 0.1 + L * 0.86), MatLib.fabric(Color(0.78, 0.66, 0.52), "fabric_pattern_07", 0.12), 0.025)
	# banc de pied de lit
	Geo.box(root, Vector3(1.4, 0.12, 0.42), Vector3(0, 0.46, L + 0.55), head, 0.04)
	for sx in [-0.62, 0.62]:
		for sz in [-0.15, 0.15]:
			Geo.box(root, Vector3(0.04, 0.4, 0.04), Vector3(sx, 0.2, L + 0.55 + sz), m_brass, 0.01)

func _table_lamp(pos: Vector3) -> void:
	var r := Node3D.new(); h.add_child(r); r.position = pos
	Geo.prop(r, "ceramic_vase_03", Vector3.ZERO, 0, 0.0, 0.34)
	var shade := MeshInstance3D.new()
	var c := CylinderMesh.new()
	c.top_radius = 0.13; c.bottom_radius = 0.18; c.height = 0.24
	shade.mesh = c
	var sm := StandardMaterial3D.new()
	sm.albedo_color = Color(0.96, 0.93, 0.86)
	sm.emission_enabled = true
	sm.emission = Color(1.0, 0.75, 0.48)
	sm.emission_energy_multiplier = 0.9
	sm.cull_mode = BaseMaterial3D.CULL_DISABLED
	shade.material_override = sm
	shade.position = Vector3(0, 0.46, 0)
	r.add_child(shade)
	var l := OmniLight3D.new()
	l.light_color = Color(1.0, 0.76, 0.5)
	l.light_energy = 0.8
	l.omni_range = 3.2
	l.shadow_enabled = true
	l.position = Vector3(0, 0.42, 0.05)
	r.add_child(l)

func _wardrobe(pos: Vector3, rot_deg: float) -> void:
	var root := Node3D.new()
	root.name = "Wardrobe"
	h.add_child(root)
	root.position = pos
	root.rotation_degrees.y = rot_deg
	var w := 2.2
	var hh := 2.35
	Geo.box(root, Vector3(w, hh, 0.6), Vector3(0, hh * 0.5, 0), m_white_paint, 0.004, true)
	for i in 4:
		var d := Node3D.new(); root.add_child(d)
		d.position = Vector3(-w * 0.5 + w * (i + 0.5) / 4.0, hh * 0.5 + 0.03, 0.31)
		_shaker_door(d, w / 4.0 - 0.006, hh - 0.12, 0.0)
		var hx := w / 8.0 - 0.06 if i % 2 == 0 else -w / 8.0 + 0.06
		Geo.box(d, Vector3(0.014, 0.3, 0.025), Vector3(hx, 0.0, 0.04), m_brass, 0.006)
	Geo.box(root, Vector3(w + 0.06, 0.06, 0.64), Vector3(0, hh + 0.03, 0.01), m_white_paint, 0.01)

# ------------------------------------------------------------------ entrée

func hall() -> void:
	Geo.prop(h, "drawer_cabinet", Vector3(7.7, 0.04, 11.3), -90)
	Geo.prop(h, "ornate_mirror_01", Vector3(7.93, 1.55, 11.3), -90, 0.0, 0.9)
	Geo.prop(h, "ceramic_vase_01", Vector3(7.7, 0.82, 11.1), 0)
	Geo.prop(h, "wall_clock", Vector3(5.08, 2.0, 11.0), 90)

# ------------------------------------------------------------------ éclairage intérieur

func lights() -> void:
	# sondes de réflexion par pièce (projection en boîte) : métal, verre, sols vernis
	for r in [[Vector3(4.0, 1.45, 2.5), Vector3(8.0, 2.9, 5.0)], [Vector3(12.0, 1.45, 8.5), Vector3(8.0, 2.9, 7.0)],
			[Vector3(12.0, 1.45, 2.5), Vector3(8.0, 2.9, 5.0)], [Vector3(12.0, F2 + 1.4, 8.5), Vector3(8.0, 2.85, 7.0)],
			[Vector3(6.5, 1.45, 8.5), Vector3(3.0, 2.9, 7.0)]]:
		var probe := ReflectionProbe.new()
		probe.position = r[0]
		probe.size = r[1]
		probe.box_projection = true
		probe.interior = true
		probe.enable_shadows = true
		probe.update_mode = ReflectionProbe.UPDATE_ONCE
		probe.ambient_mode = ReflectionProbe.AMBIENT_COLOR
		probe.ambient_color = Color(0.95, 0.88, 0.8)
		probe.ambient_color_energy = 0.8
		probe.max_distance = 30.0
		h.add_child(probe)
	for p in [Vector3(12.2, 2.8, 8.4), Vector3(6.5, 2.8, 10.5), Vector3(12.0, 2.8, 2.5), Vector3(3.2, 2.8, 2.4), Vector3(1.2, 2.8, 3.8)]:
		var l := OmniLight3D.new()
		l.light_color = Color(1.0, 0.86, 0.7)
		l.light_energy = 1.1
		l.omni_range = 7.5
		l.omni_attenuation = 1.2
		l.position = p
		h.add_child(l)
	Geo.prop(h, "ceiling_fan", Vector3(12.2, HouseBuilder.CEIL1 - 0.55, 8.4), 0)
	# chandelier salle à manger
	_pendant(Vector3(11.6, HouseBuilder.CEIL1 - 0.1, 2.5))
	_pendant(Vector3(12.4, HouseBuilder.CEIL1 - 0.1, 2.5))
