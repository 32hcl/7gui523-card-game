# 7鬼523 — 游戏规则与设定

---

## 〇、项目目录结构

```
7gui523_godot/                  # Godot 4.7 项目根目录
├── project.godot               # 引擎配置：游戏名"七鬼523"、v1.0.0、主场景入口、
│                               #   竖屏 680×1020 (canvas_items/expand)、黑体字体
├── GAME_RULES.md               # 本文件：游戏规则与设定
│
├── scenes/                     # 场景文件 (.tscn) — 主管全局屏幕分区布局，不含运行时逻辑
│   ├── play/play.tscn          #   主游戏场景，屏幕分为 6 个区：
│   │                           #     [顶栏]      标题"七鬼523"、牌堆余量、HP 文字、回合数
│   │                           #     [Boss区]    头像、Boss手牌(牌背)、分隔线、Boss补牌
│   │                           #     [绿毡桌面]  打出的牌、桌面分数字、HP 条(玩家/Boss)、牌堆显示
│   │                           #     [手牌区]    上排补牌区 + 下排主力区、商店按钮、重新开始
│   │                           #     [GameOver]  遮罩层 → 标题/原因/再来一局按钮
│   │                           #     [信号连线]  NewGameBtn.pressed / AgainBtn.pressed → _on_reset_pressed
│   └── shop/shop_panel.tscn    #   商店面板，全屏覆盖层分为 4 个区：
│                               #     [遮罩]      半透明暗色 ColorRect（防误触下层）
│                               #     [顶栏]      标题"关末商店"、筹码余额、关闭按钮
│                               #     [可滚区]    固定区(FixedItems)、刷新区(RefreshItems)、附魔区(EnchantItems)
│                               #     [底栏]      关闭按钮、进入下一关按钮
│
├── scripts/                    # 脚本 (.gd) — 全部 class_name 全局可访问，无 class 环依赖
│   ├── core/                   # 核心数据层（6 个文件，均 extends RefCounted）
│   │   ├── card.gd             #   Card：id(0-53不可变身份锚)、copy_mark(副本标记)、RANK_MAP(15级)、score_from_point、from_id、revert、sorted_order
│   │   ├── cardtype.gd         #   CardTypeResult：11 种枚举(SINGLE..GUI523)、TYPE_RANK、parse_any_card_type / can_beat
│   │   ├── deck.gd             #   Deck(含 DeckManager)：54 张牌、单数组 + active_count 分界线、抽牌/回收/补牌/商店插入
│   │   ├── player.gd           #   Player(含 PlayerData)：normal_hand(5)+instant_hand(2)、collect_hand_into/hand_size/is_hand_empty/next_copy_mark、名字"32%Hcl"
│   │   ├── enchant.gd          #   Enchant：9 种 EnchantType、Enchantment/EnchantedCard 数据模型
│   │   └── score.gd            #   Score：calculate_score(桌面分) + calculate_pressure_bonus(压迫加分)
│   │
│   ├── game/                   # 游戏逻辑层（2 个文件，均 extends RefCounted）
│   │   ├── round.gd            #   Round：process_play 状态机（invalid/continue/round_win/boss_finish/instant_win）、轮换、补牌、信号
│   │   └── campaign.gd         #   Campaign：9 关 HP/筹码/入场费、胜利/失败结算、关卡规则（引用 GameConfig）
│   │
│   ├── shop/                   # 商店系统（独立目录，后续扩展卡组构建核心玩法）
│   │   └── shop.gd             #   Shop：商品数据拼装(_card)、FIXED区(关末补货)、REFRESHABLE区(30筹码刷新)、附魔商店(≥32筹码解锁)、save/load
│   │
│   ├── ui/                     # UI 表现层（7 个文件：5 层单继承 + 2 个独立组件）
│   │   ├── play_base.gd        #   extends Control [第1层] 色板常量、@onready 节点引用、响应式布局、装饰构建
│   │   ├── play_anim.gd        #   extends play_base [第2层] _render_hand_row(BottomHand/TopHand)、牌桌动画、溶解/幽灵卡、_spawn_floating_damage
│   │   ├── play_ui.gd          #   extends play_anim [第3层] ShopPanel 接入、商品行渲染、购买/刷新/血量回复
│   │   ├── play_input.gd       #   extends play_ui [第4层] 拖拽出牌(_drag_indices)、双击桌面(不要)、弃牌计数、选牌高亮
│   │   ├── play.gd             #   extends play_input [第5层] _start_new 开局、Round 信号回调、AI 调度(_schedule_ai_if_needed)、玩法帮助
│   │   ├── shop_panel.gd       #   extends Control 商店面板独立组件（不参与 play 继承链）
│   │   └── card_visual.gd      #   extends Control 单牌渲染：着色器溶解(DISSOLVE_AMT)、伪 3D 倾斜(QUICK_TILT)、SUIT_DIR 花色路径映射
│   │
│   └── ai/                     # AI 决策层（3 个文件）
│       ├── ai.gd               #   AICore(extends RefCounted)：AI1 规则(最小压过)、AI2 贪心(8维评分)、CardTracker 记牌器
│       ├── ai_weights.gd       #   AIWeights(extends Resource)：8 维 base/score_card/special/endgame/combo_break/table_score/defensive/tracker 权重
│       └── boss_ai.gd          #   BossAI(extends RefCounted)：关卡特殊行为入口 — 当前全部委托 AI1(Rule)
│
├── global/                     # 全局配置（class_name 全局访问，不注册 autoload）
│   ├── card_type_names.gd      #   CardTypeNames(extends RefCounted)：CardType 枚举 → 中文名(PASS→"不出", SINGLE→"单张"...)
│   ├── game_config.gd          #   GameConfig(extends RefCounted)：NORMAL_MAX=5、INSTANT_MAX=2、PLAYER_INIT_HP=140、BOSS_NAMES[1-9]、ENTRY/REWARD_CHIPS
│   ├── save_manager.gd         #   SaveManager(extends RefCounted)：save_json/load_json/file_exists/delete_file — 纯 I/O 不涉及序列化
│   ├── event_bus.gd            #   EventBus(extends RefCounted)：15 个 signal — ROUND_START/TURN_CHANGED/ROUND_WIN/CARD_PLAYED/GAME_OVER...
│   ├── game_state.gd           #   GameState(extends RefCounted)：campaign/shop/round/hp/ai/decks/tracker 等 16 个属性、is_player_turn/is_ai_turn/reset_round_state
│   └── audio_manager.gd        #   AudioManager(extends RefCounted)：BGM/SFX/音量接口空壳，已注册 EventBus 事件钩子
│
├── data/                       # 运行时/训练数据
│   └── ai/                     #   2 个 .tres 权重文件：
│       ├── ai4_weights.tres    #     AI4 预训练权重（AIWeights Resource 实例）
│       └── best_weights.tres   #     最优权重快照
│
├── art/                        # 美术资源
│   ├── fonts/                  #   字体（1 个 ttf）：simhei.ttf — 黑体中文字体
│   ├── Top-Down/Cards/         #   扑克牌正面（4 花色 × 13 张 = 52 张 PNG）
│   │   ├── Spades/{00-12}.png  #     黑桃：00=A, 01=2, ..., 12=K
│   │   ├── Hearts/{00-12}.png  #     红桃
│   │   ├── Clubs/{00-12}.png   #     梅花
│   │   ├── Diamonds/{00-12}.png#    方块
│   │   ├── joker_black.png     #     小鬼（card_visual.gd 引用）
│   │   ├── joker_red.png       #     大鬼
│   │   └── Card_Back-Red-88x124.png  #  牌背（Boss 手牌显示）
│   ├── Top-Down/Chips/         #   筹码贴图（ChipsA/B Flat + Outline × 2 色 + Template 8 种尺寸）
│   ├── ui/                     #   UI 矢量图（chip_black_500/red_100/green_25.svg、corner_gold.svg、lantern_red.svg、xiangyun_gold.svg）
│   └── effects/                #   特效：gloden_glow.tres — 金色发光渐变（card_visual.gd 流光闪光引用）
│
├── _devtest/                   # 开发测试工具（不对玩家暴露）
│   ├── test_util.gd            #   共享断言 + 造牌辅助（CardTestHelper）
│   ├── test_cardtype.gd        #   牌型解析 / canBeat / typeRank 全覆盖
│   ├── test_score.gd           #   压迫加分 / calculateScore / tableScore
│   ├── test_deck_player.gd     #   54张牌堆 / 抽取补牌回收 / 商店销毁 / 手牌排序删除
│   ├── test_ai_round.gd        #   AI 枚举首出 / 自由局不空过 / 非法牌型 / 压不过 / PASS / GUI523
│   ├── test_campaign.gd        #   Campaign + 商店 + 伤害结算
│   ├── test_sort.gd            #   手牌排序（rank 降序 → 花色 Unicode 升序）
│   ├── test_main.gd            #   汇总入口：依次运行以上 6 个测试，打印 FAIL/PASS 汇总，exit_code=0/1
│   ├── card_lab.tscn + .gd     #   卡牌实验室：可视化调试牌型
│   ├── battle.tscn + .gd       #   战斗调试场景：集成实战模拟
│   ├── test_main_output.txt    #   最近一次 test_main 控制台输出
│   ├── test_main_err.txt       #   最近一次 test_main 错误日志
│   ├── test_sort_output.txt    #   最近一次 test_sort 输出
│   ├── test_sort_err.txt       #   最近一次 test_sort 错误日志
│   └── ai_training/            #   AI 训练器（7 个文件）
│       ├── ai_trainer.gd       #     训练主循环
│       ├── ai_eval.gd          #     评估器
│       ├── ai_metrics.gd       #     指标统计
│       ├── ai_battle.gd        #     对战模拟
│       ├── ai4_weights.tres    #     训练中间权重快照
│       ├── best_weights.tres   #     训练最优权重
│       └── README.md           #     训练器使用说明
│
└── .godot/                     # Godot 引擎自动生成（编辑器缓存、导入缓存、着色器缓存）
```

> **关键配置**（来自 `project.godot`）：  
> - 入口场景：`scenes/play/play.tscn`  
> - 窗口尺寸：680 × 1020（竖屏手游比例）  
> - 缩放模式：`canvas_items` + `expand`  
> - 自定义字体：`art/fonts/simhei.ttf`（黑体，支持中文）  
> - global/ 下 6 个文件全部通过 `class_name` 全局访问，不注册 autoload

---

## 一、牌库

### 1.1 标准牌库（54 张）

| 花色 | 点数 | 数量 |
|------|------|------|
| 黑桃 / 红桃 / 梅花 / 方块 | A K Q J 10 9 8 7 6 5 4 3 2 | 各 4 张 = 52 张 |
| 无花色 | 大鬼 | 1 张 |
| 无花色 | 小鬼 | 1 张 |

### 1.2 牌点大小排名（rank 值越大牌越大）

```
7(15) > 大鬼(14) > 小鬼(13) > 5(12) > 2(11) > 3(10) >
A(9) > K(8) > Q(7) > J(6) > 10(5) > 9(4) > 8(3) > 6(2) > 4(1)
```

### 1.3 伤害牌（有 desk 分的牌）

| 点数 | 伤害值 |
|------|--------|
| 5 | 5 HP |
| 10 | 10 HP |
| K | 20 HP |
| 其余 | 0 |

---

## 二、牌型

### 2.1 牌型一览

| 牌型 | 张数 | 说明 |
|------|------|------|
| 单张 | 1 | 任意 1 张牌 |
| 对子 | 2 | 2 张同点数（不含鬼牌） |
| 三张 | 3 | 3 张同点数（不含鬼牌） |
| 三带一 | 4 | 3 张同点 + 1 张任意 |
| 三带二 | 5 | 3 张同点 + 2 张同点另一组 |
| 炸弹 | 4 | 4 张同点数（不含鬼牌） |
| 五同点 | 5+ | 5 张及以上同点数 |
| 王炸 | 2 | 大鬼 + 小鬼 |
| 7gui523 | 5 | 7 + 鬼(任一) + 5 + 2 + 3 |

### 2.2 牌型等级（比较优先级）

```
非法(-2) < 不出(-1) < 单张(0) < 对子(1) < 三张(2) <
三带一(3) < 三带二(4) < 炸弹(5) < 王炸(6) < 五同点(7) < 7gui523(999)
```

- 出牌必须与上家**同牌型**才能压过（普通牌型：单张压单张、对子压对子、三张压三张…）
- 特殊牌型（炸弹/王炸/五同点/7gui523）不受限制，可跨牌型压制任何牌
- 同牌型：比 points 大小（rank 值比较）
- 三带 X：先比主体点数，再比带的牌点数

---

## 三、出牌规则

### 3.1 基本流程

1. 双方从各自牌堆顶摸 5 张手牌（最大手牌数 = 5）
2. 轮流出牌（双指针轮换）
3. 必须出合法牌型，且必须压过对方上一手牌
4. 可以"不要"（Pass）跳过

### 3.2 关键规则

| 规则 | 说明 |
|------|------|
| **自由局** | 无历史出牌时 → 空出 = 非法（不能空过），必须出手牌 |
| **不要（Pass）** | 有历史出牌时 → 空出 = "不要" → 上家赢下本手 → 双方补牌至 5 张 |
| **出牌后** | 不自动补牌。补牌只在"不要"时触发 |
| **玩家手牌+牌堆双空** | 打出最后一手 → 玩家直接获胜 |
| **BOSS 手牌+牌堆双空** | 返回 `boss_finish`，最后一张牌完成后按玩家剩余牌数结算 `3N` |
| **7gui523** | 打出即获胜（无视当前桌面状态） |

### 3.3 压迫（反压）

当你用**同牌型同点数**压过对方上一手，且对方那手牌全是伤害牌时：
→ 触发"大牌反压"，额外获得压迫加分（累计到桌面 bonus）

---

## 四、伤害结算

### 4.1 每手结算（有人"不要"时）

```
桌面伤害 = 桌面所有 5/10/K 分（card.score） + 压迫加分（table_bonus）
```

- 上家（最后出牌者）获胜
- 对家（说"不要"的人）扣等量 HP

### 4.2 一关结束

| 结局 | 条件 | 结算 |
|------|------|------|
| **玩家先出完** | 玩家手牌+牌堆双空 | 按现有玩家胜利流程结算 |
| **BOSS 先出完** | BOSS 手牌+牌堆双空 | 等最后一张牌动画完成，玩家剩余牌数 N × 3 扣除玩家 HP |
| **HP > 0** | 结算后玩家 HP > 0 | 进入下一关 |
| **HP ≤ 0** | 结算后玩家 HP ≤ 0 | 游戏结束 |

---

## 五、关卡系统

### 5.1 全局配置

| 参数 | 值 |
|------|-----|
| 玩家初始 HP | 140 |
| 玩家最大 HP | 210 |
| BOSS HP | 110 |
| 总关卡数 | 9 |
| 每张余牌回复/扣除 | 3 HP |

### 5.2 入场与奖励筹码

| 关 | 入场费 | 获胜奖励 |
|----|--------|----------|
| 1 新手 | 1 | 3 |
| 2 抹牌虫 | 5 | 12 |
| 3 急眼雀 | 10 | 25 |
| 4 疯狗 | 20 | 50 |
| 5 赖账鬼 | 50 | 120 |
| 6 不识数 | 100 | 250 |
| 7 黑老大 | 200 | 500 |
| 8 千里眼 | 400 | 1000 |
| 9 赌场老板 | 800 | 2000 |

### 5.3 BOSS 一览

| 关 | BOSS | 特殊规则 |
|----|------|----------|
| 1 | 新手 | 无 |
| 2 | 抹牌虫 | 无 |
| 3 | **急眼雀** | 每轮 BOSS 必定先手 |
| 4 | **疯狗** | BOSS 每张出牌附带 ×3 额外伤害 |
| 5 | **赖账鬼** | 同花色可无视点数压过（仅对 J 以下单张）；禁止玩家出相同点数 |
| 6 | **不识数** | 50% 概率蒙混过关（同牌型即算压过） |
| 7 | **黑老大** | BOSS 先出完时，强制弃掉玩家手中最大的一张牌 |
| 8 | 换牌鬼 | 无特殊规则 |
| 9 | **赌场老板** | 禁止打出 7gui523；不可通关的最终挑战，Boss 不掉血 |

### 5.4 赌场门票与关卡筹码

- 进入赌场前需支付一次 **5,000,000 金币**购买门票。
- 金币门票与每关开始时消耗的筹码完全独立，不能互相替代或回滚。
- 门票资格在同一轮战役的各关之间保留；玩家死亡或最终挑战结束后失效。
- 第 9 关赌场老板不可被击败：玩家不能用 7gui523，也不能通过正常出完牌获得通关。

---

## 六、商店系统

### 6.1 进入条件

除第 9 关最终挑战结束外，每关结束后自动进入商店。第 9 关结束即为本轮战役终止，不进入商店。商店分为两个区域：

### 6.2 不可刷新区（FIXED）— 关末自动补货

#### 回复类

| 物品 | 价格 | 效果 |
|------|------|------|
| 小回复药水 | 30 筹码 | HP +20 |
| 大回复药水 | 60 筹码 | HP +50 |

#### TO_HAND（下关开打直接到手牌）

| 物品 | 价格 | 库存 | 说明 |
|------|------|------|------|
| 5 点牌 | 40 | 2 | 伤害 5HP |
| 2 点牌 | 30 | 3 | 大单压牌 |
| 3 点牌 | 25 | 3 | 大单压牌 |
| K 点牌 | 30 | 2 | 伤害 20HP |
| 10 点牌 | 35 | 2 | 伤害 10HP |

### 6.3 可刷新区（REFRESHABLE）— 花 30 筹码手动刷新

#### TO_PLAY_DECK（洗入本关游玩牌堆）

| 物品 | 价格 | 库存 |
|------|------|------|
| A 点牌 | 35 | 2 |
| Q 点牌 | 28 | 2 |
| J 点牌 | 26 | 2 |
| 9 点牌 | 18 | 3 |
| 8 点牌 | 18 | 3 |
| 7 点牌 | 15 | 3 |
| 6 点牌 | 15 | 3 |
| 4 点牌 | 12 | 3 |

#### TO_HIDDEN（进入隐藏牌堆）

| 物品 | 价格 | 库存 |
|------|------|------|
| 5 点牌 | 40 | 2 |
| A 点牌 | 35 | 2 |
| K 点牌 | 30 | 2 |

---

## 七、二级附魔商店

### 7.1 进入条件

本关在一级商店**累计消费 ≥ 32 筹码**后解锁二级。

### 7.2 规则

- 每关结束自动补货，随机展示 **2 件**附魔
- 已购买的附魔等级不会再次出现（该精确 `类型_Lv` 不再入池）
- 附魔**永久存在**于小丑牌（大鬼/小鬼），跨关跨局均保留
- 只有小丑牌（大鬼/小鬼）可以附魔

### 7.3 附魔列表

| 附魔 | 等级 | 价格 | 效果 |
|------|------|------|------|
| 护盾 | Lv1 | 30 | 不能被压迫伤害抢走 |
| 护盾 | Lv2 | 70 | 同上 |
| 吸血 | Lv1 | 40 | 造成伤害时回复 2 HP |
| 吸血 | Lv2 | 90 | 造成伤害时回复 4 HP |
| 吸血 | Lv3 | 160 | 造成伤害时回复 6 HP |
| 双倍 | Lv1 | 80 | 该牌伤害 ×2 |
| 双倍 | Lv2 | 180 | 同上 |
| 穿刺 | Lv1 | 35 | 无视对手护盾 |
| 穿刺 | Lv2 | 75 | 同上 |
| 升阶 | Lv1 | 35 | 出牌 rank +1 |
| 升阶 | Lv2 | 80 | 出牌 rank +2 |
| 升阶 | Lv3 | 150 | 出牌 rank +3 |
| 连击 | Lv1 | 45 | 赢下回合时补 1 张牌 |
| 连击 | Lv2 | 95 | 赢下回合时补 1 张牌 |
| 连击 | Lv3 | 170 | 赢下回合时补 2 张牌 |
| 嘲讽 | Lv1 | 25 | 迫使对手优先压此牌组合 |
| 嘲讽 | Lv2 | 55 | 同上 |
| 刻印·伤 | Lv1 | 20 | 额外 +3 伤害 |
| 刻印·伤 | Lv2 | 50 | 额外 +5 伤害 |
| 刻印·伤 | Lv3 | 100 | 额外 +8 伤害 |
| 幸运 | Lv1 | 25 | 50% 概率额外 +4 伤害 |
| 幸运 | Lv2 | 60 | 50% 概率额外 +7 伤害 |
| 幸运 | Lv3 | 110 | 50% 概率额外 +10 伤害 |

### 7.4 附魔持久化

附魔数据保存在 `user://enchants.save`，包括：
- 已购买的附魔 ID 列表（防止该等级再次出现）
- 已附魔的卡牌及其附魔列表

---

## 八、AI 系统

### 8.1 AI1（规则驱动）

- 首出：取最小点数的同点组，≥3 张时只出 1 张
- 跟牌：取最小能压过对方的同牌型牌

### 8.2 AI2（贪心评分）

> AI2 保留用于兼容和测试，不接入当前正式关卡。当前正式 BOSS 使用 AI1。

8 维加权评分选择最优出牌：

| 维度 | 逻辑 |
|------|------|
| base | 出牌张数 ×10，出完手牌 +1000 |
| score_card | 有桌面伤害时出 5/10/K 加分 |
| special | 出炸弹 -92，王炸 -459；对手 ≤2 张时炸弹 +236 |
| endgame | 大牌 rank 扣分（rank≥13 的牌 -r×3） |
| combo_break | 拆对/三/四 -17 |
| table_score | 桌面伤害 ×2 |
| defensive | 反压风险 ×2（负向） |
| tracker | 打出已耗尽的点数 +30 |

### 8.3 记牌器（CardTracker）

- 追踪全场已出牌计数
- 推算对手可能持有的点数期望值
- 公式：`expected = remaining × (opp_hand / (opp_hand + opp_deck))`

---

## 九、牌堆系统

### 9.1 标准 54 张牌

```
花色 × 点数 = 52 张普通牌 + 大鬼 + 小鬼 = 54 张
```

### 9.2 双牌堆

每局开始前，标准牌堆洗牌后对半切，双方各持一副：
- 玩家牌堆
- BOSS 牌堆

双方牌堆各以满副 54 张为单位（`Deck.DeckManager`），`init_standard()` 洗牌后分出 **32 张游戏牌堆 + 22 张隐藏牌堆**，默认牌堆容量上限 `DECK_CAP = 32`。

### 9.3 牌堆结构（单数组 + 分界线）

```
cards[0 .......... active_count ........... 末尾]
    ◄── 游戏牌堆 ──►◄───── 隐藏牌堆 ──────►
```

- `cards`：54 张牌的 ID 有序数组
- `active_count`：游戏/隐藏分界线（游戏牌堆张数）
- 顶部补牌目标 = `DECK_CAP` = 32

| 操作 | 实现 |
|------|------|
| 抽牌 | 从游戏牌堆左端抽出，真实移出数组 |
| 出牌回收 | 追加到隐藏牌堆末尾（保底 LIFO） |
| 补牌 | 分界线右移，隐藏牌堆头部并入游戏牌堆 |
| 商店插入 | 随机位原生插入（可打断 LIFO） |

### 9.4 隐藏牌堆

商店购买的 `TO_HIDDEN` 卡牌进入隐藏牌堆，不立即使用，等待后续触发。

## 十、事件系统

| 事件 | 触发时机 |
|------|----------|
| ROUND_START | 新的一手开始 |
| TURN_CHANGED | 轮到下一个人出牌 |
| ROUND_WIN | 有人"不要"，上家赢下这一手 |
| CARD_PLAYED | 正常出牌成功 |
| CARD_PASSED | 选择"不要" |
| PRESSURE_BONUS | 压迫加分触发 |
| ERROR_INVALID_PLAY | 牌型非法 |
| ERROR_CANNOT_BEAT | 打不过上家 |
| BIG_CARD_COUNTERED | 炸弹/王炸/五同点被更大反压 |
| SPECIAL_VICTORY | 7gui523 直接获胜 |
| DECK_REFILL | 手牌从牌堆补回 5 张 |
| DECK_EMPTIED | 玩家牌堆抽空；BOSS 双空由 `boss_finish` 处理 |
| GAME_OVER | 游戏结束 |

---

## 十一、常用 UI 反馈

提示与反馈内联在 UI 层，无独立模块：
- **底部状态文字**（`play.gd::_refresh_status`）：红色（非法牌型/无法压过）、黄色（首出不能弃牌）、蓝色（普通信息）
- **浮动伤害数字**（`play_anim.gd::_spawn_floating_damage`）：桌面伤害/压迫加分/弃牌扣血，上浮淡出 1.2 秒
- **拖拽落点高亮**（`play.gd::_drop_hint`）：手牌拖到桌面时绿色高亮目标区

---

## 十二、代码结构

### 12.0 UI 继承链

对战场面按关注点拆分为 5 层单继承：

```
play_base.gd   →  [第1层] 常量 / 节点引用 / 单卡构建 / 主题装饰
play_anim.gd   →  [第2层] 手牌渲染 / 桌面动画 / 溶解与幽灵卡 / 浮动伤害数字
play_ui.gd     →  [第3层] 商店面板 / 商品渲染 / 购买刷新
play_input.gd  →  [第4层] 拖拽出牌 / 双击/单击 / 弃牌 / 选牌
play.gd        →  [第5层] 入口 / 关卡流程 / Round 信号 / AI 调度
```

终端常驻组件独立：
- `card_visual.gd` — 单牌渲染节点（着色器溶解、伪 3D）


### 12.1 对局详细流程

### 开局初始化

代码：`play.gd::_start_new` + `_ready`

1. 创建两副独立牌堆 `Deck.DeckManager`（各 54 张，双牌堆）
   - `_human_deck.init_standard()` / `_ai_deck.init_standard()`（各洗牌、Active 区 + 隐藏区）
2. 创建两名玩家 `Player.PlayerData`
   - 玩家：`is_human=true`、`ai_level=AI1_RULE`（仅作为玩家数据默认等级）
   - BOSS：`is_human=false`、`ai_level=AI1_RULE`（当前正式关卡使用）
3. 各自 `refill_normal_hand(deck)` 抽满 **普通区 5 张**
4. 双方 HP 从战役配置读取：玩家初始 HP 140、最大 HP 210，BOSS HP 110
5. `Round.new(human, ai, human_deck, ai_deck)` 挂接信号
6. `_schedule_ai_if_needed()` 确定先手

```
人类(真实玩家)         AI(发狂AI)
  ├─ 手牌 5 张          ├─ 手牌 5 张
  └─ 牌堆 54 张          └─ 牌堆 54 张
HP=140/210 current=玩家   HP=110 opponent=BOSS
```

### 12.2 一手牌的判定（核心状态机）

代码：`round.gd::process_play(play)`

每手 `current` 玩家给出一组牌，Round 返回字典做分流：

| 输入状态 | 判定 | 返回 state | current 变化 |
|----------|------|-----------|--------------|
| **空数组 且 自由局**（无上家牌） | `last_play` 为空就空过 → 非法，"不能空过" | `invalid` | `_swap()` 轮换 |
| **空数组 且 有人出过牌** | =「不要」→ 上家赢，双方补牌到 5 | `round_win` | 赢家成为下一小局先手 |
| **非空** | `parse_any_card_type` → 非法牌型 | `invalid` | `_swap()` 轮换 |
| **非空 但压不过上家** | `CardTypeResult.can_beat==false` | `cannot_beat` | `_swap()` 轮换 |
| **非空 且是 7gui523** | 直接获胜 | `instant_win` | 结束 |
| **非空 且 BOSS 手牌+牌堆双空** | 保留最后一张牌动画，进入 3N 伤害结算 | `boss_finish` | 结算后结束本关 |
| **非空 且玩家手牌+牌堆双空** | 直接获胜 | `instant_win` | 结束 |
| **非空 且合法且压过（正常）** | 从手牌移除 → 上桌 → 记 `last_play/last_player` | `continue` | `_swap()` 轮换 |

> 注：出牌后**不自动补牌**。补牌仅在「不要」触发时双方同时补回 5 张。

### 12.3 「不要」的结算链

代码：`round.gd` 空数组分支 → `_round_win(winner)`

```
对方出牌 → current 空数组"不要"
   → winner = last_player（上家）
   → 双方 refill_to_five(各补回 5 张)
   → _round_win(winner)
        ├─ 若 winner ≠ current：_swap()（赢家下一小局先手）
        ├─ damage = Score.calculate_score(桌面) + table_bonus
        ├─ 清空桌面 / bonus / last_play
        └─ round_won.emit(winner, damage)
```

`play.gd::_on_round_won` 扣血：winner==human → `_ai_hp -= damage`；否则 `_human_hp -= damage`。

### 12.4 直接获胜与 BOSS 清空牌堆

代码：`round.gd::_instant_win(winner)`

`instant_win` 仅处理 7gui523，或玩家正常打空手牌与牌堆。

BOSS 打空手牌与牌堆不再直接触发 `instant_win`，而是返回 `boss_finish`：

1. 保留 BOSS 最后一张牌的桌面动画。
2. 统计玩家剩余牌数：普通手牌 + 即时手牌 + 玩家牌堆。
3. 造成 `3N` 点玩家伤害。
4. 播放桌面白色延迟条后才扣除 HP。
5. 只根据结算后玩家 HP 判断：HP > 0 进入下一关，HP ≤ 0 游戏结束。

第 9 关例外：玩家不能打出 7gui523；即使正常打空手牌与牌堆，`instant_win` 也不会转化为玩家通关，Boss 仍不会掉血，随后结束最终挑战。

`play.gd::_resolve_boss_finish` 负责 BOSS 清空后的延迟结算与关卡推进。

### 12.5 回合轮换与 AI

代码：`round.gd::_swap`、`play.gd::_schedule_ai_if_needed`

- 每次有效行动后 `_swap()` 交换 `current ↔ opponent`（连同各自的牌堆）
- current 为 BOSS 时 `_schedule_ai_if_needed` 启动 0.7s 定时器 → `_ai_think` → `BossAI.choose` → `AICore.choose_ai1` → `process_play`
- AI 自由局若意外返回空数组：兜底强制出一张最小单张，避免"非法空过"

### 12.6 伤害与 HP 流向总图

```
桌面牌 5/10/K 分数  + 压迫加分(bonus)
        ↓ 上家"不要"时一次性结算
伤害分 → winner(上家) → opponent 扣 HP
        ↓ 反复
玩家 HP ≤ 0   →  GAME_OVER
BOSS 先出完   →  结算 3N 伤害后按玩家 HP 进入下一关或结束
```

### 12.7 数据接线汇总（阶段 C 实际做法）

```
双牌堆：
  玩家牌堆  Deck.DeckManager(54)          AI 牌堆  Deck.DeckManager(54)
       │ refill_normal_hand                    │
玩家手牌  PlayerData.normal_hand(5)          AI 手牌  PlayerData.normal_hand(5)
       └────────────── Round(current, opponent) ──────────────┘
                              │ process_play(play)
                              ├ round_won → HP 结算
                              ├ card_played / card_passed → UI 刷新
                              └ turn_changed → _schedule_ai_if_needed
```

---

## 十三、自动测试

> 纯逻辑测试位于 `_devtest/`，与游戏代码隔离，可用命令行 headless 直接运行验证，无需手动试玩。
> 覆盖：牌型规则 / 计分 / 牌堆与玩家存储 / AI 决策与回合链路。

### 13.1 测试文件结构

| 文件 | 被测对象 | 说明 |
|------|----------|------|
| `_devtest/test_util.gd` | 基础设施 | 共享断言 + 造牌辅助，供各测试复用 |
| `_devtest/test_cardtype.gd` | CardTypeResult | 牌型解析 / canBeat / typeRank全覆盖 / pass&空 |
| `_devtest/test_score.gd` | CardTypeResult + Score | 压迫加分 / calculateScore / tableScore |
| `_devtest/test_deck_player.gd` | Deck.DeckManager + Player.PlayerData | 标准54张 / 抽取 / 补牌 / 回收 / 商店销毁 / 手牌删除排序 |
| `_devtest/test_ai_round.gd` | AICore + Round | AI枚举首出 · 自由局不能空过 · 合法首出轮换 · 非法牌型 · 压不过 · PASS上家赢 · GUI523直接胜 |
| `_devtest/test_main.gd` | 汇总入口 | 依次调用上面四个，汇总失败数 |

> 说明：`test_deck_player.gd` 覆盖 Godot 双牌堆 `Deck.DeckManager` 与 `Player.PlayerData` 的存储层行为。

### 13.2 运行命令

Godot 控制台路径：`D:\app\Godot_v4.7.2-stable_win64.exe\Godot_v4.7.2-stable_win64_console.exe`

```powershell
# 跑全部纯逻辑测试（推荐）
"D:\app\Godot_v4.7.2-stable_win64.exe\Godot_v4.7.2-stable_win64_console.exe" --headless --path . --script _devtest/test_main.gd

# 只跑牌型规则
"...Godot_v4.7.2-stable_win64_console.exe" --headless --path . --script _devtest/test_cardtype.gd

# 只跑计分与压迫加分
"...Godot_v4.7.2-stable_win64_console.exe" --headless --path . --script _devtest/test_score.gd

# 只跑牌堆与玩家存储
"...Godot_v4.7.2-stable_win64_console.exe" --headless --path . --script _devtest/test_deck_player.gd

# 只跑 AI 决策与回合链路
"...Godot_v4.7.2-stable_win64_console.exe" --headless --path . --script _devtest/test_ai_round.gd
```

### 13.3 判定标准

- 任一用例失败 → 打印 `✗ FAIL` 并记录失败数
- 退出码 `0` = 全部通过；`1` = 存在失败