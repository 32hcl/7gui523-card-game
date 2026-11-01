# AI 训练隔离区

这里存放自博弈训练器、对局评估器和参数搜索代码。

正式运行时只加载 `scripts/ai/ai_weights.gd` 定义的训练参数；训练脚本不会被正式场景引用。

## 文件

| 文件 | 说明 |
|---|---|
| `ai_metrics.gd` | `AITrainingMetrics` — 训练指标（胜率/均伤/剩牌） |
| `ai_battle.gd` | `AIBattleSim` — 无头对局模拟器（AI vs AI 完整一局，4 套牌堆） |
| `ai_trainer.gd` | `AITrainer` — 爬山法权重训练（AI4） |
| `ai_trainer_ga.gd` | `AITrainerGA` — 遗传算法权重训练（产出 ai2_weights.tres → AI2_GREEDY） |
| `ai_eval.gd` | 评估脚本：AI3 vs AI2 多局对抗 |
| `best_weights.tres` | 训练产出的最优权重（`.gitignore`） |

## 训练架构

```
AITrainerGA (SceneTree, headless) — 遗传算法
  │
  ├─ AIBattleSim.simulate(AI3_TRAINED, AI1_RULE) × 40 局/个体
  │     ├─ Round.new(a, b, deck_a, deck_b, instant_a, instant_b)
  │     ├─ AICore.choose() → AI3_TRAINED / AI1_RULE
  │     └─ CardTracker 记牌
  │
  ├─ 种群 12 个体 × 40 局 = 每代 480 局
  ├─ 锦标赛选择 → 算术交叉 → 变异
  ├─ 冠军加测 200 局 → 对抗 AI1 规则基线
  └─ 每 10 代存盘 ai2_weights.tres → 供关卡 BOSS(AI2_GREEDY) 加载

AITrainer (SceneTree, headless) — 爬山法
  │
  ├─ AIBattleSim.simulate(AI4_TRAINED, AI3_TRAINED) × 300 局/轮
  │     ├─ Round.new(a, b, deck_a, deck_b, instant_a, instant_b)
  │     ├─ AICore.choose() → AI4_TRAINED / AI3_TRAINED
  │     └─ CardTracker 记牌
  │
  ├─ 随机扰动 1~3 个参数 × 0.8~1.2
  ├─ 爬山法：胜率提升 → 采纳，否则回退
  └─ 每 20 轮存盘 ai4_weights.tres
```

## 使用方法

```bash
# GA 遗传算法训练（产出关卡 BOSS 权重 ai2_weights.tres）
godot --headless --path . --script _devtest/ai_training/ai_trainer_ga.gd

# 爬山法训练（产出 AI4 权重 ai4_weights.tres）
godot --headless --path . --script _devtest/ai_training/ai_trainer.gd

# 评估：AI3 训练权重 vs AI2(关卡BOSS)
godot --headless --path . --script _devtest/ai_training/ai_eval.gd

# 调整 GA 参数：修改 ai_trainer_ga.gd 中的常量
#   POP_SIZE          = 12    种群大小
#   EVAL_GAMES        = 40    每个体评估局数
#   EVAL_CHAMPION     = 200   冠军加测局数
#   MAX_GENERATIONS   = 50    最大代数
```

## 权重载入

训练产出的 `best_weights.tres` 可在运行时加载：

```gdscript
var w := load("res://_devtest/ai_training/best_weights.tres") as AIWeights
AICore.weights = w
_ai.ai_level = AICore.Level.AI3_TRAINED
```

## 训练目标

按字典序比较：

1. 胜率最高；
2. 平均造成伤害最高；
3. 平均剩余手牌最少。