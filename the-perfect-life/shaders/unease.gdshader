shader_type canvas_item;
// Malaise visuel piloté par la suspicion : vignette, aberration chromatique, grain,
// désaturation, et coupures « glitch » (bandes décalées) pendant les anomalies.

uniform sampler2D screen_tex : hint_screen_texture, filter_linear;
uniform float suspicion : hint_range(0.0, 1.0) = 0.0;
uniform float glitch : hint_range(0.0, 1.0) = 0.0;
uniform float pulse : hint_range(0.0, 1.0) = 0.0;

float hash(vec2 p) { return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453); }

void fragment() {
	vec2 uv = SCREEN_UV;
	float t = TIME;
	// bandes décalées pendant un glitch
	float band = step(0.82, hash(vec2(floor(uv.y * 40.0), floor(t * 18.0))));
	uv.x += band * glitch * (hash(vec2(floor(t * 30.0), uv.y)) - 0.5) * 0.08;
	// aberration chromatique
	float ca = 0.0015 + suspicion * 0.004 + glitch * 0.012 + pulse * 0.006;
	vec2 dir = (uv - 0.5) * ca;
	vec3 col;
	col.r = texture(screen_tex, uv + dir).r;
	col.g = texture(screen_tex, uv).g;
	col.b = texture(screen_tex, uv - dir).b;
	// désaturation et froid progressif
	float l = dot(col, vec3(0.299, 0.587, 0.114));
	col = mix(col, vec3(l) * vec3(0.95, 1.0, 1.06), suspicion * 0.35);
	// grain
	col += (hash(uv * 900.0 + t) - 0.5) * (0.02 + suspicion * 0.05 + glitch * 0.1);
	// vignette qui se resserre
	float v = smoothstep(0.9 - suspicion * 0.25 - pulse * 0.15, 0.25, distance(uv, vec2(0.5)));
	col *= mix(1.0, v, 0.35 + suspicion * 0.45);
	COLOR = vec4(col, 1.0);
}
