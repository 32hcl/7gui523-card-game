# AI 训练隔离区

这里存放自博弈训练器、对局评估器和参数搜索代码。

正式运行时只加载 `scripts/ai/ai_weights.gd` 定义的训练参数；训练脚本不会被正式场景引用。

## 文件

| 文件 | 说明 |
|---|---|
| `ai_metrics.gd` | `AITrainingMetrics` — 训练指标（胜率/均伤/剩牌） |
| `ai_battle.gd` | `AIBattleSim` — 无头对局模拟器（AI vs AI 完整一局） |
| `ai_trainer.gd` | `AITrainer` — 自博弈权重训练主脚本 |
| `best_weights.tres` | 训练产出的最优权重（`.gitignore`） |

## 训练架构

```
AITrainer (SceneTree, headless)
  │
  ├─ AIBattleSim.simulate(ai3, ai2) × 50 局/轮
  │     ├─ Round.new() → process_play() 循环
  │     ├─ AICore.choose() → AI3_TRAINED / AI2_GREEDY
  │     └─ CardTracker 记牌
  │
  ├─ 随机扰动 _perturb()：1~3 个参数 × 0.8~1.2
  ├─ 爬山法：胜率提升 → 采纳，否则回退
  └─ 每 20 轮存盘 best_weights.tres
```

## 使用方法

```bash
# 开始训练（200 轮，每轮 50 局 = 共 10000 局）
godot --headless --path . --script _devtest/ai_training/ai_trainer.gd

# 调整训练参数：修改 ai_trainer.gd 中的常量
#   EVAL_GAMES        = 50    每轮评估局数
#   MAX_ITERS         = 200   训练总轮数
#   CHECKPOINT_ITERS  = 20    存盘间隔
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