class_name World
extends Node3D
## Monde : ciel HDRI, soleil, éclairage global, quartier résidentiel (rue, trottoirs,
## maisons voisines), jardin (piscine, terrasse), végétation, voitures.

@export var quality := Quality.Level.ULTRA

var env: Environment
var sun: DirectionalLight3D
var main_house: HouseBuilder
var _lots: Node3D

const ROAD_Z0 := 22.3
const ROAD_Z1 := 30.3
const LOT := 26.0
## Éléments interactifs jamais fusionnés.
const KEEP := ["FrontDoor", "GarageDoor", "Fridge", "Wardrobe", "Television"]

func _ready() -> void:
	_lots = Node3D.new()
	_lots.name = "Lots"
	add_child(_lots)
	_environment()
	_ground()
	_street()
	main_house = _lot(Transform3D.IDENTITY, Color(0.93, 0.94, 0.95), Color(0.5, 0.52, 0.56), Color(0.42, 0.08, 0.07), true)
	Interiors.new(main_house).build_all()
	StaticBatcher.merge(main_house.get_parent(), PackedStringArray(KEEP))
	_backyard()
	var variants := [
		[Color(0.72, 0.8, 0.86), Color(0.36, 0.36, 0.38), Color(0.12, 0.2, 0.3)],
		[Color(0.88, 0.84, 0.74), Color(0.42, 0.3, 0.24), Color(0.1, 0.25, 0.15)],
		[Color(0.66, 0.68, 0.64), Color(0.25, 0.26, 0.28), Color(0.95, 0.95, 0.93)],
		[Color(0.95, 0.93, 0.88), Color(0.3, 0.32, 0.36), Color(0.2, 0.2, 0.22)],
	]
	var i := 0
	for x in [-LOT, LOT, -LOT * 2, LOT * 2]:
		var v: Array = variants[i % variants.size()]
		StaticBatcher.merge(_lot(Transform3D(Basis.IDENTITY, Vector3(x, 0, 0)), v[0], v[1], v[2], false).get_parent(), PackedStringArray(KEEP))
		i += 1
	for x in [-LOT * 1.5, -LOT * 0.5, LOT * 0.5, LOT * 1.5, LOT * 2.5]:
		var v: Array = variants[i % variants.size()]
		var t := Transform3D(Basis(Vector3.UP, PI), Vector3(x + 16.0 + 2.0, 0, 52.6))
		StaticBatcher.merge(_lot(t, v[0], v[1], v[2], false).get_parent(), PackedStringArray(KEEP))
		i += 1
	_cars()
	_horizon_trees()
	StaticBatcher.merge(self, PackedStringArray(KEEP + ["Lots"]))
	Quality.apply(quality, env, sun, get_viewport())

# ---------------------------------------------------------------- ciel et lumière

func _environment() -> void:
	var we := WorldEnvironment.new()
	env = Environment.new()
	var sky := Sky.new()
	var pano := PanoramaSkyMaterial.new()
	pano.panorama = load("res://assets/polyhaven/hdri/kloofendal_48d_partly_cloudy_puresky.hdr")
	pano.energy_multiplier = 1.0
	sky.sky_material = pano
	sky.radiance_size = Sky.RADIANCE_SIZE_256
	env.background_mode = Environment.BG_SKY
	env.sky = sky
	env.ambient_light_source = Environment.AMBIENT_SOURCE_SKY
	env.reflected_light_source = Environment.REFLECTION_SOURCE_SKY
	env.tonemap_mode = Environment.TONE_MAPPER_ACES
	env.tonemap_exposure = 1.0
	env.tonemap_white = 6.0
	env.glow_intensity = 0.35
	env.glow_bloom = 0.04
	env.glow_hdr_threshold = 1.2
	env.glow_blend_mode = Environment.GLOW_BLEND_MODE_SOFTLIGHT
	env.fog_enabled = true
	env.fog_mode = Environment.FOG_MODE_DEPTH
	env.fog_light_color = Color(0.74, 0.8, 0.88)
	env.fog_depth_begin = 60.0
	env.fog_depth_end = 420.0
	env.fog_density = 0.35
	env.fog_sun_scatter = 0.15
	env.fog_sky_affect = 0.0
	env.volumetric_fog_density = 0.004
	env.volumetric_fog_albedo = Color(0.9, 0.92, 0.95)
	env.volumetric_fog_length = 48.0
	env.adjustment_enabled = true
	env.adjustment_contrast = 1.06
	env.adjustment_saturation = 1.04
	we.environment = env
	# exposition automatique (comme un appareil photo) : intérieurs lisibles, extérieurs non brûlés
	var ca := CameraAttributesPractical.new()
	ca.auto_exposure_enabled = true
	ca.auto_exposure_scale = 0.4
	ca.auto_exposure_speed = 4.0
	ca.auto_exposure_min_sensitivity = 50.0
	ca.auto_exposure_max_sensitivity = 400.0
	we.camera_attributes = ca
	add_child(we)
	sun = DirectionalLight3D.new()
	sun.name = "Sun"
	sun.rotation_degrees = Vector3(-38, -32, 0)
	sun.light_color = Color(1.0, 0.95, 0.86)
	sun.light_energy = 2.2
	sun.shadow_enabled = true
	sun.shadow_blur = 1.2
	sun.directional_shadow_blend_splits = true
	add_child(sun)

# ---------------------------------------------------------------- sol, rue

func _ground() -> void:
	var grass := MatLib.grass_ground()
	# grande pelouse découpée autour de la piscine (x 3..12, z -12..-5)
	var big := 600.0
	var hole := Rect2(3.0, -12.0, 9.0, 7.0)
	var pieces := [
		Rect2(-big * 0.5, -big * 0.5, big, big * 0.5 - 12.0),                  # derrière la piscine
		Rect2(-big * 0.5, -5.0, big, ROAD_Z0 - 0.2 + 5.0),                    # jusqu'au trottoir
		Rect2(-big * 0.5, -12.0, big * 0.5 + 3.0, 7.0),                         # gauche piscine
		Rect2(12.0, -12.0, big * 0.5 - 12.0, 7.0),                              # droite piscine
		Rect2(-big * 0.5, ROAD_Z1 + 0.2, big, big * 0.5 - ROAD_Z1),           # en face
	]
	for r in pieces:
		var size := Vector3(r.size.x, 0.1, r.size.y)
		Geo.box(self, size, Vector3(r.position.x + r.size.x * 0.5, -0.05, r.position.y + r.size.y * 0.5), grass, 0.0, true)

func _street() -> void:
	var asphalt := MatLib.pbr("clean_asphalt", 4.0, Color(0.78, 0.78, 0.8), 1.0, 1.0)
	var walk := MatLib.pbr("concrete_pavement", 2.4, Color(0.9, 0.89, 0.86))
	var curb := MatLib.pbr("concrete_pavement", 1.2, Color(0.82, 0.81, 0.79))
	var len := 420.0
	Geo.box(self, Vector3(len, 0.1, ROAD_Z1 - ROAD_Z0), Vector3(0, -0.03, (ROAD_Z0 + ROAD_Z1) * 0.5), asphalt, 0.0, true)
	for side in [[20.5, 22.1], [30.5, 32.1]]:
		Geo.box(self, Vector3(len, 0.16, side[1] - side[0]), Vector3(0, 0.0, (side[0] + side[1]) * 0.5), walk, 0.01, true)
	for z in [ROAD_Z0 - 0.1, ROAD_Z1 + 0.1]:
		Geo.box(self, Vector3(len, 0.2, 0.2), Vector3(0, 0.02, z), curb, 0.03, true)
	# bandes herbeuses entre trottoir et bordure
	# marquage : axe jaune discontinu
	var yellow := MatLib.flat(Color(0.85, 0.65, 0.15), 0.7)
	var x := -len * 0.5
	while x < len * 0.5:
		Geo.box(self, Vector3(3.0, 0.005, 0.12), Vector3(x, 0.022, (ROAD_Z0 + ROAD_Z1) * 0.5), yellow, 0.0)
		x += 7.0
	# lampadaires + bornes
	x = -LOT * 2.5
	var n := 0
	while x <= LOT * 2.5:
		Geo.prop(self, "street_lamp_01", Vector3(x + 9.0, 0.08, 21.85), 180, 0.0, 6.0)
		Geo.prop(self, "street_lamp_01", Vector3(x + 22.0, 0.08, 30.75), 0, 0.0, 6.0)
		if n % 2 == 0:
			Geo.prop(self, "fire_hydrant", Vector3(x + 15.0, 0.08, 21.7), 0)
		x += LOT
		n += 1
	# la caméra cachée : un détail à remarquer (phase 2)
	var cam := Geo.prop(self, "security_camera_01", Vector3(9.0, 5.2, 21.95), 160, 0.0, 0.25)
	Geo.prop(self, "utility_box_01", Vector3(-2.2, 0.08, 21.6), 0)

## Une parcelle : maison + allée + chemin + haies + arbre de façade + boîte aux lettres.
func _lot(t: Transform3D, siding: Color, roof: Color, door: Color, interior: bool) -> HouseBuilder:
	var root := Node3D.new()
	root.name = "Lot"
	root.transform = t
	_lots.add_child(root, true)
	var hb := HouseBuilder.new()
	hb.siding_tint = siding
	hb.roof_tint = roof
	hb.door_color = door
	hb.with_interior = interior
	root.add_child(hb)
	hb.build()
	var drive := MatLib.pbr("concrete_pavement", 3.0, Color(0.95, 0.94, 0.92))
	var walk := MatLib.pbr("concrete_pavement", 2.0, Color(0.92, 0.9, 0.87))
	Geo.box(root, Vector3(4.6, 0.08, 8.6), Vector3(2.5, 0.0, 16.3), drive, 0.01, true)
	Geo.box(root, Vector3(1.3, 0.07, 6.6), Vector3(6.5, 0.0, 17.2), walk, 0.01, true)
	# massifs au pied de la façade
	var mulch: Material = MatLib.pbr("forest_floor", 2.0) if ResourceLoader.exists("res://assets/polyhaven/textures/forest_floor/forest_floor_diff.jpg") else MatLib.flat(Color(0.2, 0.13, 0.08), 0.95)
	Geo.box(root, Vector3(7.6, 0.06, 1.3), Vector3(12.0, 0.0, 12.75), mulch, 0.01)
	var shrubs := ["shrub_02", "shrub_04", "shrub_03"]
	for k in 6:
		Geo.prop(root, shrubs[k % shrubs.size()], Vector3(8.8 + k * 1.3, 0.03, 12.8), k * 47.0, 0.0, 0.9 + (k % 3) * 0.15)
	Geo.prop(root, "planter_box_01", Vector3(5.3, 0.18, 13.1), 0)
	# arbre de façade
	Geo.prop(root, "island_tree_01", Vector3(15.2, 0.0, 18.2), randf() * 360.0, 0.0, 9.5)
	# boîte aux lettres
	var mb := Node3D.new(); root.add_child(mb); mb.position = Vector3(5.2, 0, 19.9)
	Geo.box(mb, Vector3(0.09, 1.1, 0.09), Vector3(0, 0.55, 0), MatLib.flat(Color(0.95, 0.95, 0.93), 0.5), 0.01)
	Geo.box(mb, Vector3(0.22, 0.24, 0.5), Vector3(0, 1.2, 0), MatLib.flat(Color(0.08, 0.08, 0.09), 0.35, 0.7), 0.04)
	# arbustes de séparation entre parcelles
	for z in [3.0, 8.0, 15.0]:
		Geo.prop(root, "shrub_02", Vector3(-4.5, 0.0, z), z * 33.0, 0.0, 1.6)
	return hb

func _backyard() -> void:
	var deck := MatLib.pbr("wood_floor_deck", 2.0, Color(0.95, 0.9, 0.85), 0.9)
	Geo.box(self, Vector3(8.2, 0.22, 3.6), Vector3(12.0, 0.11, -1.8), deck, 0.01, true)
	# piscine
	var tile := MatLib.pbr("long_white_tiles", 0.6, Color(0.7, 0.85, 0.92), 0.4)
	var coping := MatLib.pbr("granite_tile", 1.0, Color(0.92, 0.9, 0.86))
	var r := Rect2(3.0, -12.0, 9.0, 7.0)
	var depth := 1.6
	Geo.box(self, Vector3(r.size.x, 0.1, r.size.y), Vector3(r.get_center().x, -depth, r.get_center().y), tile, 0.0)
	for side in [[Vector3(r.size.x, depth, 0.2), Vector3(r.get_center().x, -depth * 0.5, r.position.y - 0.1)],
			[Vector3(r.size.x, depth, 0.2), Vector3(r.get_center().x, -depth * 0.5, r.end.y + 0.1)],
			[Vector3(0.2, depth, r.size.y), Vector3(r.position.x - 0.1, -depth * 0.5, r.get_center().y)],
			[Vector3(0.2, depth, r.size.y), Vector3(r.end.x + 0.1, -depth * 0.5, r.get_center().y)]]:
		Geo.box(self, side[0], side[1], tile, 0.0)
	for e in [[Vector3(r.size.x + 1.0, 0.1, 0.5), Vector3(r.get_center().x, 0.02, r.position.y - 0.25)],
			[Vector3(r.size.x + 1.0, 0.1, 0.5), Vector3(r.get_center().x, 0.02, r.end.y + 0.25)],
			[Vector3(0.5, 0.1, r.size.y), Vector3(r.position.x - 0.25, 0.02, r.get_center().y)],
			[Vector3(0.5, 0.1, r.size.y), Vector3(r.end.x + 0.25, 0.02, r.get_center().y)]]:
		Geo.box(self, e[0], e[1], coping, 0.02, true)
	var water := MeshInstance3D.new()
	var p := PlaneMesh.new()
	p.size = r.size
	p.subdivide_width = 16; p.subdivide_depth = 16
	water.mesh = p
	water.material_override = MatLib.pool_water()
	water.position = Vector3(r.get_center().x, -0.12, r.get_center().y)
	add_child(water)
	# clôture bois du jardin
	var fence := MatLib.pbr("white_planks_clean", 1.5, Color(0.92, 0.9, 0.86))
	Geo.box(self, Vector3(25.0, 1.8, 0.05), Vector3(8.0, 0.9, -16.0), fence, 0.005, true)
	for x in range(-4, 21, 2):
		Geo.box(self, Vector3(0.1, 1.9, 0.1), Vector3(x, 0.95, -15.95), fence, 0.01)
	Geo.prop(self, "island_tree_01", Vector3(16.5, 0, -13.0), 40, 0.0, 7.0)
	Geo.prop(self, "potted_plant_02", Vector3(15.6, 0.22, -0.4), 0)

func _cars() -> void:
	Geo.prop(self, "res://assets/drive/realistic_supercar.glb", Vector3(2.5, 0.04, 17.0), 90, 4.6)
	Geo.prop(self, "res://assets/drive/realistic_sports_car.glb", Vector3(LOT + 2.5, 0.04, 16.5), 90, 4.5)
	Geo.prop(self, "res://assets/drive/white_sports_car.glb", Vector3(-20.0, 0.0, 29.0), 0, 4.5)

func _horizon_trees() -> void:
	var names := ["island_tree_01", "jacaranda_tree", "tree_small_02"]
	var rng := RandomNumberGenerator.new()
	rng.seed = 7
	for k in 16:
		var x := -80.0 + k * 10.5 + rng.randf_range(-2.0, 2.0)
		for z in [-24.0 - rng.randf() * 6.0, 66.0 + rng.randf() * 6.0]:
			var name: String = names[k % names.size()]
			var t := Geo.prop(self, name, Vector3(x, 0, z), rng.randf() * 360.0, 0.0, rng.randf_range(8.0, 12.0))
			Geo.far_settings(t, 260.0, false)
