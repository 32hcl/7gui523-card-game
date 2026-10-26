class_name AIWeights
extends Resource

## 运行时只读取训练结果；策略权重不在 AI 本体中手写。
@export var finish_hand_reward: float
@export var finish_round_reward: float
@export var clear_table_reward: float
@export var reduce_hand_size_reward: float
@export var play_card_count_cost: float
@export var rank_cost: float
@export var high_card_cost: float
@export var low_card_release_reward: float
@export var break_pair_cost: float
@export var break_triple_cost: float
@export var break_bomb_cost: float
@export var break_five_kind_cost: float
@export var break_rocket_cost: float
@export var preserve_same_point_reward: float
@export var preserve_bomb_reward: float
@export var preserve_rocket_reward: float
@export var preserve_gui523_reward: float
@export var beat_same_type_reward: float
@export var beat_bomb_reward: float
@export var beat_rocket_reward: float
@export var opponent_low_hand_reward: float
@export var opponent_empty_hand_penalty: float
@export var counter_risk_cost: float
@export var table_score_reward: float
@export var score_card_reward: float
@export var five_card_score_reward: float
@export var damage_per_point_reward: float
@export var round_damage_reward: float
@export var large_damage_reward: float
@export var exhausted_point_reward: float
@export var rare_point_reward: float
@export var known_opponent_card_reward: float
@export var unknown_card_risk_cost: float
@export var remaining_deck_risk_cost: float

static func parameter_names() -> Array[String]:
	var result: Array[String] = []
	for property in AIWeights.new().get_property_list():
		if property.get("usage", 0) & PROPERTY_USAGE_SCRIPT_VARIABLE:
			result.append(property.name)
	return result


## 数量级初值：全部取 1 / 10 / 100 / 1000 / 10000 / 100000，
## 语义只定"哪一档"，精确数值交给训练器（爬山法）调。
## 档位约定：1e5 出完即胜 > 1e4 拦截濒死对手 > 1e3 组合完整性
## （压牌/拆牌/保炸弹/五同/大伤害/轮终结） > 1e2 次级奖励
## （清桌面/保对子/扫点） > 1e1 逐张逐点系数 > 1e0 细粒度系数 > 1e-1 超细。
static func default_weights() -> AIWeights:
	var w := AIWeights.new()
	w.finish_hand_reward = 100000.0
	w.finish_round_reward = 1000.0
	w.clear_table_reward = 100.0
	w.reduce_hand_size_reward = 0.1
	w.play_card_count_cost = 1.0
	w.rank_cost = 10.0
	w.high_card_cost = 10.0
	w.low_card_release_reward = 1.0
	w.break_pair_cost = 1000.0
	w.break_triple_cost = 1000.0
	w.break_bomb_cost = 1000.0
	w.break_five_kind_cost = 1000.0
	w.break_rocket_cost = 1000.0
	w.preserve_same_point_reward = 100.0
	w.preserve_bomb_reward = 1000.0
	w.preserve_rocket_reward = 1000.0
	w.preserve_gui523_reward = 100.0
	w.beat_same_type_reward = 1000.0
	w.beat_bomb_reward = -1000.0
	w.opponent_low_hand_reward = 10000.0
	w.opponent_empty_hand_penalty = 1000.0
	w.counter_risk_cost = 1.0
	w.table_score_reward = 1.0
	w.score_card_reward = 10.0
	w.five_card_score_reward = 1000.0
	w.damage_per_point_reward = 1.0
	w.round_damage_reward = 1.0
	w.large_damage_reward = 1000.0
	w.exhausted_point_reward = 10.0
	w.rare_point_reward = 1.0
	w.known_opponent_card_reward = 100.0
	w.unknown_card_risk_cost = 1.0
	w.remaining_deck_risk_cost = 0.1
	return w


func duplicate_weights() -> AIWeights:
	return duplicate(true) as AIWeights