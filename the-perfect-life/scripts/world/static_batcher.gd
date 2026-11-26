class_name StaticBatcher
extends RefCounted
## Fusionne la géométrie statique d'un sous-arbre par matériau (1 maillage = 1 draw call).
## Réduit drastiquement les appels de rendu sur mobile. Les nœuds dont le nom figure dans
## `keep` (portes, frigo… éléments interactifs) et leurs enfants ne sont pas fusionnés.
## Les collisions (StaticBody3D) sont conservées à leur position.

static func merge(root: Node3D, keep: PackedStringArray = []) -> int:
	var groups := {}       # Material -> Array[[MeshInstance3D, Transform3D]]
	var victims: Array[MeshInstance3D] = []
	var inv := root.global_transform.affine_inverse()
	_collect(root, root, keep, groups, victims)
	var merged := 0
	for mat in groups:
		var st := SurfaceTool.new()
		st.begin(Mesh.PRIMITIVE_TRIANGLES)
		for pair in groups[mat]:
			var mi: MeshInstance3D = pair[0]
			var t: Transform3D = pair[1]
			for s in mi.mesh.get_surface_count():
				st.append_from(mi.mesh, s, t)
		var out := MeshInstance3D.new()
		out.name = "Batched_%d" % merged
		out.mesh = st.commit()
		out.material_override = mat
		root.add_child(out)
		merged += 1
	for mi in victims:
		for c in mi.get_children():
			if c is StaticBody3D:
				var gt: Transform3D = (c as Node3D).global_transform
				mi.remove_child(c)
				root.add_child(c)
				(c as Node3D).global_transform = gt
		mi.get_parent().remove_child(mi)
		mi.queue_free()
	return merged

static func _collect(root: Node3D, n: Node, keep: PackedStringArray, groups: Dictionary, victims: Array[MeshInstance3D]) -> void:
	for c in n.get_children():
		if String(c.name) in keep:
			continue
		var taken := false
		if c is MeshInstance3D:
			var mi := c as MeshInstance3D
			if mi.material_override != null and mi.mesh is ArrayMesh and mi.cast_shadow == GeometryInstance3D.SHADOW_CASTING_SETTING_ON:
				var t := Geo._rel_transform(root, mi)
				if not groups.has(mi.material_override):
					groups[mi.material_override] = []
				groups[mi.material_override].append([mi, t])
				victims.append(mi)
				taken = true
		if c.get_child_count() > 0 and not taken:
			_collect(root, c, keep, groups, victims)
