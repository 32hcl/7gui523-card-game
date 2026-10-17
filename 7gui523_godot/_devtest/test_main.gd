extends SceneTree

# ═══════════════════════════════════════════════════════════════
# TestMain — 纯逻辑测试汇总入口（对应 C++ tests/test_main.cpp）
# 一次性跑：test_cardtype.gd（牌型规则）+ test_score.gd（计分）
#          + test_deck_player.gd（牌堆/玩家存储）+ test_ai_round.gd（AI/回合链路）
#
# 命令行：
#   godot --headless --path . --script _devtest/test_main.gd
# ═══════════════════════════════════════════════════════════════

const U := preload("res://_devtest/test_util.gd")

# 依赖被隔离的测试脚本，分别实例化调用其静态 _run()
const TestCardType := preload("res://_devtest/test_cardtype.gd")
const TestScore := preload("res://_devtest/test_score.gd")
const TestDeckPlayer := preload("res://_devtest/test_deck_player.gd")
const TestAIRound := preload("res://_devtest/test_ai_round.gd")
const TestCampaign := preload("res://_devtest/test_campaign.gd")


func _init() -> void:
	var failed := _run()
	quit(1 if failed > 0 else 0)


static func _run() -> int:
	print("=".repeat(60))
	print("  纯逻辑测试汇总 — test_main.gd")
	print("=".repeat(60))

	print("──── 调用 test_cardtype.gd ────")
	var failed := TestCardType._run()
	print("──── 调用 test_score.gd ────")
	failed += TestScore._run()
	print("──── 调用 test_deck_player.gd ────")
	failed += TestDeckPlayer._run()
	print("──── 调用 test_ai_round.gd ────")
	failed += TestAIRound._run()
	print("──── 调用 test_campaign.gd ────")
	failed += TestCampaign._run()

	print("")
	print("汇总 Result: %s failed" % failed)
	print("=".repeat(60))
	return failed
