class_name AITrainingMetrics
extends RefCounted

## 训练指标 — 比较两套权重的优劣
## 训练目标按字典序比较：
##   1. 胜率最高
##   2. 平均造成伤害最高
##   3. 平均剩余手牌最少

var wins := 0
var games := 0
var total_damage := 0.0
var total_remaining_hand := 0.0


func win_rate() -> float:
	return float(wins) / max(games, 1)


func average_damage() -> float:
	return total_damage / max(games, 1)


func average_remaining_hand() -> float:
	return total_remaining_hand / max(games, 1)


func dominates(other: AITrainingMetrics) -> bool:
	if not is_equal_approx(win_rate(), other.win_rate()):
		return win_rate() > other.win_rate()
	if not is_equal_approx(average_damage(), other.average_damage()):
		return average_damage() > other.average_damage()
	return average_remaining_hand() < other.average_remaining_hand()


func to_str() -> String:
	return "胜率 %.1f%%  均伤 %.1f  剩牌 %.1f  (N=%d)" % [
		win_rate() * 100, average_damage(), average_remaining_hand(), games
	]


func merge(other: AITrainingMetrics) -> void:
	wins += other.wins
	games += other.games
	total_damage += other.total_damage
	total_remaining_hand += other.total_remaining_hand