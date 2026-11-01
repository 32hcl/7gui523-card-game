class_name TrainingSession
extends RefCounted

var is_active: bool = false
var show_ai_hand: bool = false
var restrictions: Dictionary = {}
var ai_override_level: int = -1

func start(ai_level: int, _show_ai_hand: bool, _restrictions: Dictionary) -> void:
	is_active = true
	ai_override_level = ai_level
	show_ai_hand = _show_ai_hand
	restrictions = _restrictions

func ai_display_name() -> String:
	var ai_names: Array[String] = ["规则引擎", "GA训练", "默认权重", "备选权重"]
	var idx: int = clampi(ai_override_level, 0, 3)
	return "训练 · " + ai_names[idx]

func build_rule() -> Campaign.LevelRule:
	var rule := Campaign.LevelRule.new()
	for key in restrictions:
		if restrictions[key]:
			match key:
				"mask_player_draws":      rule.mask_player_draws = true
				"boss_always_first":      rule.boss_always_first = true
				"boss_bite_damage":       rule.boss_bite_damage = true
				"ghost_cheat":            rule.ghost_cheat = true
				"blind_mix":              rule.blind_mix = true
				"boss_steal_max":         rule.boss_steal_max = true
				"boss_swap_low_cards":    rule.boss_swap_low_cards = true
				"ban_gui523":             rule.ban_gui523 = true
	return rule