class_name NavBaker
extends RefCounted
## Navmesh de la maison principale, calculé au lancement depuis les collisions statiques
## (murs, sols, escalier, meubles). Utilisé par l'IA de Claire.

static func bake(root: Node3D) -> NavigationRegion3D:
	var nm := NavigationMesh.new()
	nm.agent_radius = 0.3
	nm.agent_height = 1.7
	nm.agent_max_climb = 0.25
	nm.agent_max_slope = 40.0
	nm.cell_size = 0.08
	nm.cell_height = 0.05
	nm.geometry_parsed_geometry_type = NavigationMesh.PARSED_GEOMETRY_STATIC_COLLIDERS
	nm.geometry_collision_mask = 1
	nm.filter_baking_aabb = AABB(Vector3(-1.0, -0.5, -4.0), Vector3(18.0, 7.0, 17.5))
	var region := NavigationRegion3D.new()
	region.navigation_mesh = nm
	root.add_child(region)
	var src := NavigationMeshSourceGeometryData3D.new()
	NavigationServer3D.parse_source_geometry_data(nm, src, root)
	NavigationServer3D.bake_from_source_geometry_data(nm, src)
	region.navigation_mesh = nm
	return region
