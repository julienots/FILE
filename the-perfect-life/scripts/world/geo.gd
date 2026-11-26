class_name Geo
extends RefCounted
## Primitives géométriques réalistes : boîtes chanfreinées (arêtes qui accrochent la lumière,
## jamais d'arêtes parfaitement vives), placement de nœuds, collisions statiques.

static var _mesh_cache := {}

## Boîte chanfreinée centrée sur l'origine. bevel en mètres.
static func bevel_box(size: Vector3, bevel := 0.01) -> ArrayMesh:
	var key := "%s|%s" % [size, bevel]
	if _mesh_cache.has(key):
		return _mesh_cache[key]
	var b := minf(bevel, minf(size.x, minf(size.y, size.z)) * 0.45)
	var h := size * 0.5
	var st := SurfaceTool.new()
	st.begin(Mesh.PRIMITIVE_TRIANGLES)
	# Pour chaque coin, 3 sommets décalés (un par axe).
	var corner := func(sx: int, sy: int, sz: int, axis: int) -> Vector3:
		var p := Vector3(sx * h.x, sy * h.y, sz * h.z)
		var q := p
		if axis != 0: q.x -= sx * b
		if axis != 1: q.y -= sy * b
		if axis != 2: q.z -= sz * b
		return q
	var quad := func(a: Vector3, bb: Vector3, c: Vector3, d: Vector3, outward: Vector3) -> void:
		var n := (bb - a).cross(c - a)
		if n.dot(outward) < 0.0:
			var t := bb; bb = d; d = t
		for v in [a, bb, c, a, c, d]:
			st.add_vertex(v)
	var tri := func(a: Vector3, bb: Vector3, c: Vector3, outward: Vector3) -> void:
		if (bb - a).cross(c - a).dot(outward) < 0.0:
			var t := bb; bb = c; c = t
		for v in [a, bb, c]:
			st.add_vertex(v)
	var s := [-1, 1]
	# 6 faces principales
	for axis in 3:
		for sgn in s:
			var pts := []
			for u in s:
				for w in s:
					var c := [0, 0, 0]
					c[axis] = sgn
					c[(axis + 1) % 3] = u
					c[(axis + 2) % 3] = w
					pts.append(corner.call(c[0], c[1], c[2], axis))
			var out := Vector3.ZERO
			out[axis] = sgn
			quad.call(pts[0], pts[1], pts[3], pts[2], out)
	# 12 chanfreins d'arêtes
	for axis in 3:
		var a1 := (axis + 1) % 3
		var a2 := (axis + 2) % 3
		for s1 in s:
			for s2 in s:
				var c0 := [0, 0, 0]; c0[axis] = -1; c0[a1] = s1; c0[a2] = s2
				var c1 := [0, 0, 0]; c1[axis] = 1; c1[a1] = s1; c1[a2] = s2
				var out := Vector3.ZERO; out[a1] = s1; out[a2] = s2
				quad.call(corner.call(c0[0], c0[1], c0[2], a1), corner.call(c1[0], c1[1], c1[2], a1),
					corner.call(c1[0], c1[1], c1[2], a2), corner.call(c0[0], c0[1], c0[2], a2), out)
	# 8 triangles de coins
	for sx in s:
		for sy in s:
			for sz in s:
				tri.call(corner.call(sx, sy, sz, 0), corner.call(sx, sy, sz, 1), corner.call(sx, sy, sz, 2), Vector3(sx, sy, sz))
	st.generate_normals()
	var m := st.commit()
	_mesh_cache[key] = m
	return m

## Ajoute une boîte chanfreinée (option : collision statique).
static func box(parent: Node3D, size: Vector3, pos: Vector3, mat: Material, bevel := 0.008, collide := false, rot_y := 0.0) -> MeshInstance3D:
	var mi := MeshInstance3D.new()
	mi.mesh = bevel_box(size, bevel)
	mi.material_override = mat
	mi.position = pos
	mi.rotation.y = rot_y
	parent.add_child(mi)
	if collide:
		var body := StaticBody3D.new()
		var cs := CollisionShape3D.new()
		var shape := BoxShape3D.new()
		shape.size = size
		cs.shape = shape
		body.add_child(cs)
		mi.add_child(body)
	return mi

static func cylinder(parent: Node3D, radius: float, height: float, pos: Vector3, mat: Material, segments := 32) -> MeshInstance3D:
	var mi := MeshInstance3D.new()
	var c := CylinderMesh.new()
	c.top_radius = radius
	c.bottom_radius = radius
	c.height = height
	c.radial_segments = segments
	mi.mesh = c
	mi.material_override = mat
	mi.position = pos
	parent.add_child(mi)
	return mi

## Instancie un modèle glTF Poly Haven : models/<name>/<name>.gltf
static func model(parent: Node3D, name: String, pos: Vector3, rot_y_deg := 0.0, scale := 1.0) -> Node3D:
	var path := "res://assets/polyhaven/models/%s/%s.gltf" % [name, name]
	var ps: PackedScene = load(path)
	if ps == null:
		push_error("Modèle introuvable : " + path)
		return null
	var n: Node3D = ps.instantiate()
	n.position = pos
	n.rotation_degrees.y = rot_y_deg
	n.scale = Vector3.ONE * scale
	parent.add_child(n)
	return n

static func scene(parent: Node3D, path: String, pos: Vector3, rot_y_deg := 0.0, scale := 1.0) -> Node3D:
	var ps: PackedScene = load(path)
	if ps == null:
		push_error("Scène introuvable : " + path)
		return null
	var n: Node3D = ps.instantiate()
	n.position = pos
	n.rotation_degrees.y = rot_y_deg
	n.scale = Vector3.ONE * scale
	parent.add_child(n)
	return n

## Boîte englobante d'un nœud (en coordonnées locales du nœud).
static func local_aabb(n: Node3D) -> AABB:
	var inv := n.global_transform.affine_inverse() if n.is_inside_tree() else Transform3D.IDENTITY
	var out := AABB()
	var first := true
	for mi in n.find_children("*", "MeshInstance3D", true, false):
		var t: Transform3D = _rel_transform(n, mi)
		var b: AABB = t * (mi as MeshInstance3D).get_aabb()
		if first:
			out = b; first = false
		else:
			out = out.merge(b)
	return out

static func _rel_transform(root: Node3D, child: Node3D) -> Transform3D:
	var t := Transform3D.IDENTITY
	var n: Node = child
	while n != root and n != null:
		if n is Node3D:
			t = (n as Node3D).transform * t
		n = n.get_parent()
	return t

## Pose un modèle : sa base est au sol en pos, centré en XZ, tourné de rot_y_deg.
## fit_len > 0 : met à l'échelle pour que la plus grande dimension horizontale vaille fit_len (m).
static func prop(parent: Node3D, path_or_name: String, pos: Vector3, rot_y_deg := 0.0, fit_len := 0.0, fit_height := 0.0) -> Node3D:
	var path := path_or_name
	if not path.begins_with("res://"):
		path = "res://assets/polyhaven/models/%s/%s.gltf" % [path_or_name, path_or_name]
	var ps: PackedScene = load(path)
	if ps == null:
		push_error("Modèle introuvable : " + path)
		return null
	var pivot := Node3D.new()
	pivot.name = path.get_file().get_basename()
	var m: Node3D = ps.instantiate()
	pivot.add_child(m)
	var b := local_aabb(pivot)
	var s := 1.0
	if fit_len > 0.0:
		s = fit_len / maxf(b.size.x, b.size.z)
	elif fit_height > 0.0:
		s = fit_height / b.size.y
	m.scale = Vector3.ONE * s
	var c := b.get_center()
	m.position = Vector3(-c.x * s, -b.position.y * s, -c.z * s)
	pivot.position = pos
	pivot.rotation_degrees.y = rot_y_deg
	parent.add_child(pivot)
	return pivot
