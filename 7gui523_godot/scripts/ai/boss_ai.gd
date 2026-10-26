class_name BossAI
extends RefCounted

## 正式关卡使用的 Boss 决策入口。
## AI2/AI3 和训练权重不进入正式游戏流程。

static func choose(player: Player.PlayerData, previous: CardTypeResult,
		table_score: int = 0) -> Array[Card]:
	return AICore.choose_ai1(player, previous, null, null, table_score, null)
