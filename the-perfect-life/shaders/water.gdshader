shader_type spatial;
render_mode blend_mix, depth_draw_always, cull_back, specular_schlick_ggx;
// Eau de piscine : petites vagues procédurales, fresnel, réfraction de l'écran, teinte selon profondeur.

uniform vec3 shallow_color : source_color = vec3(0.25, 0.75, 0.82);
uniform vec3 deep_color : source_color = vec3(0.02, 0.32, 0.45);
uniform float depth_fade = 1.6;
uniform float wave_scale = 1.4;
uniform float refraction = 0.04;
uniform sampler2D screen_tex : hint_screen_texture, filter_linear_mipmap;
uniform sampler2D depth_tex : hint_depth_texture;

float h(vec2 p, float t) {
	return sin(p.x * 3.1 + t * 1.3) * 0.5 + sin(p.y * 2.7 - t * 1.1) * 0.5
		+ sin((p.x + p.y) * 5.3 + t * 2.1) * 0.25 + sin((p.x - p.y) * 7.9 - t * 1.7) * 0.15;
}

void fragment() {
	vec3 wp = (INV_VIEW_MATRIX * vec4(VERTEX, 1.0)).xyz;
	vec2 p = wp.xz * wave_scale;
	float e = 0.05;
	float t = TIME;
	vec3 n = normalize(vec3(h(p - vec2(e, 0), t) - h(p + vec2(e, 0), t), 2.0 * e * 12.0, h(p - vec2(0, e), t) - h(p + vec2(0, e), t)));
	NORMAL = normalize(mat3(VIEW_MATRIX) * n);
	float d = texture(depth_tex, SCREEN_UV).r;
	vec4 up = INV_PROJECTION_MATRIX * vec4(SCREEN_UV * 2.0 - 1.0, d, 1.0);
	float scene_z = -up.z / up.w;
	float water_depth = clamp((scene_z - (-VERTEX.z)) / depth_fade, 0.0, 1.0);
	vec2 ruv = SCREEN_UV + n.xz * refraction;
	vec3 under = textureLod(screen_tex, ruv, 0.0).rgb;
	vec3 tint = mix(shallow_color, deep_color, water_depth);
	float fres = pow(1.0 - clamp(dot(NORMAL, VIEW), 0.0, 1.0), 4.0);
	ALBEDO = mix(under * tint * 1.2, tint, 0.35 + water_depth * 0.4);
	ROUGHNESS = 0.03;
	METALLIC = 0.0;
	SPECULAR = 0.6 + fres * 0.4;
}
