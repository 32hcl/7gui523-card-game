extends SceneTree

const BattleSim := preload("res://_devtest/ai_training/ai_battle.gd")
const GAMES := 500

func _init() -> void:
	print("═══════════════════════════════════════════════")
	print("  AI3(训练权重) vs AI2(硬编码基线)  —  %d 局" % GAMES)
	print("═══════════════════════════════════════════════")

	var weights: AIWeights = null
	if ResourceLoader.exists("res://_devtest/ai_training/best_weights.tres"):
		weights = load("res://_devtest/ai_training/best_weights.tres") as AIWeights
		AICore.weights = weights
		print("[OK] 已加载 best_weights.tres")
	else:
		AICore.weights = AIWeights.default_weights()
		print("[WARN] 未找到训练权重，使用 default_weights")

	var ai3_wins := 0

	for i in range(GAMES):
		# 交替先手避免顺序偏差
		var swap := i % 2 == 0

		var p1 := Player.PlayerData.new()
		p1.name = "AI3" if swap else "AI2"
		p1.ai_level = AICore.Level.AI3_TRAINED if swap else AICore.Level.AI2_GREEDY

		var p2 := Player.PlayerData.new()
		p2.name = "AI2" if swap else "AI3"
		p2.ai_level = AICore.Level.AI2_GREEDY if swap else AICore.Level.AI3_TRAINED

		var result = BattleSim.simulate(p1, p2)
		if result == null:
			continue

		var ai3_won := false
		if swap:
			ai3_won = result.winner_ai_level == AICore.Level.AI3_TRAINED
		else:
			ai3_won = result.winner_ai_level == AICore.Level.AI3_TRAINED

		if ai3_won:
			ai3_wins += 1

		if (i + 1) % 50 == 0:
			var rate := float(ai3_wins) / float(i + 1) * 100.0
			print("[%4d/%d]  AI3 胜 %d  胜率 %.1f%%" % [i + 1, GAMES, ai3_wins, rate])

	var final_rate := float(ai3_wins) / float(GAMES) * 100.0
	print("═══════════════════════════════════════════════")
	print("  AI3 胜: %d / %d  胜率: %.1f%%" % [ai3_wins, GAMES, final_rate])
	print("═══════════════════════════════════════════════")
	quit(0)