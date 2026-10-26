extends SceneTree

# ═══════════════════════════════════════════════════════════════
# AITrainer — 自博弈权重训练器
#
# 用法:
#   godot --headless --path . --script _devtest/ai_training/ai_trainer.gd
#
# 算法: 随机扰动 + 爬山法 (hill climbing)
#   1. 从 default_weights() 开始
#   2. 每轮随机扰动 1~3 个参数
#   3. AI3(新权重) vs AI2(硬编码基线) 打 EVAL_GAMES 局
#   4. 胜率提升 → 保留；否则回退
#   5. 每 CHECKPOINT_ITERS 轮保存 best_weights
# ═══════════════════════════════════════════════════════════════

const BattleSim := preload("res://_devtest/ai_training/ai_battle.gd")
const Metrics := preload("res://_devtest/ai_training/ai_metrics.gd")

const EVAL_GAMES       := 300     ## 每轮评估对局数（50→300 消除抽样偏差）
const CHECKPOINT_ITERS := 20     ## 每 N 轮存一次盘
const MAX_ITERS         := 100   ## 训练总轮数（300局/轮 总约3万局）
const BASELINE_WEIGHTS_PATH := "res://data/ai/best_weights.tres"  ## AI3 现役权重
const SAVE_PATH         := "res://data/ai/ai4_weights.tres"
const SAVE_PATH_DEV     := "res://_devtest/ai_training/ai4_weights.tres"

var _baseline := AICore.Level.AI3_TRAINED
var _candidate := AICore.Level.AI4_TRAINED
var _best_weights: AIWeights
var _best_winrate  := 0.0
var _iter          := 0


func _init() -> void:
	print("=".repeat(60))
	print("  AI Trainer — 第 4 代权重训练 (AI4 vs AI3)")
	print("  算法: 随机扰动 + 爬山法  |  活跃参数: %d" % AIWeights.parameter_names().size())
	print("  每轮 %d 局  |  总 %d 轮  |  每 %d 轮存盘" % [EVAL_GAMES, MAX_ITERS, CHECKPOINT_ITERS])
	print("=".repeat(60))

	## 候选 AI4：从数量级初值出发
	_best_weights = AIWeights.default_weights()
	AICore.weights4 = _best_weights.duplicate_weights()

	## 基线 AI3：加载现役 best_weights（加载失败退回默认值）
	var base_res := load(BASELINE_WEIGHTS_PATH)
	if base_res is AIWeights:
		AICore.weights = base_res
		print("  基线 AI3 权重已加载: %s" % BASELINE_WEIGHTS_PATH)
	else:
		AICore.weights = AIWeights.default_weights()
		print("  基线 AI3 权重加载失败，使用默认值")

	## 先测起点胜率，作为爬山地平线
	var start_metrics := _evaluate(_best_weights)
	_best_winrate = start_metrics.win_rate()
	_best_damage_val = start_metrics.average_damage()
	print("  AI4 起点对 AI3 胜率: %.2f%%  均伤 %.1f" % [_best_winrate * 100, _best_damage_val])
	print("")

	var start_time := Time.get_ticks_msec()

	while _iter < MAX_ITERS:
		_iter += 1

		# 1. 生成扰动候选
		var candidate := _best_weights.duplicate_weights()
		_perturb(candidate)

		# 2. 测试候选
		var metrics := _evaluate(candidate)
		var wr := metrics.win_rate()

		# 3. 判断是否保留
		var improved: bool = wr > _best_winrate or (is_equal_approx(wr, _best_winrate) and metrics.average_damage() > _best_damage_val)
		if improved:
			var old_wr := _best_winrate
			_best_weights = candidate
			_best_winrate = wr
			_best_damage_val = metrics.average_damage()
			print("[%4d] ✓ 采纳  胜率 %.2f%% (↑%.2f%%)  均伤 %.1f  剩牌 %.1f" %
				[_iter, wr * 100, (wr - old_wr) * 100, metrics.average_damage(), metrics.average_remaining_hand()])
		else:
			print("[%4d] ✗ 放弃  胜率 %.2f%% (≤%.2f%%)  均伤 %.1f" %
				[_iter, wr * 100, _best_winrate * 100, metrics.average_damage()])

		# 4. 存盘
		if _iter % CHECKPOINT_ITERS == 0:
			_save_weights()

	var elapsed := (Time.get_ticks_msec() - start_time) / 1000.0
	print("")
	print("=".repeat(60))
	print("  训练完成  %d 轮  |  耗时 %.1f 秒" % [_iter, elapsed])
	print("  最佳胜率: %.2f%%  均伤: %.1f" % [_best_winrate * 100, _best_damage_val])
	_save_weights()
	print("  权重已保存至: %s" % SAVE_PATH)
	print("=".repeat(60))
	quit(0)


var _best_damage_val := 0.0


# ═══════════════════════════════════════════════════════════════
# 随机扰动：对 1~3 个活跃参数施加 ±factor 的乘法扰动
# 活跃参数 = 全部 @export 字段（AIWeights.parameter_names()）
# ═══════════════════════════════════════════════════════════════

func _perturb(w: AIWeights) -> void:
	var n := randi() % 3 + 1
	var shuffled := AIWeights.parameter_names()
	shuffled.shuffle()
	for i: int in n:
		if i >= shuffled.size():
			break
		var pname: String = shuffled[i]
		var old_val: float = w.get(pname) as float
		var factor: float = 0.8 + randf() * 0.4
		var new_val: float = old_val * factor
		if is_zero_approx(old_val):
			new_val = (randf() - 0.5) * 2.0
		w.set(pname, new_val)


# ═══════════════════════════════════════════════════════════════
# 评估：AI4（候选权重） vs AI3（现役基线） 打 EVAL_GAMES 局
# ═══════════════════════════════════════════════════════════════

func _evaluate(candidate: AIWeights) -> AITrainingMetrics:
	var metrics = Metrics.new()

	# 创建两个 AI 玩家
	var ai_cand := Player.PlayerData.new()
	ai_cand.name = "AI4"
	ai_cand.ai_level = _candidate
	var ai_base := Player.PlayerData.new()
	ai_base.name = "AI3"
	ai_base.ai_level = _baseline

	for game_i: int in EVAL_GAMES:
		AICore.weights4 = candidate

		var res
		if game_i % 2 == 0:
			res = BattleSim.simulate(ai_cand, ai_base)
		else:
			res = BattleSim.simulate(ai_base, ai_cand)
			if res != null:
				var tmp_w = res.winner
				var tmp_l = res.loser
				var tmp_wl: int = res.winner_ai_level
				var tmp_ll: int = res.loser_ai_level
				res.winner = tmp_l
				res.loser = tmp_w
				res.winner_ai_level = tmp_ll
				res.loser_ai_level = tmp_wl

		if res == null:
			continue

		metrics.games += 1
		var ai_cand_is_winner: bool = res.winner_ai_level == _candidate

		if ai_cand_is_winner:
			metrics.wins += 1
			metrics.total_damage += float(res.winner_damage)
			metrics.total_remaining_hand += float(res.winner_remaining)
		else:
			metrics.total_damage += float(res.loser_damage)
			metrics.total_remaining_hand += float(res.loser_remaining)

	return metrics


# ═══════════════════════════════════════════════════════════════
# 存盘
# ═══════════════════════════════════════════════════════════════

func _save_weights() -> void:
	var err := ResourceSaver.save(_best_weights, SAVE_PATH)
	if err == OK:
		print("  💾 已保存 best_weights → data/ai/ (胜率 %.2f%%)" % (_best_winrate * 100))
		ResourceSaver.save(_best_weights, SAVE_PATH_DEV)
	else:
		printerr("  ✗ 保存失败: %d" % err)