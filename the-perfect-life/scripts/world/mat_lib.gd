class_name MatLib
extends RefCounted
## Bibliothèque de matériaux PBR (textures CC0 Poly Haven : couleur, normale, ORM).
## Les matériaux d'architecture utilisent une projection triplanaire en coordonnées monde,
## ce qui garde une échelle réaliste quelle que soit la taille des murs/sols.

const TEX := "res://assets/polyhaven/textures/%s/%s_%s.jpg"

static var _cache := {}

## name : dossier Poly Haven ; size_m : taille réelle couverte par la texture (mètres).
static func pbr(name: String, size_m := 2.0, tint := Color.WHITE, rough_mul := 1.0, normal_strength := 1.0) -> ORMMaterial3D:
	var key := "%s|%s|%s|%s|%s" % [name, size_m, tint, rough_mul, normal_strength]
	if _cache.has(key):
		return _cache[key]
	var m := ORMMaterial3D.new()
	m.albedo_texture = load(TEX % [name, name, "diff"])
	m.albedo_color = tint
	m.normal_enabled = true
	m.normal_texture = load(TEX % [name, name, "nor"])
	m.normal_scale = normal_strength
	m.orm_texture = load(TEX % [name, name, "arm"])
	m.roughness = rough_mul
	m.uv1_triplanar = true
	m.uv1_world_triplanar = true
	m.uv1_triplanar_sharpness = 4.0
	m.uv1_scale = Vector3.ONE / size_m
	m.texture_filter = BaseMaterial3D.TEXTURE_FILTER_LINEAR_WITH_MIPMAPS_ANISOTROPIC
	_cache[key] = m
	return m

## Peinture murale mate : couleur propre + micro-relief (normale du plâtre).
static func paint(color: Color, normal_tex := "white_plaster_02", size_m := 1.5) -> ORMMaterial3D:
	var key := "paint|%s|%s" % [color, normal_tex]
	if _cache.has(key):
		return _cache[key]
	var m := ORMMaterial3D.new()
	m.albedo_color = color
	m.roughness = 0.92
	m.normal_enabled = true
	m.normal_texture = load(TEX % [normal_tex, normal_tex, "nor"])
	m.normal_scale = 0.35
	m.uv1_triplanar = true
	m.uv1_world_triplanar = true
	m.uv1_scale = Vector3.ONE / size_m
	_cache[key] = m
	return m

## Textile uni : couleur choisie + trame (normale d'un tissu scanné), éclairage doux.
static func fabric(color: Color, normal_tex := "fabric_pattern_07", size_m := 0.25) -> ORMMaterial3D:
	var key := "fabric|%s|%s|%s" % [color, normal_tex, size_m]
	if _cache.has(key):
		return _cache[key]
	var m := ORMMaterial3D.new()
	m.albedo_color = color
	m.roughness = 0.95
	m.normal_enabled = true
	m.normal_texture = load(TEX % [normal_tex, normal_tex, "nor"])
	m.normal_scale = 0.7
	m.uv1_triplanar = true
	m.uv1_world_triplanar = true
	m.uv1_scale = Vector3.ONE / size_m
	m.rim_enabled = true
	m.rim = 0.25
	m.rim_tint = 0.6
	m.cull_mode = BaseMaterial3D.CULL_DISABLED
	_cache[key] = m
	return m

## Matériau uni réaliste (peinture, métal, plastique…), sans texture.
static func flat(color: Color, roughness := 0.5, metallic := 0.0, clearcoat := 0.0) -> StandardMaterial3D:
	var key := "flat|%s|%s|%s|%s" % [color, roughness, metallic, clearcoat]
	if _cache.has(key):
		return _cache[key]
	var m := StandardMaterial3D.new()
	m.albedo_color = color
	m.roughness = roughness
	m.metallic = metallic
	if clearcoat > 0.0:
		m.clearcoat_enabled = true
		m.clearcoat = clearcoat
		m.clearcoat_roughness = 0.1
	_cache[key] = m
	return m

## Métal brossé : anisotropie + rugosité directionnelle.
static func brushed_metal(color := Color(0.78, 0.78, 0.8)) -> StandardMaterial3D:
	if _cache.has("brushed"):
		return _cache["brushed"]
	var m := StandardMaterial3D.new()
	m.albedo_color = color
	m.metallic = 1.0
	m.roughness = 0.32
	m.anisotropy_enabled = true
	m.anisotropy = 0.7
	_cache["brushed"] = m
	return m

static func glass() -> StandardMaterial3D:
	if _cache.has("glass"):
		return _cache["glass"]
	var m := StandardMaterial3D.new()
	m.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA_DEPTH_PRE_PASS
	m.albedo_color = Color(0.82, 0.9, 0.92, 0.16)
	m.metallic = 0.0
	m.roughness = 0.02
	m.metallic_specular = 0.9
	m.cull_mode = BaseMaterial3D.CULL_DISABLED
	_cache["glass"] = m
	return m

## Écran allumé : émission + léger reflet.
static func screen(tex: Texture2D = null, energy := 1.6) -> StandardMaterial3D:
	var m := StandardMaterial3D.new()
	m.albedo_color = Color(0.02, 0.02, 0.025)
	m.roughness = 0.08
	m.emission_enabled = true
	if tex:
		m.emission_texture = tex
		m.emission = Color.WHITE
	else:
		m.emission = Color(0.35, 0.5, 0.75)
	m.emission_energy_multiplier = energy
	return m

## Source lumineuse visible (abat-jour, plafonnier…).
static func emissive(color: Color, energy := 4.0) -> StandardMaterial3D:
	var m := StandardMaterial3D.new()
	m.albedo_color = color
	m.emission_enabled = true
	m.emission = color
	m.emission_energy_multiplier = energy
	return m

## Eau de piscine : shader dédié (vagues, fresnel, réfraction).
static func pool_water() -> ShaderMaterial:
	if _cache.has("water"):
		return _cache["water"]
	var m := ShaderMaterial.new()
	m.shader = load("res://shaders/water.gdshader")
	_cache["water"] = m
	return m
