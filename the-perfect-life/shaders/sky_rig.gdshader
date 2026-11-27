shader_type spatial;
render_mode unshaded, cull_disabled, blend_mix;
// Un pan de ciel qui « tombe » : derrière, une charpente de studio et des projecteurs.

void fragment() {
	vec2 uv = UV;
	vec2 g = fract(uv * vec2(18.0, 12.0));
	float line = step(0.94, max(g.x, g.y));
	float beam = step(0.97, fract(uv.x * 4.0 + uv.y * 0.3));
	float lamp = 0.0;
	for (int i = 0; i < 6; i++) {
		vec2 c = vec2(0.12 + float(i) * 0.15, 0.82);
		lamp += smoothstep(0.035, 0.0, distance(uv, c));
	}
	vec3 col = vec3(0.025, 0.028, 0.035) + line * vec3(0.16) + beam * vec3(0.1) + lamp * vec3(1.6, 1.5, 1.2);
	float edge = smoothstep(0.0, 0.03, uv.x) * smoothstep(0.0, 0.03, 1.0 - uv.x) * smoothstep(0.0, 0.03, uv.y) * smoothstep(0.0, 0.03, 1.0 - uv.y);
	ALBEDO = col;
	ALPHA = edge;
}
