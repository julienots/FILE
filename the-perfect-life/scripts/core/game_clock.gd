extends Node
## Horloge du monde (autoload « GameClock »). 1 seconde réelle = `speed` minutes de jeu.

signal minute_changed(day: int, minute: int)
signal hour_changed(hour: int)
signal day_changed(day: int)

const DAY_NAMES := ["Lundi", "Mardi", "Mercredi", "Jeudi", "Vendredi", "Samedi", "Dimanche"]
const SUNRISE := 6.5 * 60.0
const SUNSET := 20.5 * 60.0

var day := 0
var minute := 7 * 60
var speed := 1.0
var paused := false
var _acc := 0.0

func _process(delta: float) -> void:
	if paused:
		return
	_acc += delta * speed
	while _acc >= 1.0:
		_acc -= 1.0
		advance(1)

func advance(minutes: int) -> void:
	for i in minutes:
		var old_hour := hour()
		minute += 1
		if minute >= 1440:
			minute -= 1440
			day += 1
			day_changed.emit(day)
		if hour() != old_hour:
			hour_changed.emit(hour())
	minute_changed.emit(day, minute)

## Avance jusqu'à une heure donnée (ex. dormir jusqu'à 7 h).
func skip_to(target_minute: int) -> void:
	var delta_m := (target_minute - minute + 1440) % 1440
	advance(delta_m)

func hour() -> int:
	return minute / 60

func time_text() -> String:
	return "%02d:%02d" % [minute / 60, minute % 60]

func day_name() -> String:
	return DAY_NAMES[day % 7]

func is_night() -> bool:
	return minute < SUNRISE - 20 or minute > SUNSET + 20

## 0 = nuit, 1 = plein jour (transition douce à l'aube et au crépuscule).
func daylight() -> float:
	var m := float(minute)
	return clampf(minf((m - (SUNRISE - 40.0)) / 80.0, ((SUNSET + 40.0) - m) / 80.0), 0.0, 1.0)

## Élévation (degrés) et azimut du soleil.
func sun_angles() -> Vector2:
	var t := (float(minute) - SUNRISE) / (SUNSET - SUNRISE)
	var elevation := sin(clampf(t, -0.2, 1.2) * PI) * 58.0
	var azimuth := lerpf(-110.0, 110.0, t)
	return Vector2(elevation, azimuth)

func to_dict() -> Dictionary:
	return {day = day, minute = minute}

func from_dict(d: Dictionary) -> void:
	day = int(d.get("day", 0))
	minute = int(d.get("minute", 420))
	minute_changed.emit(day, minute)
