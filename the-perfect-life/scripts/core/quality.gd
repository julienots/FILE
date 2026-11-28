class_name Quality
extends RefCounted
## Niveaux de qualité graphique. Même direction artistique partout : seuls le coût GPU et
## la finesse des effets changent. ULTRA/ÉLEVÉ ciblent Forward+ (Vulkan), MOYEN/PERF le
## renderer Mobile (pas de SDFGI/SSR : l'éclairage indirect vient alors des sondes et du ciel).

enum Level { ULTRA, HIGH, MEDIUM, PERFORMANCE }

const NAMES := ["Ultra", "Élevé", "Moyen", "Performance"]

static func apply(level: int, env: Environment, sun: DirectionalLight3D, vp: Viewport) -> void:
	# rendu Mobile (téléphones) : les effets Forward+ n'existent pas, on ne les demande pas
	var fplus := RenderingServer.get_current_rendering_method() == "forward_plus"
	if not fplus:
		level = maxi(level, Level.MEDIUM)
	var ultra := level == Level.ULTRA
	var high := level <= Level.HIGH
	var mid := level <= Level.MEDIUM
	env.sdfgi_enabled = high
	env.sdfgi_cascades = 6 if ultra else 4
	env.sdfgi_min_cell_size = 0.12 if ultra else 0.15
	env.sdfgi_use_occlusion = true
	env.sdfgi_energy = 1.3 if ultra else 1.0
	env.sdfgi_bounce_feedback = 0.85
	env.ssao_enabled = mid and fplus
	env.ssao_radius = 1.2
	env.ssao_intensity = 2.2
	env.ssao_detail = 0.6
	env.ssil_enabled = high
	env.ssil_intensity = 0.8
	env.ssr_enabled = high
	env.ssr_max_steps = 96 if ultra else 48
	env.ssr_fade_in = 0.15
	env.ssr_fade_out = 2.0
	env.glow_enabled = mid
	env.volumetric_fog_enabled = ultra
	sun.shadow_enabled = true
	sun.directional_shadow_mode = DirectionalLight3D.SHADOW_PARALLEL_4_SPLITS if high else DirectionalLight3D.SHADOW_PARALLEL_2_SPLITS
	sun.directional_shadow_max_distance = 90.0 if ultra else (70.0 if high else 45.0)
	sun.light_angular_distance = 0.6 if mid else 0.0
	vp.scaling_3d_scale = 1.0 if high else (0.85 if mid else 0.7)
	vp.msaa_3d = Viewport.MSAA_4X if ultra else (Viewport.MSAA_2X if high else Viewport.MSAA_DISABLED)
	vp.screen_space_aa = Viewport.SCREEN_SPACE_AA_FXAA if mid else Viewport.SCREEN_SPACE_AA_DISABLED
	vp.use_taa = ultra
	vp.mesh_lod_threshold = 0.5 if ultra else (1.0 if high else 2.0)
	RenderingServer.directional_soft_shadow_filter_set_quality(
		RenderingServer.SHADOW_QUALITY_SOFT_HIGH if ultra else (RenderingServer.SHADOW_QUALITY_SOFT_MEDIUM if high else RenderingServer.SHADOW_QUALITY_SOFT_LOW))
	RenderingServer.positional_soft_shadow_filter_set_quality(
		RenderingServer.SHADOW_QUALITY_SOFT_MEDIUM if high else RenderingServer.SHADOW_QUALITY_SOFT_LOW)
	_tune_lights(level, vp.get_tree())

## Rendu Mobile : 8 lumières max par objet. On coupe les ombres des lampes décoratives,
## on éteint les lumières lointaines (fondu à distance) et, en Performance, les lampes déco.
static func _tune_lights(level: int, tree: SceneTree) -> void:
	if tree == null:
		return
	var high := level <= Level.HIGH
	for l in tree.get_nodes_in_group("lights_decor"):
		var li := l as Light3D
		li.shadow_enabled = high and li is SpotLight3D
		li.distance_fade_enabled = true
		li.distance_fade_begin = 9.0 if high else 6.0
		li.distance_fade_length = 3.0
		li.set_meta("allowed", level != Level.PERFORMANCE)
		if level == Level.PERFORMANCE:
			li.visible = false
	for g in ["lights_rdc", "lights_chambre"]:
		for l in tree.get_nodes_in_group(g):
			var li := l as Light3D
			li.distance_fade_enabled = true
			li.distance_fade_begin = 16.0
			li.distance_fade_length = 4.0
			if not high:
				li.shadow_enabled = false
	for l in tree.get_nodes_in_group("street_lights"):
		var li := l as Light3D
		li.distance_fade_enabled = true
		li.distance_fade_begin = 22.0 if high else 14.0
		li.distance_fade_length = 6.0

