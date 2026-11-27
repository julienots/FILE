class_name Interactable
extends Node3D
## Point d'interaction : nom affiché + action courante + rappel. Ajouté au groupe
## « interactable » ; le joueur choisit le plus pertinent et le HUD affiche un bouton.

var display_name := ""
var action_text := ""
var radius := 1.7
var enabled := true
## func(player: Node, it: Interactable) -> void ; peut modifier it.action_text (ex. Ouvrir -> Fermer).
var on_interact: Callable

func _ready() -> void:
	add_to_group("interactable")

func interact(player: Node) -> void:
	if on_interact.is_valid():
		on_interact.call(player, self)

static func attach(parent: Node, pos: Vector3, label: String, action: String, cb: Callable, reach := 1.7) -> Interactable:
	var it := Interactable.new()
	it.display_name = label
	it.action_text = action
	it.on_interact = cb
	it.radius = reach
	it.position = pos
	it.name = "I_" + label.validate_node_name()
	parent.add_child(it)
	return it
