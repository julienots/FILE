class_name UneaseFX
extends CanvasLayer
## Post-process plein écran (sous le HUD) piloté par la suspicion. Désactivé tant que tout va bien
## (aucun coût GPU en phase 1).

var rect: ColorRect
var mat: ShaderMaterial
var _glitch := 0.0
var _pulse := 0.0

func _ready() -> void:
	layer = 5
	add_to_group("unease_fx")
	rect = ColorRect.new()
	rect.set_anchors_preset(Control.PRESET_FULL_RECT)
	rect.mouse_filter = Control.MOUSE_FILTER_IGNORE
	mat = ShaderMaterial.new()
	mat.shader = load("res://shaders/unease.gdshader")
	rect.material = mat
	add_child(rect)

func glitch(strength: float) -> void:
	_glitch = maxf(_glitch, strength)
	Input.vibrate_handheld(int(120 * strength))

func pulse(strength: float) -> void:
	_pulse = maxf(_pulse, strength)

func _process(delta: float) -> void:
	_glitch = move_toward(_glitch, 0.0, delta * 0.9)
	_pulse = move_toward(_pulse, 0.0, delta * 0.7)
	var s := Game.suspicion
	rect.visible = s > 0.02 or _glitch > 0.0 or _pulse > 0.0
	mat.set_shader_parameter("suspicion", s)
	mat.set_shader_parameter("glitch", _glitch)
	mat.set_shader_parameter("pulse", _pulse)
