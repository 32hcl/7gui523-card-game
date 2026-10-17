extends Object

const U := preload("res://_devtest/test_util.gd")


static func _run() -> int:
	var failed := 0
	print("============================================================")
	print("  Campaign 门票 / 最终挑战测试 — test_campaign.gd")
	print("============================================================")

	failed += U.section("金币门票与筹码独立", func():
		var campaign := Campaign.new()
		campaign.player_chips = 50
		var before_dollars := campaign.dollar
		U.ok(not campaign.has_purchased_entry, "初始没有入园资格")
		U.ok(campaign.purchase_entry_ticket(), "金币足够时可以购买门票")
		U.eq(campaign.dollar, before_dollars - Campaign.ENTRY_TICKET_DOLLARS, "购买门票只扣金币")
		U.eq(campaign.player_chips, 50, "购买门票不扣筹码")
		U.ok(campaign.has_purchased_entry, "购买后获得入园资格")
	)

	failed += U.section("第9关不可通关", func():
		var campaign := Campaign.new()
		campaign.start_level(9)
		var result := campaign.finish_level(true, 0, 0)
		U.ok(not result.player_won, "第9关不接受玩家胜利")
		U.ok(not result.boss_cleared, "第9关不标记Boss被击败")
		U.ok(not campaign.is_victory(), "最终挑战不产生全部通关状态")
	)

	failed += U.section("新战役重置门票资格", func():
		var campaign := Campaign.new()
		campaign.purchase_entry_ticket()
		campaign.player_chips = 3
		campaign.start_new()
		U.eq(campaign.player_chips, 50, "新战役恢复初始筹码")
		U.eq(campaign.dollar, Campaign.INITIAL_DOLLARS, "新战役恢复初始金币")
		U.ok(not campaign.has_purchased_entry, "新战役必须重新购买门票")
	)

	return U.finish(failed, "Campaign")
