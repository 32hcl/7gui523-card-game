class_name LevelEntrance
extends RefCounted

# ═══════════════════════════════════════════════════════════════
# LevelEntrance — 主菜单入口、门票、训练模式、重置/清理
#
# 由 play.gd 门面创建，负责：
#   enter_casino_then_start — 门票购买弹窗后进入关卡
#   start_training          — 训练模式入口（绕过门票）
#   reset_pressed           — 重开
#   back_to_menu            — 回主菜单
#   clear_battle_runtime    — 关卡间清理
# ═══════════════════════════════════════════════════════════════


func start_training(base: Control, ai_level: int, restrictions: Dictionary, show_ai_hand: bool) -> void:
	base._training.start(ai_level, show_ai_hand, restrictions)
	var state: GameState = base._state
	state.campaign.current_level = 1
	state.campaign.has_purchased_entry = true
	state.campaign.player_chips = GameConfig.INITIAL_CHIPS
	state.campaign.boss_chips = GameConfig.BOSS_INITIAL_CHIPS
	base._round_binding.start_level(base, 1)


func reset_pressed(base: Control) -> void:
	clear_battle_runtime(base)

	if base._training.is_active:
		base._state.campaign.start_new()
		base._state.shop.reset_stock()
		base._game_over_layer.visible = false
		base._training.start(base._training.ai_override_level, base._training.show_ai_hand, base._training.restrictions)
		base._state.campaign.current_level = 1
		base._state.campaign.has_purchased_entry = true
		base._state.campaign.player_chips = GameConfig.INITIAL_CHIPS
		base._state.campaign.boss_chips = GameConfig.BOSS_INITIAL_CHIPS
		base._round_binding.start_level(base, 1)
		return

	base._state.campaign.start_new()
	base._state.shop.reset_stock()
	enter_casino_then_start(base, base._state.campaign.current_level)


func back_to_menu(base: Control) -> void:
	clear_battle_runtime(base)
	base._state.campaign.start_new()
	base._state.shop.reset_stock()
	base._main_menu.dismiss()
	base._main_menu.show()


func clear_battle_runtime(base: Control) -> void:
	var state: GameState = base._state
	state.flow_generation += 1
	state.phase = "battle"
	state.finished = true
	state.ai_busy = false
	if base._shop_panel:
		base._shop_panel.visible = false
	base._round_binding.teardown_round(base)
	base._table_anim_running = false
	base._table_anim_pending = false
	base._table_cards_anim.clear()
	base._table_render_sig = "@init"
	base._play_src_global = Vector2.ZERO
	state.selected.clear()
	state.player_discard_count = 0
	state.player_discarded_this_round = false
	state.player_can_discard = false
	for row: HBoxContainer in [base._top_hand, base._bottom_hand, base._boss_hand, base._boss_hand_instant]:
		base._clear_children(row)
	base._clear_children(base._table_cards)
	base._clear_children(base._anim_overlay)
	state.human_deck = null
	state.ai_deck = null
	state.instant_deck = null


func enter_casino_then_start(base: Control, level: int) -> void:
	var state: GameState = base._state
	if state.campaign.has_purchased_entry:
		base._round_binding.start_level(base, level)
		return

	if state.campaign.dollar < GameConfig.ENTRY_TICKET_DOLLARS:
		base._show_game_over(
			"金币不足",
			"进入赌场需要 %d 金币\n当前金币：%d" % [GameConfig.ENTRY_TICKET_DOLLARS, state.campaign.dollar])
		return

	var confirm_cb := func():
		if state.campaign.purchase_entry_ticket():
			base._round_binding.start_level(base, level)
	var back_cb := func(): back_to_menu(base)
	base._dialogs.show_entry_ticket(base, state.campaign.dollar, confirm_cb, back_cb)