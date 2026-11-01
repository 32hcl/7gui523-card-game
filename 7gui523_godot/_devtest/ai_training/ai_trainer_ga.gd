extends SceneTree

# ═══════════════════════════════════════════════════════════════
# AITrainerGA — 遗传算法权重训练器
#
# 用法:
#   godot --headless --path . --script _devtest/ai_training/ai_trainer_ga.gd
#
# 目标: 训练 AI3(权重驱动) 击败 AI1(规则驱动, 固定基线)
#
# 算法: 遗传算法 (Genetic Algorithm)
#   1. 从 default_weights() 扰动生成种群 (POP_SIZE 个个体)
#   2. 每代：每个个体 vs AI1(规则) 打 EVAL_GAMES 局 → 胜率 = 适应度
#   3. 锦标赛选择 → 算术交叉 → 变异 → 精英保留 → 下一代
#   4. 每 CHECKPOINT_GEN 代存盘
#
# AI1 是固定规则对手(规则基线, 不做共进化)。
# 训练产出的权重 -> AI3_TRAINED; 生产关卡 BOSS 使用 AI2_GREEDY。
# ═══════════════════════════════════════════════════════════════

const BattleSim := preload("res://_devtest/ai_training/ai_battle.gd")
const Metrics   := preload("res://_devtest/ai_training/ai_metrics.gd")

# ——— 遗传算法参数 ———
const POP_SIZE             := 12      ## 种群大小
const ELITE_COUNT          := 2       ## 精英保留数
const TOURNAMENT_SIZE      := 3       ## 锦标赛选择候选数
const CROSSOVER_PROB       := 0.7     ## 交叉概率（否则直接克隆）
const EVAL_GAMES           := 40      ## 每个个体评估对局数
const EVAL_CHAMPION        := 200     ## 冠军加测对局数
const MAX_GENERATIONS      := 50      ## 最大代数
const CHECKPOINT_GEN       := 10      ## 每 N 代存盘

# ——— 路径 ———
const SAVE_PATH     := "res://data/ai/ai2_weights.tres"
const SAVE_PATH_DEV := "res://_devtest/ai_training/ai2_weights.tres"

# ——— 变异参数 ———
const MUTATE_PARAM_COUNT_MIN := 1
const MUTATE_PARAM_COUNT_MAX := 3
const MUTATE_FACTOR_MIN      := 0.8
const MUTATE_FACTOR_MAX      := 1.2

# ——— AI 等级常量 ———
## 基线: AI1 规则驱动, 固定规则基线
const L_BASELINE   := AICore.Level.AI1_RULE
## 候选: AI3 权重驱动, 训练目标 → 输出到 ai2_weights.tres 供关卡 BOSS(AI2) 使用
const L_CANDIDATE  := AICore.Level.AI3_TRAINED

# ——— 运行时状态 ———
var _population: Array[AIWeights] = []
var _fitness: Array[float] = []          ## population[i] 的适应度 (vs AI1 胜率)
var _champion: AIWeights
var _champion_fitness := 0.0
var _champion_damage  := 0.0
var _champion_gen     := 0               ## 冠军诞生代数
var _generation       := 0
var _total_games      := 0
var _diversity_history: Array[float] = []


# ═══════════════════════════════════════════════════════════════
# 入口
# ═══════════════════════════════════════════════════════════════

func _init() -> void:
	_print_banner()
	AICore.weights = AIWeights.default_weights()
	_init_population()
	_evaluate_population()

	print("  gen=%d  冠军 vs AI1 胜率=%.2f%%  均伤=%.1f  种群均值=%.2f%%  多样性=%.3f" % [
		_generation,
		_champion_fitness * 100,
		_champion_damage,
		_average_fitness() * 100,
		_diversity()
	])
	print("")

	var start_time := Time.get_ticks_msec()

	while _generation < MAX_GENERATIONS:
		_generation += 1

		var next_gen := _select_and_breed()
		_population = next_gen
		_evaluate_population()

		var avg_fit := _average_fitness() * 100
		var div := _diversity()
		_diversity_history.append(div)
		print("  gen=%d  冠军=%.2f%%  均伤=%.1f  种群均值=%.2f%%  多样性=%.3f" % [
			_generation,
			_champion_fitness * 100,
			_champion_damage,
			avg_fit,
			div
		])

		if _generation % CHECKPOINT_GEN == 0:
			_save_weights()

		if _generation - _champion_gen >= 15 and div < 0.01:
			print("  早停: 种群已收敛 (连续 %d 代无进步, 多样性 %.3f)" %
				[_generation - _champion_gen, div])
			break

	var elapsed := (Time.get_ticks_msec() - start_time) / 1000.0
	print("")
	print("=".repeat(60))
	print("  GA 训练完成  %d 代  |  %d 局  |  耗时 %.1f 秒" %
		[_generation, _total_games, elapsed])
	print("  冠军 vs AI1 胜率: %.2f%%  均伤: %.1f  (产生于第 %d 代)" %
		[_champion_fitness * 100, _champion_damage, _champion_gen])
	_save_weights()
	print("  权重已保存至: %s" % SAVE_PATH)
	print("=".repeat(60))
	quit(0)


# ═══════════════════════════════════════════════════════════════
# 初始化
# ═══════════════════════════════════════════════════════════════

func _print_banner() -> void:
	print("=".repeat(60))
	print("  AI Trainer GA — 训练 AI2 权重 vs AI1(规则Boss)")
	print("  种群 %d  |  每代 %d 局/个体  |  最大 %d 代" %
		[POP_SIZE, EVAL_GAMES, MAX_GENERATIONS])
	print("  选择: 锦标赛(k=%d)  |  交叉: 算术混合  |  精英: %d" %
		[TOURNAMENT_SIZE, ELITE_COUNT])
	print("  参数数: %d  |  对手: AI1(固定规则, 同关卡Boss)" % AIWeights.parameter_names().size())
	print("=".repeat(60))


func _init_population() -> void:
	_population.clear()
	var seed := _perturb_copy(AIWeights.default_weights())
	_population.append(seed)
	for _i in range(1, POP_SIZE):
		var w := _perturb_copy(AIWeights.default_weights())
		_population.append(w)
	print("  种群已初始化: %d 个体" % _population.size())


# ═══════════════════════════════════════════════════════════════
# 适应度评估
# ═══════════════════════════════════════════════════════════════

func _evaluate_population() -> void:
	_fitness.clear()
	_fitness.resize(_population.size())
	for i in _population.size():
		AICore.weights = _population[i]
		var metrics := _evaluate_one(_population[i])

		_fitness[i] = metrics.win_rate()
		if metrics.win_rate() > _champion_fitness or \
			(is_equal_approx(metrics.win_rate(), _champion_fitness) and \
			 metrics.average_damage() > _champion_damage):
			_champion = _population[i].duplicate_weights()
			_champion_fitness = metrics.win_rate()
			_champion_damage = metrics.average_damage()
			_champion_gen = _generation


func _evaluate_one(weights: AIWeights) -> AITrainingMetrics:
	var metrics := Metrics.new()
	var ai_cand := _make_player(L_CANDIDATE, "AI2")
	var ai_base := _make_player(L_BASELINE, "AI1")

	for game_i in EVAL_GAMES:
		var res: AIBattleSim.BattleResult
		if game_i % 2 == 0:
			res = BattleSim.simulate(ai_cand, ai_base)
		else:
			res = BattleSim.simulate(ai_base, ai_cand)
			if res != null:
				var tmp_w: Player.PlayerData = res.winner
				var tmp_l: Player.PlayerData = res.loser
				var tmp_wl: int = res.winner_ai_level
				var tmp_ll: int = res.loser_ai_level
				res.winner = tmp_l
				res.loser = tmp_w
				res.winner_ai_level = tmp_ll
				res.loser_ai_level = tmp_wl

		if res == null:
			continue

		metrics.games += 1
		_total_games += 1
		if res.winner_ai_level == L_CANDIDATE:
			metrics.wins += 1
			metrics.total_damage += float(res.winner_damage)
			metrics.total_remaining_hand += float(res.winner_remaining)
		else:
			metrics.total_damage += float(res.loser_damage)
			metrics.total_remaining_hand += float(res.loser_remaining)

	return metrics


# ═══════════════════════════════════════════════════════════════
# 锦标赛选择
# ═══════════════════════════════════════════════════════════════

func _tournament_select() -> AIWeights:
	var best_idx := -1
	var best_fit := -INF
	for _k in TOURNAMENT_SIZE:
		var idx := randi() % _population.size()
		if _fitness[idx] > best_fit:
			best_fit = _fitness[idx]
			best_idx = idx
	return _population[best_idx]


# ═══════════════════════════════════════════════════════════════
# 算术交叉
# ═══════════════════════════════════════════════════════════════

func _crossover(parent_a: AIWeights, parent_b: AIWeights) -> AIWeights:
	var child := AIWeights.new()
	for pname in AIWeights.parameter_names():
		var alpha: float = randf()
		var val: float = alpha * (parent_a.get(pname) as float) + \
			(1.0 - alpha) * (parent_b.get(pname) as float)
		child.set(pname, val)
	return child


# ═══════════════════════════════════════════════════════════════
# 变异
# ═══════════════════════════════════════════════════════════════

func _mutate(w: AIWeights) -> void:
	var n := randi() % (MUTATE_PARAM_COUNT_MAX - MUTATE_PARAM_COUNT_MIN + 1) \
		+ MUTATE_PARAM_COUNT_MIN
	var shuffled := AIWeights.parameter_names()
	shuffled.shuffle()
	for i: int in n:
		if i >= shuffled.size():
			break
		var pname: String = shuffled[i]
		var old_val: float = w.get(pname) as float
		var factor: float = MUTATE_FACTOR_MIN + randf() * \
			(MUTATE_FACTOR_MAX - MUTATE_FACTOR_MIN)
		var new_val: float = old_val * factor
		if is_zero_approx(old_val):
			new_val = (randf() - 0.5) * 2.0
		w.set(pname, new_val)


func _mutate_all(w: AIWeights) -> void:
	var params: Array[String] = AIWeights.parameter_names()
	var shuffled: Array[String] = []
	shuffled.assign(params)
	shuffled.shuffle()
	var n: int = clamp(params.size() / 2, 8, params.size())
	for i in n:
		var pname: String = shuffled[i]
		var old_val: float = w.get(pname) as float
		var factor: float = 0.75 + randf() * 0.5
		var new_val: float = old_val * factor
		if is_zero_approx(old_val):
			new_val = (randf() - 0.5) * 2.0
		w.set(pname, new_val)


func _perturb_copy(src: AIWeights) -> AIWeights:
	var w := src.duplicate_weights()
	_mutate_all(w)
	return w


# ═══════════════════════════════════════════════════════════════
# 选择 + 交叉 + 变异 → 下一代
# ═══════════════════════════════════════════════════════════════

func _select_and_breed() -> Array[AIWeights]:
	var ranked := _ranked_indices()
	var next_gen: Array[AIWeights] = []
	for i in ELITE_COUNT:
		if i < ranked.size():
			next_gen.append(_population[ranked[i]].duplicate_weights())

	while next_gen.size() < POP_SIZE:
		var parent_a := _tournament_select()
		var child: AIWeights
		if randf() < CROSSOVER_PROB and _population.size() >= 2:
			var parent_b := _tournament_select()
			child = _crossover(parent_a, parent_b)
		else:
			child = parent_a.duplicate_weights()
		var is_elite := false
		for e in next_gen:
			if e == child:
				is_elite = true
				break
		if not is_elite:
			_mutate(child)
		next_gen.append(child)

	return next_gen


func _ranked_indices() -> Array:
	var indices: Array[int] = []
	for i in _population.size():
		indices.append(i)
	indices.sort_custom(func(a: int, b: int): return _fitness[a] > _fitness[b])
	return indices


# ═══════════════════════════════════════════════════════════════
# 冠军精测 — 存盘时用更多对局验证
# ═══════════════════════════════════════════════════════════════

func _evaluate_champion_thorough() -> AITrainingMetrics:
	var metrics := Metrics.new()
	var ai_cand := _make_player(L_CANDIDATE, "AI2")
	var ai_base := _make_player(L_BASELINE, "AI1")
	for game_i in EVAL_CHAMPION:
		AICore.weights = _champion
		var res: AIBattleSim.BattleResult
		if game_i % 2 == 0:
			res = BattleSim.simulate(ai_cand, ai_base)
		else:
			res = BattleSim.simulate(ai_base, ai_cand)
			if res != null:
				var tmp_w: Player.PlayerData = res.winner
				var tmp_l: Player.PlayerData = res.loser
				var tmp_wl: int = res.winner_ai_level
				var tmp_ll: int = res.loser_ai_level
				res.winner = tmp_l
				res.loser = tmp_w
				res.winner_ai_level = tmp_ll
				res.loser_ai_level = tmp_wl
		if res == null:
			continue
		metrics.games += 1
		if res.winner_ai_level == L_CANDIDATE:
			metrics.wins += 1
			metrics.total_damage += float(res.winner_damage)
		else:
			metrics.total_damage += float(res.loser_damage)
	return metrics


# ═══════════════════════════════════════════════════════════════
# 种群多样性
# ═══════════════════════════════════════════════════════════════

func _diversity() -> float:
	if _population.size() < 2:
		return 0.0
	var total := 0.0
	var count := 0
	for i in _population.size():
		for j in range(i + 1, _population.size()):
			total += _weight_distance(_population[i], _population[j])
			count += 1
	return total / max(count, 1)


func _weight_distance(a: AIWeights, b: AIWeights) -> float:
	var sq_sum := 0.0
	var normalizer := 0.0
	for pname in AIWeights.parameter_names():
		var va: float = a.get(pname) as float
		var vb: float = b.get(pname) as float
		var diff := va - vb
		sq_sum += diff * diff
		normalizer += max(abs(va), abs(vb)) * max(abs(va), abs(vb))
	if normalizer < 0.001:
		return 0.0
	return sqrt(sq_sum / max(normalizer, 0.001))


func _average_fitness() -> float:
	if _fitness.is_empty():
		return 0.0
	var s := 0.0
	for f in _fitness:
		s += f
	return s / float(_fitness.size())


# ═══════════════════════════════════════════════════════════════
# 工具
# ═══════════════════════════════════════════════════════════════

func _make_player(level: int, name: String) -> Player.PlayerData:
	var p := Player.PlayerData.new()
	p.name = name
	p.ai_level = level
	return p


# ═══════════════════════════════════════════════════════════════
# 存盘
# ═══════════════════════════════════════════════════════════════

func _save_weights() -> void:
	if _champion == null:
		return
	var metrics := _evaluate_champion_thorough()
	var err := ResourceSaver.save(_champion, SAVE_PATH)
	if err == OK:
		ResourceSaver.save(_champion, SAVE_PATH_DEV)
		print("  💾 已保存 %s  (冠军 %d 局精测: 胜率 %.2f%%)" %
			[SAVE_PATH, metrics.games, metrics.win_rate() * 100])
	else:
		printerr("  ✗ 保存失败: %d" % err)