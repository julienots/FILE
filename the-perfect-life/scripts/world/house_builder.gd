class_name HouseBuilder
extends Node3D
## Construit une maison américaine à deux niveaux, entièrement modélisée (murs, ouvertures,
## fenêtres vitrées, portes, escalier, planchers, toiture, perron, garage).
## Repère local : X = largeur (0..16 m), Z = profondeur (0 = arrière, 12 = façade), Y = haut.

@export var siding_tint := Color(0.93, 0.94, 0.95)
@export var roof_tint := Color(0.55, 0.56, 0.6)
@export var trim_color := Color(0.97, 0.97, 0.96)
@export var shutter_color := Color(0.09, 0.11, 0.13)
@export var door_color := Color(0.42, 0.08, 0.07)
@export var with_interior := true

const W := 16.0
const D := 12.0
const FLOOR2 := 3.15     # niveau du plancher de l'étage
const CEIL1 := 2.9       # plafond RDC
const TOP := 6.0         # haut des murs de l'étage
const EXT_T := 0.25
const INT_T := 0.12

var m_plaster: Material
var m_siding: Material
var m_trim: Material
var m_floor_wood: Material
var m_tiles: Material
var m_concrete: Material
var m_roof: Material
var m_found: Material

## Liste des fenêtres/portes générées (utilisée par le gameplay pour les interactions).
var doors: Array[Node3D] = []

func build() -> void:
	m_plaster = MatLib.paint(Color(0.9, 0.88, 0.84))
	m_siding = MatLib.pbr("white_planks_clean", 2.4, siding_tint, 1.0, 1.0)
	m_trim = MatLib.flat(trim_color, 0.45)
	m_floor_wood = MatLib.pbr("laminate_floor_02", 2.2, Color(1, 0.97, 0.93), 0.85)
	m_tiles = MatLib.pbr("large_grey_tiles", 2.0, Color(0.95, 0.95, 0.95), 0.6)
	m_concrete = MatLib.pbr("concrete_pavement", 3.0, Color(0.85, 0.85, 0.85))
	m_roof = MatLib.pbr("grey_roof_tiles", 2.5, roof_tint, 1.0, 1.2)
	m_found = MatLib.pbr("granite_tile", 1.5, Color(0.75, 0.74, 0.72))
	_floors()
	_exterior_walls()
	if with_interior:
		_interior_walls()
		_stairs()
	_roof()
	_porch()
	_garage_door()

# ---------------------------------------------------------------- murs et ouvertures

## Mur aligné sur X (z constant) ou Z (x constant). openings : [{c, w, y0, y1}] relatifs au mur.
func _wall(axis_x: bool, fixed: float, from: float, to: float, y0: float, h: float, t: float,
		openings: Array, mat: Material, collide := true) -> void:
	var ops := openings.duplicate()
	ops.sort_custom(func(a, b): return a.c < b.c)
	var cursor := from
	for o in ops:
		var a: float = o.c - o.w * 0.5
		var b: float = o.c + o.w * 0.5
		if a > cursor:
			_wall_piece(axis_x, fixed, cursor, a, y0, y0 + h, t, mat, collide)
		if o.y0 > 0.001:
			_wall_piece(axis_x, fixed, a, b, y0, y0 + o.y0, t, mat, collide)
		if o.y1 < h - 0.001:
			_wall_piece(axis_x, fixed, a, b, y0 + o.y1, y0 + h, t, mat, collide)
		cursor = b
	if cursor < to:
		_wall_piece(axis_x, fixed, cursor, to, y0, y0 + h, t, mat, collide)

func _wall_piece(axis_x: bool, fixed: float, a: float, b: float, y0: float, y1: float, t: float, mat: Material, collide: bool) -> void:
	var len := b - a
	var size := Vector3(len, y1 - y0, t) if axis_x else Vector3(t, y1 - y0, len)
	var mid := (a + b) * 0.5
	var pos := Vector3(mid, (y0 + y1) * 0.5, fixed) if axis_x else Vector3(fixed, (y0 + y1) * 0.5, mid)
	Geo.box(self, size, pos, mat, 0.004, collide)

## Mur extérieur : âme en plâtre (intérieur) + bardage extérieur décalé vers l'extérieur.
func _ext_wall(axis_x: bool, fixed: float, out_sign: float, from: float, to: float, y0: float, h: float, openings: Array) -> void:
	_wall(axis_x, fixed, from, to, y0, h, EXT_T, openings, m_plaster)
	var skin := 0.035
	_wall(axis_x, fixed + out_sign * (EXT_T * 0.5 + skin * 0.5), from - (skin if true else 0.0), to + skin, y0, h, skin, openings, m_siding, false)
	for o in openings:
		if o.get("door", false):
			_door_frame(axis_x, fixed, out_sign, o, y0)
		else:
			_window(axis_x, fixed, out_sign, o, y0)

func _window(axis_x: bool, fixed: float, out_sign: float, o: Dictionary, y0: float) -> void:
	var holder := Node3D.new()
	holder.name = "Window"
	add_child(holder)
	var w: float = o.w
	var hh: float = o.y1 - o.y0
	var cy: float = y0 + (o.y0 + o.y1) * 0.5
	var basis_pos := Vector3(o.c, cy, fixed) if axis_x else Vector3(fixed, cy, o.c)
	holder.position = basis_pos
	if not axis_x:
		holder.rotation.y = PI * 0.5
	var outdir := out_sign
	var fw := 0.07  # largeur des montants
	var depth := 0.12
	var zf := outdir * 0.02
	# cadre
	Geo.box(holder, Vector3(w, fw, depth), Vector3(0, hh * 0.5 - fw * 0.5, zf), m_trim, 0.006)
	Geo.box(holder, Vector3(w, fw, depth), Vector3(0, -hh * 0.5 + fw * 0.5, zf), m_trim, 0.006)
	Geo.box(holder, Vector3(fw, hh, depth), Vector3(-w * 0.5 + fw * 0.5, 0, zf), m_trim, 0.006)
	Geo.box(holder, Vector3(fw, hh, depth), Vector3(w * 0.5 - fw * 0.5, 0, zf), m_trim, 0.006)
	# meneaux (style colonial : petits bois)
	var mull := 0.03
	if w > 1.3:
		var n := 2 if w < 2.5 else 3
		for i in range(1, n):
			Geo.box(holder, Vector3(mull, hh - fw * 2, 0.04), Vector3(-w * 0.5 + w * i / n, 0, zf), m_trim, 0.003)
	if hh > 1.0 and not o.get("slider", false):
		Geo.box(holder, Vector3(w - fw * 2, mull * 1.6, 0.06), Vector3(0, 0, zf), m_trim, 0.004)
	# vitre
	var glass := MeshInstance3D.new()
	var q := QuadMesh.new()
	q.size = Vector2(w - fw * 2, hh - fw * 2)
	glass.mesh = q
	glass.material_override = MatLib.glass()
	glass.position = Vector3(0, 0, zf)
	glass.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	holder.add_child(glass)
	# appui extérieur + encadrement
	if o.y0 > 0.2:
		Geo.box(holder, Vector3(w + 0.16, 0.05, 0.2), Vector3(0, -hh * 0.5 - 0.03, outdir * (EXT_T * 0.5 + 0.08)), m_trim, 0.008)
	Geo.box(holder, Vector3(w + 0.2, 0.1, 0.05), Vector3(0, hh * 0.5 + 0.05, outdir * (EXT_T * 0.5 + 0.06)), m_trim, 0.006)
	for sx in [-1, 1]:
		Geo.box(holder, Vector3(0.1, hh + 0.1, 0.05), Vector3(sx * (w * 0.5 + 0.05), 0, outdir * (EXT_T * 0.5 + 0.06)), m_trim, 0.006)
	# volets décoratifs (façade)
	if o.get("shutters", false):
		var sm := MatLib.flat(shutter_color, 0.55)
		for sx in [-1, 1]:
			var sh := Node3D.new()
			holder.add_child(sh)
			sh.position = Vector3(sx * (w * 0.5 + 0.1 + 0.27), 0, outdir * (EXT_T * 0.5 + 0.07))
			Geo.box(sh, Vector3(0.5, hh + 0.1, 0.035), Vector3.ZERO, sm, 0.006)
			for k in 7:
				Geo.box(sh, Vector3(0.42, 0.025, 0.02), Vector3(0, -hh * 0.38 + k * hh * 0.127, outdir * 0.02), sm, 0.004)

func _door_frame(axis_x: bool, fixed: float, out_sign: float, o: Dictionary, y0: float) -> void:
	var holder := Node3D.new()
	holder.name = "DoorFrame"
	add_child(holder)
	var w: float = o.w
	var hh: float = o.y1
	holder.position = Vector3(o.c, y0, fixed) if axis_x else Vector3(fixed, y0, o.c)
	if not axis_x:
		holder.rotation.y = PI * 0.5
	var outdir := out_sign
	Geo.box(holder, Vector3(w + 0.24, 0.12, 0.06), Vector3(0, hh + 0.06, outdir * (EXT_T * 0.5 + 0.06)), m_trim, 0.008)
	for sx in [-1, 1]:
		Geo.box(holder, Vector3(0.12, hh, 0.06), Vector3(sx * (w * 0.5 + 0.06), hh * 0.5, outdir * (EXT_T * 0.5 + 0.06)), m_trim, 0.008)
	if o.get("panel", "") == "front":
		_front_door(holder, w, hh, outdir)

## Porte d'entrée : panneaux moulurés, poignée laiton, imposte vitrée.
func _front_door(holder: Node3D, w: float, hh: float, outdir: float) -> void:
	var dm := MatLib.flat(door_color, 0.35, 0.0, 0.6)
	var door := Node3D.new()
	door.name = "FrontDoor"
	holder.add_child(door)
	door.position = Vector3(-w * 0.5, 0, outdir * 0.02)
	var dh := hh - 0.35
	Geo.box(door, Vector3(w, dh, 0.05), Vector3(w * 0.5, dh * 0.5, 0), dm, 0.006, true)
	for r in 3:
		for c in 2:
			var ph := dh * (0.22 if r < 2 else 0.36)
			var py: float = [dh * 0.83, dh * 0.58, dh * 0.24][r]
			Geo.box(door, Vector3(w * 0.36, ph, 0.02), Vector3(w * (0.27 + c * 0.46), py, outdir * 0.03), dm, 0.008)
	var brass := MatLib.flat(Color(0.85, 0.68, 0.38), 0.25, 1.0)
	Geo.box(door, Vector3(0.03, 0.18, 0.03), Vector3(w * 0.88, 1.0, outdir * 0.05), brass, 0.008)
	Geo.cylinder(door, 0.03, 0.05, Vector3(w * 0.88, 0.95, outdir * 0.07), brass).rotation.x = PI * 0.5
	# imposte vitrée
	var g := MeshInstance3D.new()
	var q := QuadMesh.new()
	q.size = Vector2(w - 0.1, 0.25)
	g.mesh = q
	g.material_override = MatLib.glass()
	g.position = Vector3(0, hh - 0.17, outdir * 0.0)
	holder.add_child(g)
	Geo.box(holder, Vector3(w, 0.06, 0.1), Vector3(0, hh - 0.33, 0), m_trim, 0.005)
	doors.append(door)

# ---------------------------------------------------------------- volumes

func _floors() -> void:
	# fondation visible (soubassement en pierre)
	Geo.box(self, Vector3(W + 0.14, 0.45, D + 0.14), Vector3(W * 0.5, -0.2, D * 0.5), m_found, 0.02, true)
	if not with_interior:
		Geo.box(self, Vector3(W - 0.3, 0.2, D - 0.3), Vector3(W * 0.5, FLOOR2 - 0.1, D * 0.5), MatLib.flat(Color(0.2, 0.2, 0.2), 0.9))
		return
	# sols du RDC
	Geo.box(self, Vector3(W - 5.0, 0.04, D), Vector3(5.0 + (W - 5.0) * 0.5, 0.02, D * 0.5), m_floor_wood, 0.002, true)
	Geo.box(self, Vector3(8.0, 0.045, 5.0), Vector3(4.0, 0.022, 2.5), m_tiles, 0.002, true)
	Geo.box(self, Vector3(5.0, 0.04, 7.0), Vector3(2.5, 0.02, 8.5), m_concrete, 0.002, true)
	# plancher de l'étage avec trémie d'escalier (x 5..6.3, z 5.4..10)
	var slab_y := FLOOR2 - 0.12
	var ceil_mat := m_plaster
	var parts := [
		[0.0, 5.0, 0.0, D], [5.0, 6.3, 0.0, 5.4], [5.0, 6.3, 10.0, D], [6.3, W, 0.0, D],
	]
	for p in parts:
		var sx: float = p[1] - p[0]
		var sz: float = p[3] - p[2]
		Geo.box(self, Vector3(sx, 0.24, sz), Vector3((p[0] + p[1]) * 0.5, slab_y, (p[2] + p[3]) * 0.5), ceil_mat, 0.002, true)
		Geo.box(self, Vector3(sx, 0.03, sz), Vector3((p[0] + p[1]) * 0.5, FLOOR2 + 0.015, (p[2] + p[3]) * 0.5), m_floor_wood, 0.002, true)
	# plafond de l'étage
	Geo.box(self, Vector3(W, 0.15, D), Vector3(W * 0.5, TOP + 0.075, D * 0.5), m_plaster, 0.002)
	# plinthes : ajoutées par pièce via _baseboard

func _exterior_walls() -> void:
	var h1 := FLOOR2
	var h2 := TOP - FLOOR2
	# RDC
	_ext_wall(true, D, 1.0, 0.0, W, 0.0, h1, [
		{c = 2.5, w = 3.9, y0 = 0.0, y1 = 2.35, door = true},           # garage
		{c = 6.5, w = 1.05, y0 = 0.0, y1 = 2.5, door = true, panel = "front"},
		{c = 10.6, w = 2.3, y0 = 0.45, y1 = 2.45, shutters = true},
		{c = 14.0, w = 2.3, y0 = 0.45, y1 = 2.45, shutters = true},
	])
	_ext_wall(true, 0.0, -1.0, 0.0, W, 0.0, h1, [
		{c = 3.0, w = 1.8, y0 = 1.05, y1 = 2.25},
		{c = 12.0, w = 3.6, y0 = 0.0, y1 = 2.45, slider = true},
	])
	_ext_wall(false, W, 1.0, 0.0, D, 0.0, h1, [
		{c = 2.5, w = 1.6, y0 = 0.6, y1 = 2.4},
		{c = 9.0, w = 2.0, y0 = 0.45, y1 = 2.45},
	])
	_ext_wall(false, 0.0, -1.0, 0.0, D, 0.0, h1, [
		{c = 2.5, w = 1.2, y0 = 1.05, y1 = 2.25},
		{c = 8.5, w = 0.9, y0 = 1.2, y1 = 2.2},
	])
	# étage
	_ext_wall(true, D, 1.0, 0.0, W, FLOOR2, h2, [
		{c = 2.5, w = 1.5, y0 = 0.7, y1 = 2.3, shutters = true},
		{c = 6.5, w = 1.0, y0 = 0.9, y1 = 2.3, shutters = true},
		{c = 10.6, w = 1.6, y0 = 0.7, y1 = 2.3, shutters = true},
		{c = 14.0, w = 1.6, y0 = 0.7, y1 = 2.3, shutters = true},
	])
	_ext_wall(true, 0.0, -1.0, 0.0, W, FLOOR2, h2, [
		{c = 2.5, w = 0.9, y0 = 1.4, y1 = 2.3},
		{c = 9.5, w = 1.0, y0 = 0.9, y1 = 2.3},
		{c = 13.5, w = 1.6, y0 = 0.8, y1 = 2.3},
	])
	_ext_wall(false, W, 1.0, 0.0, D, FLOOR2, h2, [
		{c = 2.5, w = 1.4, y0 = 0.8, y1 = 2.3},
		{c = 9.0, w = 1.6, y0 = 0.7, y1 = 2.3},
	])
	_ext_wall(false, 0.0, -1.0, 0.0, D, FLOOR2, h2, [
		{c = 8.5, w = 1.4, y0 = 0.8, y1 = 2.3},
	])
	# planches d'angle et bandeau entre étages
	for x in [0.0, W]:
		for z in [0.0, D]:
			var ox := -0.16 if x == 0.0 else 0.16
			var oz := -0.16 if z == 0.0 else 0.16
			Geo.box(self, Vector3(0.16, TOP, 0.16), Vector3(x + ox * 0.5, TOP * 0.5, z + oz * 0.5), m_trim, 0.01)
	Geo.box(self, Vector3(W + 0.34, 0.16, 0.06), Vector3(W * 0.5, FLOOR2, D + 0.19), m_trim, 0.01)
	Geo.box(self, Vector3(W + 0.34, 0.16, 0.06), Vector3(W * 0.5, FLOOR2, -0.19), m_trim, 0.01)

func _interior_walls() -> void:
	var h1 := CEIL1
	var dy := 2.1
	# RDC
	_wall(false, 5.0, 5.0, D, 0.0, h1, INT_T, [{c = 6.0, w = 0.9, y0 = 0.0, y1 = dy}], m_plaster)           # garage | hall
	_wall(true, 5.0, 0.0, 5.0, 0.0, h1, INT_T, [], m_plaster)                                                 # cuisine | garage
	_wall(false, 8.0, 5.0, D, 0.0, h1, INT_T, [{c = 8.6, w = 2.4, y0 = 0.0, y1 = 2.5}], m_plaster)           # hall | salon (arche)
	_wall(true, 5.0, 5.0, 8.0, 0.0, h1, INT_T, [{c = 6.8, w = 1.0, y0 = 0.0, y1 = dy}], m_plaster)           # hall | cuisine
	# Étage
	var y2 := FLOOR2
	var h2 := TOP - FLOOR2
	_wall(false, 5.0, 0.0, D, y2, h2, INT_T, [{c = 3.6, w = 0.85, y0 = 0.0, y1 = dy}, {c = 7.0, w = 0.85, y0 = 0.0, y1 = dy}], m_plaster)
	_wall(true, 5.0, 0.0, 5.0, y2, h2, INT_T, [], m_plaster)
	_wall(false, 8.0, 0.0, D, y2, h2, INT_T, [{c = 6.2, w = 0.9, y0 = 0.0, y1 = dy}, {c = 2.5, w = 0.85, y0 = 0.0, y1 = dy}], m_plaster)
	_wall(true, 5.0, 8.0, W, y2, h2, INT_T, [{c = 9.5, w = 0.85, y0 = 0.0, y1 = dy}, {c = 13.5, w = 0.85, y0 = 0.0, y1 = dy}], m_plaster)
	_wall(false, 11.0, 0.0, 5.0, y2, h2, INT_T, [], m_plaster)
	# plinthes blanches
	var bb := MatLib.flat(Color(0.96, 0.96, 0.95), 0.4)
	for seg in [[true, 0.14, 5.1, W - 0.14], [true, D - 0.14, 8.1, W - 0.14], [false, W - 0.14, 0.14, D - 0.14],
			[false, 8.08, 5.1, 7.4], [false, 8.08, 9.8, D - 0.14], [true, 0.14, 0.14, 5.0], [false, 0.14, 0.14, 4.9]]:
		var axis_x: bool = seg[0]
		var len: float = seg[3] - seg[2]
		var size := Vector3(len, 0.1, 0.015) if axis_x else Vector3(0.015, 0.1, len)
		var mid: float = (seg[2] + seg[3]) * 0.5
		Geo.box(self, size, Vector3(mid, 0.09, seg[1]) if axis_x else Vector3(seg[1], 0.09, mid), bb, 0.003)
	# plinthes chambre parentale
	for seg in [[true, D - 0.14, 8.1, W - 0.14], [false, W - 0.14, 5.1, D - 0.14], [true, 5.08, 8.1, W - 0.14], [false, 8.08, 5.1, D - 0.14]]:
		var axis_x: bool = seg[0]
		var len: float = seg[3] - seg[2]
		var size := Vector3(len, 0.1, 0.015) if axis_x else Vector3(0.015, 0.1, len)
		var mid: float = (seg[2] + seg[3]) * 0.5
		Geo.box(self, size, Vector3(mid, FLOOR2 + 0.08, seg[1]) if axis_x else Vector3(seg[1], FLOOR2 + 0.08, mid), bb, 0.003)

func _stairs() -> void:
	var wood := MatLib.pbr("white_oak_veneer", 1.2, Color(0.95, 0.88, 0.78), 0.8)
	var riser := MatLib.flat(Color(0.95, 0.95, 0.94), 0.4)
	var steps := 16
	var rise := FLOOR2 / steps
	var run := 0.28
	var z0 := 10.0
	for i in steps:
		var y := rise * (i + 1)
		var z := z0 - run * (i + 0.5)
		Geo.box(self, Vector3(1.15, 0.04, run + 0.03), Vector3(5.67, y - 0.02, z), wood, 0.006, true)
		Geo.box(self, Vector3(1.1, rise - 0.04, 0.02), Vector3(5.67, y - rise * 0.5 - 0.02, z + run * 0.5), riser, 0.002)
	# limon + garde-corps côté vide
	var len := sqrt(pow(run * steps, 2) + FLOOR2 * FLOOR2)
	var ang := atan2(FLOOR2, run * steps)
	var stringer := Geo.box(self, Vector3(0.05, 0.3, len), Vector3(6.27, FLOOR2 * 0.5, z0 - run * steps * 0.5), riser, 0.006)
	stringer.rotation.x = ang
	var rail := Geo.box(self, Vector3(0.06, 0.05, len), Vector3(6.27, FLOOR2 * 0.5 + 0.95, z0 - run * steps * 0.5), wood, 0.012)
	rail.rotation.x = ang
	for i in range(0, steps, 2):
		var y := rise * (i + 1)
		var z := z0 - run * (i + 0.5)
		Geo.box(self, Vector3(0.03, 0.9, 0.03), Vector3(6.27, y + 0.45, z), riser, 0.004)
	Geo.box(self, Vector3(0.09, 1.1, 0.09), Vector3(6.27, 0.55, z0 + 0.05), riser, 0.01)

func _roof() -> void:
	var over := 0.55
	var half := D * 0.5 + over
	var pitch := deg_to_rad(32.0)
	var rise := (D * 0.5) * tan(pitch)
	var slope_len := half / cos(pitch)
	var eave_y := TOP + 0.15
	for s in [-1.0, 1.0]:
		var r := Geo.box(self, Vector3(W + over * 2, 0.12, slope_len), Vector3(W * 0.5, eave_y + rise * 0.5 - over * tan(pitch) * 0.5, D * 0.5 + s * half * 0.5), m_roof, 0.01, true)
		r.rotation.x = s * pitch
		# bordure de rive / fascia
		var f := Geo.box(self, Vector3(W + over * 2 + 0.04, 0.2, 0.04), Vector3(W * 0.5, eave_y - over * tan(pitch) - 0.08, D * 0.5 + s * (half + 0.01)), m_trim, 0.006)
	# faîtage
	Geo.box(self, Vector3(W + over * 2, 0.1, 0.32), Vector3(W * 0.5, eave_y + rise + 0.03, D * 0.5), MatLib.flat(roof_tint * 0.7, 0.8), 0.02)
	# pignons (triangles en bardage)
	for x in [0.0, W]:
		var mi := MeshInstance3D.new()
		var st := SurfaceTool.new()
		st.begin(Mesh.PRIMITIVE_TRIANGLES)
		var off := (-EXT_T * 0.5 - 0.035) if x == 0.0 else (EXT_T * 0.5 + 0.035)
		var a := Vector3(x + off, eave_y - 0.02, -0.0)
		var b := Vector3(x + off, eave_y - 0.02, D)
		var c := Vector3(x + off, eave_y + rise - 0.05, D * 0.5)
		if x == 0.0:
			st.add_vertex(a); st.add_vertex(b); st.add_vertex(c)
		else:
			st.add_vertex(a); st.add_vertex(c); st.add_vertex(b)
		st.generate_normals()
		mi.mesh = st.commit()
		mi.material_override = m_siding
		add_child(mi)
		# bande sous les pignons
		Geo.box(self, Vector3(0.26, 0.3, D + 0.3), Vector3(x + off * 0.9, eave_y - 0.1, D * 0.5), m_siding, 0.005)
		# ventilation de comble (oculus)
		var v := Geo.cylinder(self, 0.32, 0.06, Vector3(x + off * 1.2, eave_y + rise * 0.45, D * 0.5), m_trim, 40)
		v.rotation.z = PI * 0.5
	# sous-face des débords
	Geo.box(self, Vector3(W + over * 2, 0.03, over), Vector3(W * 0.5, eave_y - 0.03 - over * tan(pitch) * 0.5, D + over * 0.5), m_trim, 0.003)
	Geo.box(self, Vector3(W + over * 2, 0.03, over), Vector3(W * 0.5, eave_y - 0.03 - over * tan(pitch) * 0.5, -over * 0.5), m_trim, 0.003)
	# gouttières
	var gm := MatLib.flat(Color(0.95, 0.95, 0.95), 0.35, 0.3)
	for z in [D + over + 0.06, -over - 0.06]:
		Geo.box(self, Vector3(W + over * 2, 0.11, 0.12), Vector3(W * 0.5, eave_y - over * tan(pitch) - 0.08, z), gm, 0.02)
	for p in [Vector3(-0.25, 0, D + 0.25), Vector3(W + 0.25, 0, D + 0.25)]:
		Geo.box(self, Vector3(0.08, eave_y - 0.4, 0.08), Vector3(p.x, (eave_y - 0.4) * 0.5, p.z), gm, 0.02)
	# cheminée en brique
	var brick := MatLib.pbr("granite_tile", 1.0, Color(0.6, 0.58, 0.56))
	Geo.box(self, Vector3(1.1, 2.6, 0.9), Vector3(13.2, eave_y + rise - 0.2, 3.6), brick, 0.02)
	Geo.box(self, Vector3(1.25, 0.12, 1.05), Vector3(13.2, eave_y + rise + 1.12, 3.6), m_found, 0.01)

func _porch() -> void:
	var step_mat := m_found
	Geo.box(self, Vector3(2.6, 0.18, 1.6), Vector3(6.5, 0.09, D + 0.8), step_mat, 0.02, true)
	Geo.box(self, Vector3(2.2, 0.16, 0.4), Vector3(6.5, -0.08, D + 1.8), step_mat, 0.02, true)
	# auvent
	Geo.box(self, Vector3(2.8, 0.12, 1.7), Vector3(6.5, 2.95, D + 0.85), m_trim, 0.02)
	var cap := Geo.box(self, Vector3(2.9, 0.06, 1.85), Vector3(6.5, 3.05, D + 0.85), m_roof, 0.01)
	cap.rotation.x = deg_to_rad(-8)
	for sx in [-1, 1]:
		Geo.box(self, Vector3(0.16, 2.8, 0.16), Vector3(6.5 + sx * 1.25, 1.5, D + 1.55), m_trim, 0.015)
		Geo.box(self, Vector3(0.22, 0.12, 0.22), Vector3(6.5 + sx * 1.25, 0.24, D + 1.55), m_trim, 0.01)
	# applique murale
	var lamp_m := MatLib.flat(Color(0.05, 0.05, 0.05), 0.4, 0.8)
	for sx in [-1, 1]:
		Geo.box(self, Vector3(0.16, 0.3, 0.14), Vector3(6.5 + sx * 0.85, 2.05, D + EXT_T * 0.5 + 0.12), lamp_m, 0.01)
		var bulb := Geo.box(self, Vector3(0.1, 0.18, 0.1), Vector3(6.5 + sx * 0.85, 2.03, D + EXT_T * 0.5 + 0.13), MatLib.emissive(Color(1.0, 0.82, 0.55), 3.0), 0.01)

func _garage_door() -> void:
	var gm := MatLib.flat(Color(0.96, 0.96, 0.95), 0.5)
	var root := Node3D.new()
	root.name = "GarageDoor"
	add_child(root)
	root.position = Vector3(2.5, 0, D + 0.02)
	Geo.box(root, Vector3(3.9, 2.35, 0.05), Vector3(0, 1.175, 0), gm, 0.006, true)
	for r in 4:
		for c in 4:
			Geo.box(root, Vector3(0.82, 0.42, 0.025), Vector3(-1.44 + c * 0.96, 0.32 + r * 0.56, 0.03), gm, 0.012)
	# rangée de hublots en haut
	for c in 4:
		var g := MeshInstance3D.new()
		var q := QuadMesh.new()
		q.size = Vector2(0.7, 0.28)
		g.mesh = q
		g.material_override = MatLib.flat(Color(0.05, 0.06, 0.07), 0.05, 0.0)
		g.position = Vector3(-1.44 + c * 0.96, 2.0, 0.045)
		root.add_child(g)
	doors.append(root)
