shader_type spatial;
// Pelouse sans répétition visible : texture PBR projetée en monde, mélangée à deux échelles
// avec rotation, + variations de teinte à grande échelle (zones plus sèches / plus vertes).

uniform sampler2D albedo_tex : source_color, filter_linear_mipmap_anisotropic, repeat_enable;
uniform sampler2D normal_tex : hint_normal, filter_linear_mipmap_anisotropic, repeat_enable;
uniform sampler2D orm_tex : filter_linear_mipmap_anisotropic, repeat_enable;
uniform float tile_m = 2.4;
uniform vec3 tint_lush : source_color = vec3(0.62, 0.8, 0.5);
uniform vec3 tint_dry : source_color = vec3(0.86, 0.85, 0.6);

varying vec3 wpos;

float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
float noise(vec2 p) {
	vec2 i = floor(p); vec2 f = fract(p);
	vec2 u = f * f * (3.0 - 2.0 * f);
	return mix(mix(hash(i), hash(i + vec2(1, 0)), u.x), mix(hash(i + vec2(0, 1)), hash(i + vec2(1, 1)), u.x), u.y);
}

void vertex() {
	wpos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
}

void fragment() {
	vec2 uv1 = wpos.xz / tile_m;
	mat2 rot = mat2(vec2(0.8, 0.6), vec2(-0.6, 0.8));
	vec2 uv2 = rot * wpos.xz / (tile_m * 2.7) + 0.37;
	float m = smoothstep(0.3, 0.7, noise(wpos.xz * 0.11));
	vec3 a = mix(texture(albedo_tex, uv1).rgb, texture(albedo_tex, uv2).rgb, m);
	vec3 n = mix(texture(normal_tex, uv1).rgb, texture(normal_tex, uv2).rgb, m);
	vec3 orm = mix(texture(orm_tex, uv1).rgb, texture(orm_tex, uv2).rgb, m);
	float macro = noise(wpos.xz * 0.035) * 0.7 + noise(wpos.xz * 0.15) * 0.3;
	ALBEDO = a * mix(tint_lush, tint_dry, smoothstep(0.35, 0.85, macro)) * 1.15;
	NORMAL_MAP = n;
	AO = orm.r;
	ROUGHNESS = clamp(orm.g * 1.05, 0.0, 1.0);
	SPECULAR = 0.35;
}
