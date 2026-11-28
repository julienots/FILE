class_name AudioDirector
extends Node
## Bande-son 100 % procédurale (aucun fichier audio) :
## nappe douce et rassurante → drone grave qui bat → acouphène aigu, selon la suspicion.
## Coups (« stingers ») sur les anomalies, bourdonnement électrique pendant les coupures.

const RATE := 16000.0

var player: AudioStreamPlayer
var gen: AudioStreamGeneratorPlayback
var _ph := PackedFloat32Array([0, 0, 0, 0, 0, 0, 0, 0])
var _sting := 0.0
var _hum := 0.0
var _t := 0.0
var _lp := 0.0

func _ready() -> void:
	add_to_group("audio_director")
	var st := AudioStreamGenerator.new()
	st.mix_rate = RATE
	st.buffer_length = 0.25
	player = AudioStreamPlayer.new()
	player.stream = st
	player.volume_db = -6.0
	add_child(player)
	player.play()
	gen = player.get_stream_playback()

func stinger(strength: float) -> void:
	_sting = maxf(_sting, clampf(strength, 0.0, 1.0))

func hum(seconds: float) -> void:
	_hum = maxf(_hum, seconds)

func _process(delta: float) -> void:
	if gen == null:
		return
	_hum = maxf(0.0, _hum - delta)
	var s := Game.suspicion
	var night := 1.0 - GameClock.daylight()
	var music := float(Game.settings.get("music", 0.7))
	var vol := float(Game.settings.get("master", 0.8))
	var frames := gen.get_frames_available()
	# accord de la nappe : do majeur 7 le jour, il se désaccorde quand la suspicion monte
	var detune := 1.0 + s * 0.018
	var f := [130.81, 164.81 * detune, 196.0, 246.94 / detune]
	for i in frames:
		_t += 1.0 / RATE
		var pad := 0.0
		for k in 4:
			_ph[k] = fmod(_ph[k] + f[k] / RATE, 1.0)
			pad += sin(_ph[k] * TAU)
		pad *= 0.25 * (0.55 + 0.45 * sin(_t * 0.35)) * (1.0 - s) * (1.0 - night * 0.5)
		# drone : deux basses proches qui battent (malaise physique)
		_ph[4] = fmod(_ph[4] + 55.0 / RATE, 1.0)
		_ph[5] = fmod(_ph[5] + 57.3 / RATE, 1.0)
		var drone := (sin(_ph[4] * TAU) + 0.6 * (_ph[5] * 2.0 - 1.0)) * smoothstep(0.15, 0.9, s) * 0.5
		# acouphène
		_ph[6] = fmod(_ph[6] + 4186.0 / RATE, 1.0)
		var tin := sin(_ph[6] * TAU) * smoothstep(0.6, 1.0, s) * 0.03
		# bourdonnement électrique (50 Hz + harmoniques)
		_ph[7] = fmod(_ph[7] + 50.0 / RATE, 1.0)
		var buzz := 0.0
		if _hum > 0.0:
			buzz = (sin(_ph[7] * TAU) + 0.5 * sin(_ph[7] * TAU * 3.0) + 0.3 * sign(sin(_ph[7] * TAU * 2.0))) * 0.18
		# stinger : bruit filtré + cluster dissonant qui retombe
		var sting := 0.0
		if _sting > 0.001:
			var n := randf() * 2.0 - 1.0
			_lp += (n - _lp) * 0.08
			sting = (_lp * 1.6 + sin(_t * TAU * 311.0) * 0.4 + sin(_t * TAU * 329.6) * 0.4) * _sting * 0.5
			_sting *= 0.99985
		var v := (pad * 0.16 * music + drone * 0.22 * music + tin + buzz + sting) * vol
		gen.push_frame(Vector2(v, v))
