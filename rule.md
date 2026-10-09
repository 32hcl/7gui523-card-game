# 七鬼523 - 项目总结

## 项目概述
斗地主变体卡牌游戏，使用C++17/Qt6开发，支持AI对战和关卡模式。

> **⚠️ 每次提交代码必须同步更新本文档！** 包括但不限于：新增/删除文件需更新目录结构；修改规则/牌型/数据结构需更新对应章节；新增Boss/AI需更新角色命名和能力表；修改HP/参数默认值需更新关键数据结构表。

## 构建命令
```powershell
# ⚠️ 必须使用 Qt 自带的 64 位 MinGW 13.1（D:\app\qt-c++\Tools\mingw1310_64）。
# PATH 中默认的 D:\code\w64devkit 是 32 位(i686)工具链，与 64 位 Qt6 做位数校验时
# 会报 "Qt6Config.cmake version 6.11.2 (64bit) not compatible"，导致 BUILD_GUI=OFF、
# game 目标不生成。构建目录：项目内 build-mingw64（旧的外部 build/build_7gui523 目录已删除）。
$env:PATH = "D:\app\qt-c++\Tools\mingw1310_64\bin;" + $env:PATH
cmake -B d:\code\c++\.vscode\7gui523\build-mingw64 -S d:\code\c++\.vscode\7gui523 -G Ninja `
  -DCMAKE_CXX_COMPILER="D:/app/qt-c++/Tools/mingw1310_64/bin/g++.exe" `
  -DCMAKE_C_COMPILER="D:/app/qt-c++/Tools/mingw1310_64/bin/gcc.exe" `
  -DCMAKE_MAKE_PROGRAM="D:/app/qt-c++/Tools/Ninja/ninja.exe" `
  -DQt6_DIR="D:/app/qt-c++/6.11.2/mingw_64/lib/cmake/Qt6" `
  -DCMAKE_PREFIX_PATH="D:/app/qt-c++/6.11.2/mingw_64"
cmake --build d:\code\c++\.vscode\7gui523\build-mingw64 --target <target>
# GUI 依赖组件：Qt6 Widgets / Multimedia / Svg（QSvgRenderer 需要 Svg）
```

## 测试命令
```powershell
chcp 65001  # UTF-8编码，否则中文乱码
d:\code\c++\.vscode\7gui523\build-mingw64\ai_vs_ai.exe    # AI vs AI 对战测试
d:\code\c++\.vscode\7gui523\build-mingw64\all_vs_all.exe   # 关卡通关率测试
d:\code\c++\.vscode\7gui523\build-mingw64\unit_tests.exe   # 单元测试
d:\code\c++\.vscode\7gui523\build-mingw64\game.exe         # GUI 游戏（运行时已部署，双击直接运行）
```

---

## 游戏规则

### 牌面与牌型
一副54张标准扑克（含大鬼/小鬼），使用 **4 suits × 13 ranks + 2 jokers**。

**牌点大小**（从大到小）：`7 > 大鬼 > 小鬼 > 5 > 2 > 3 > A > K > Q > J > 10 > 9 > 8 > 6 > 4`

| 牌型 | 说明 | 压牌规则 |
|------|------|----------|
| **单张** Single | 1张 | 同牌型，点数更大 |
| **对子** Pair | 2张同点 | 同牌型，点数更大 |
| **三张** Triple | 3张同点 | 同牌型，点数更大 |
| **三带一** TripleWithOne | 3张同点 + 1张任意 | 三条点数更大 |
| **三带二** TripleWithTwo | 3张同点 + 2张任意 | 三条点数更大 |
| **炸弹** Bomb | 4张同点 | 可压任何非炸弹牌型 |
| **五同点** FiveOfAKind | 5张同点（换牌虫作弊产物） | 可压炸弹 |
| **王炸** Rocket | 大鬼+小鬼 | 可压炸弹及以下 |
| **7gui523** | 7+鬼+5+2+3（五张全套） | 无敌牌型，压一切 |

### 分值牌
- **5**、**10**、**K** 为分值牌：**5 计5分**、**10 计10分**、**K 计20分**
- 回合结束时，桌面上的分值牌归最后出牌方所有

### 特殊胜利
打出 `7 + 鬼(大/小) + 5 + 2 + 3` 五张关键牌（牌型 7gui523）时，**立即获胜**（必须打出，仅持有不生效）。

### 牌堆机制（双牌堆模式）
- 双方各有独立牌堆（27张），开局各摸5张
- 每轮打完后从己方牌堆补至满手（最多5张）
- 先把手牌+牌堆全部出完的一方获胜
- 另一方手牌中分值牌归胜方

### 关卡模式概览
- 共 9 关，每关 Boss 拥有特殊能力（AI 强度递增、出牌限制、额外伤害等）
- **第 9 关（赌场老板）为不可通关的最终挑战**：Boss 不掉 HP、禁止玩家打出 7gui523。玩家无法真正获胜，存活到终局即为挑战结束，显示"最终挑战结束"而非"全部通关"
- 入园需花费 500 万金币购买筹码，一次入园可连续挑战至通关或战死

---

## 关卡模式

### 角色命名
| 旧名 | 新名 | 关卡 | AI / 能力 | Boss先手 | 称号 |
|------|------|------|-----------|----------|------|
| 傻子 | 新手 | 1 | ai1_idiot | 否 | 赌场学徒 |
| 骗子 | 换牌虫 | 2 | ai2_liar (可换牌) | 否 | 街头老千 |
| 急性子 | 急眼雀 | 3 | ai1_idiot | **是** | 暴躁赌客 |
| - | 疯狗 | 4 | 出牌数×3伤害 | 否 | 赌场打手 |
| - | 赖账鬼 | 5 | 禁用同点压牌 + 同花单张压制 | 否 | 高利贷主 |
| - | 不识数 | 6 | 50%牌型相同即可压 | 否 | 赌场荷官 |
| - | 黑老大 | 7 | ai3_search (深度8) | 否 | 赌场头目 |
| - | 千里眼 | 8 | ai4_search (深度可调) | 否 | 赌场监察 |
| - | 赌场老板 | 9 | ai4_search + 不可通关 | 否 | 幕后老板 |

> **⚠️ 第 9 关特殊规则**：不可通关的最终挑战。Boss 不掉 HP（无伤害结算）、禁止玩家打出 7gui523。玩家无法获胜，只能尽力存活更久。战役结束后无论胜负均显示"最终挑战结束"。

### Boss特殊能力（仅对Boss生效）
- **疯狗**: Boss每出一张牌咬玩家3血(对子6血,三条9血…)，轮末结算扣玩家HP
- **赖账鬼**: 玩家不能用相同点数压Boss的牌；上家单张<J时Boss可用同花色任意单张压（无视点数）
- **不识数**: Boss Pass前50%概率强行出同牌型最小牌；压牌时50%概率牌型相同即可压

**第 9 关（不可通关）**：Boss HP 不受伤害（无结算扣血）、禁止玩家打出 7gui523。玩家无法真正获胜，存活到终局即为挑战结束。Campaign::isVictory() 在此关永远为 false，改用 `isCampaignFinished()` 判定终局，结果显示"最终挑战结束"而非"全部通关"。

### 关卡HP机制
- 玩家初始HP=140，上限210；Boss HP=110
- 每轮输方扣分差值作为HP伤害
- 双方剩余牌总数 × healPerCard(3) 的HP变化，胜方加血、败方扣血
- 每关剔除部分牌（`getLevelRemoveTable`），学完后的牌永久移除

### Boss能力代码位置
- Boss能力(游戏): `ui/screens/battle_screen_ai.cpp` (doAITurn)
- Boss能力(测试): `tests/all_vs_all.cpp` (runCampaignGame)
- 玩家限制: `ui/screens/battle_screen_buttons.cpp`


## 两大基础AI

| AI | 枚举 | 算法 | 特点 |
|----|------|------|------|
| **AI1 规则** | AI1_Rule | 最小单张→对子→同牌型最小→最小炸弹→王炸 | 无状态，不依赖对手/桌面分/追踪器 |
| **AI2 贪心** | AI2_Greedy | playGain单步评分最大化 | 拆对惩罚、炸弹保留、残局加分 |

AI3/AI4为 minimax 作弊搜索AI（searchBestPlayCheat，AI3深度8 / AI4默认深度6可调），已加入 AILevel 枚举(AI3_Search/AI4_Search)并接入 aiChoosePlay 调度与UI难度切换，非基础AI。注意它们不经过 AIEngine 插件体系。

## AIEngine插件体系（架构参考，当前未启用）

可装配式AI引擎，通过组合以下组件生成不同性格AI：

| 组件 | 选项 | 说明 |
|------|------|------|
| Evaluator | Simple/Smart/Advanced | 局面评分 |
| Searcher | None/Minimax/IterativeDeepening | 搜索策略 |
| Sampler | Perfect/Uniform/Particle | 未知牌采样 |
| Tracker | None/Basic | 出牌记录追踪 |
| Policy | Greedy/TopK | 出牌选择策略 |
| StateEvaluator | Simple | 终局判断 |

## 关键数据结构

| 结构 | 文件 | 核心字段 |
|------|------|----------|
| **Card** | core/card/card.h | point(点数), suit(花色), score(分值), seq(牌堆序号) |
| **CardTypeResult** | core/card/cardtype.h | type(牌型), keyPoint(主点数), kicker(带的牌), cards, count |
| **Player** | core/player.h | name, hand(<=5张), totalScore, collected, isHuman, aiLevel |
| **Deck** | core/card/deck.h | cards(牌堆), 洗牌/发牌/分牌 |
| **RoundResult** | game/game.h | winnerName, tableCards, playerSpecialVictory, bossSpecialVictory, handEmptied |
| **GamePosition** | game/position.h | 双方手牌/牌堆/最后出牌/回合/分数(搜索用快照) |
| **DecisionBreakdown** | ai/ai.h | 评分明细: base/scoreCard/special/endgame/comboBreak/defensive/tracker |
| **AIParams** | ai/ai_types.h | 18个AI参数向量（评分权重） |
| **CampaignConfig** | game/campaign.h | playerInitHp=140, bossMaxHp=110, healPerCard=3 |

---

## 项目目录结构

```
7gui523/
├── main.cpp                          # Qt应用入口，启动主窗口
├── CMakeLists.txt                    # CMake构建：core_obj/game_obj/ai_obj三个对象库 → game/unit_tests/ai_vs_ai/all_vs_all
├── build_game.ps1                    # Windows构建脚本（64位工具链+build-mingw64+自动部署）
├── package.ps1                       # 发布打包脚本（参数化QtRoot/MinGWBin）
│
├── core/                             # ═══ 核心层：无外部依赖 ═══
│   ├── player.h / player.cpp         # Player结构体、createPlayer/refillToFive/removeCardsFromHand
│   ├── card/
│   │   ├── card.h / card.cpp         # Card结构体、RANK_MAP(15级牌点映射)、CardType枚举
│   │   ├── cardtype.h / cardtype.cpp # CardTypeResult、parseCardType(牌型解析)、parseFiveOfAKind(五同点)、parseAnyCardType(统一解析入口)、canBeat(压牌判定)
│   │   ├── deck.h / deck.cpp         # Deck结构体、shuffleDeck(洗牌)、drawRandom(抽牌)、splitDeck(分牌)
│   │   └── rank.h                    # inline getCardRank() 辅助
│   ├── rule/
│   │   ├── score.h / score.cpp       # calculateScore(计分)、settleScoreCards(结算分值牌)
│   │   └── special.h / special.cpp   # checkSpecialVictory(七鬼523检测)
│   └── variant/
│       ├── variant_registry.h        # VariantRegistry命名空间：换牌卡记录
│       └── variant_registry.cpp
│
├── ai/                               # ═══ AI模块：两大基础AI + 插件体系 ═══
│   ├── ai.h / ai.cpp                 # AI调度：aiChoosePlay(入口)→AI1/AI2/AI3/AI4分发
│   │                                 #   枚举合法出牌 enumerateLegalPlays、拆对检测 breaksCombo
│   │                                 #   评分：playGain(便捷包装)→evaluatePlayWithBreakdown(明细)
│   ├── ai_types.h / ai_types.cpp     # AIParams：18个参数 fromVector/toVector/paramCount（deckTopSpecialPenalty当前未被评分逻辑读取）
│   ├── ai_levels.h                   # Boss名称 getBossName() + AIEngine工厂 makeBossEngine()
│   ├── players/
│   │   ├── ai1_idiot.h               # AI1声明：aiChoosePlayAI1(纯规则，无状态)
│   │   └── ai1_idiot.cpp             # AI1实现：最小单张→对子→同牌型最小→最小炸弹→王炸
│   ├── engine/                       # AIEngine插件体系（当前未启用，架构参考）
│   │   ├── ai_engine.h / .cpp        # AIEngine：装配Evaluator/Searcher/Sampler→choosePlay
│   │   ├── evaluator.h / .cpp        # 评估器：SimpleEvaluator/SmartEvaluator/AdvancedEvaluator
│   │   ├── play_selector.h / .cpp    # 出牌选择：终局检测、拦截检测
│   │   ├── policy.h / .cpp           # 策略：Greedy(最高分)/TopK(前K随机)
│   │   └── state_evaluator.h / .cpp  # 局面评估：终局判断、先手优势
│   └── plugins/                      # AI可复用插件
│       ├── minimax.h / .cpp          # Minimax搜索树：genLegalMoves/applyMove/undoMove
│       ├── searcher.h / .cpp         # 搜索器接口：NoSearcher/MinimaxSearcher
│       ├── sample_search.h / .cpp    # 采样搜索：searchBestPlaySampled(蒙特卡洛思想)
│       ├── sampler.h / .cpp          # 采样接口：PerfectSampler(全知)/UniformSampler(均匀)
│       ├── bayesian_sampler.h / .cpp # 贝叶斯采样器（基于出牌记录的概率更新）
│       ├── uniform_sampler.h / .cpp  # 均匀采样器实现
│       ├── tracker.h / .cpp          # 追踪接口：NoTracker/BasicTracker
│       ├── cardtracker.h / .cpp      # CardTracker：分侧(ME/OPP)追踪、剩余牌推断
│       ├── counter_risk.h            # 反制风险：评估出牌后被压的概率
│       ├── level_rule.h / .cpp       # 关卡规则引擎（应用Boss特殊规则）
│       ├── search_params.h           # SearchParams：搜索深度/采样数/随机性 参数包
│       ├── search_state.h            # SearchState：搜索过程中的游戏状态快照
│       ├── minimax_evaluator_adapter.h # Minimax→Evaluator接口适配
│       └── particle_sampler.h        # 粒子采样器（原型/实验性）
│
├── game/                             # ═══ 游戏逻辑层 ═══
│   ├── game.h / game.cpp             # RoundResult、finalSettlement(终局结算)、compareAndAnnounce
│   ├── round.h / round.cpp           # playRoundDualDeck(双牌堆回合主循环)
│   ├── campaign.h / campaign.cpp     # Campaign类：关卡状态机、HP伤害结算、卡牌剔除表
│   ├── level_boss.h / level_boss.cpp # ai2换牌虫：ai2_variant_deck(随机换3张)、ai2_liar_choose
│   ├── position.h / position.cpp     # GamePosition：搜索用局面快照 + advancePosition
│   └── event.h                       # 游戏事件枚举
│
├── ui/                               # ═══ UI层：Qt6 Widgets ═══
│   ├── audio/
│   │   ├── audio_manager.h / .cpp     # 音效管理器（单例+BGM循环+SFX池+音量控制）
│   │   └── audio_generator.h / .cpp   # 程序化音频生成器（WAV合成，无需外部资源）
│   ├── main/
│   │   └── mainwindow.h / .cpp       # 主窗口：堆叠路由 + 标题栏
│   ├── screens/
│   │   ├── battle_screen.h / .cpp    # BattleScreen核心：初始化+UI创建
│   │   ├── battle_screen_game.cpp    # 游戏流程：startNewGame/startGameWithFirst/发牌动画
│   │   ├── battle_screen_ai.cpp      # AI回合：doAITurn/dispatchAI + Boss特殊能力判定
│   │   ├── battle_screen_buttons.cpp # 玩家操作：onPlay/onPass/onPick + 关卡玩家限制
│   │   ├── battle_screen_round.cpp   # 回合管理：refillBoth/endRound/clearLastPlay/finishGame
│   │   ├── battle_screen_level.cpp   # 关卡模式：handleLevelModeEnd/executeCheats
│   │   ├── battle_screen_stress.cpp  # 压力测试：autoPlayTest
│   │   ├── battle_screen_anim.cpp    # 动画：flyCardsToTable/refillAnim/dealAnim
│   │   ├── battle_screen_audio.cpp   # 音效：initSounds/playSound
│   │   ├── battle_screen_ui.cpp      # UI重建+布局创建：顶栏/桌面/手牌区/右侧面板
│   │   ├── main_menu_screen.h / .cpp # 主菜单：国潮赌场风（红灯笼+金币余额+金边按钮）
│   │   └── level_select_screen.h / .cpp # 关卡选择：9层楼纵向排列+Boss卡片
│   ├── widgets/
│   │   ├── card_widget.h / .cpp      # 卡牌控件：渲染/翻转/高亮（国潮风无图模式）
│   │   ├── selectable_card.h / .cpp  # 可选卡牌：点击选中/取消
│   │   ├── chip_widget.h / .cpp      # 筹码堆叠可视化组件（四色面额）
│   │   ├── chip_badge.h / .cpp       # 筹码余额徽章（带数值+标签）
│   │   ├── boss_avatar.h / .cpp      # Boss风格化头像（9关独特配色+符号）
│   │   ├── play_effect_overlay.h / .cpp # 出牌特效层（炸弹/王炸/7gui523）
│   │   └── dialogue_overlay.h / .cpp # 对话覆盖层（旁白+Boss气泡+打字机）
│   ├── dialogs/
│   │   ├── card_picker.h / .cpp      # 选牌对话框（换牌虫关卡使用）
│   │   └── shop_dialog.h / .cpp      # 商店对话框（道具购买界面）
│   ├── cheats/
│   │   └── level_cheats.h / .cpp     # 作弊工具：变牌/设先手/限制手牌（调试用）
│   ├── router/
│   │   └── screen_router.h / .cpp    # 界面路由：主菜单↔关卡选择↔战斗
│   └── theme/
│       ├── theme_manager.h / .cpp    # 主题管理器：QSS加载/颜色令牌/主题切换
│
├── resources/                        # ═══ 主题与装饰资源 ═══
│   ├── resources.qrc                 # Qt资源文件（主题+SVG）
│   ├── themes/
│   │   └── theme_casino.qss          # 国潮赌场风 全局样式表
│   └── svg/
│       ├── xiangyun_gold.svg         # 祥云纹 金色装饰
│       ├── corner_gold.svg           # 回字纹 四角金边
│       └── lantern_red.svg           # 红灯笼 国潮装饰
│
├── data/                             # ═══ 数据/存档层（桩代码） ═══
│   ├── wallet.h / .cpp               # 钱包：余额增减
│   ├── inventory.h / .cpp            # 背包：道具增删查
│   ├── progress.h / .cpp             # 关卡进度：通关记录
│   ├── codex.h / .cpp                # 图鉴：收集记录
│   └── save/
│       ├── save_data.h               # 存档数据结构
│       └── save_manager.h / .cpp     # 存档管理器：读/写/重置
│
├── dialogue/                         # ═══ 对话系统 ═══
│   └── dialogue_engine.h / .cpp      # 剧情引擎：9关Boss战前/胜利/失败/特殊对话
│
├── shop/                             # ═══ 商店（桩） ═══
│   └── shop.h / .cpp                 # 道具购买
│
├── tests/                            # ═══ 测试 ═══
│   ├── test_common.h                 # 测试公共头：AIFn类型定义、makeOldAI/makeNewEngine工厂
│   ├── test_main.cpp                 # 单元测试入口（Catch2风格）
│   ├── ai_vs_ai.cpp                  # AI vs AI 对战（80局独立可执行文件）
│   ├── all_vs_all.cpp                # 关卡通关率测试（100局独立可执行文件）
│   ├── test_cardtype.cpp             # 牌型解析 + canBeat压牌测试
│   ├── test_score.cpp                # 计分 + 结算测试
│   ├── test_special.cpp              # 七鬼523特殊胜利测试
│   ├── test_player.cpp               # Player发牌/弃牌测试
│   ├── test_deck.cpp                 # Deck洗牌/分牌测试
│   ├── test_campaign.cpp             # Campaign关卡HP/剔除测试
│   ├── test_tracker_dual_side.cpp    # CardTracker分侧追踪测试
│   ├── test_variant_registry.cpp     # 换牌记录测试
│   ├── test_search_p0.cpp            # 搜索P0冒烟测试
│   ├── test_sampler.cpp              # 采样器测试
│   ├── test_level_rules.cpp          # 关卡规则引擎测试
│   ├── test_bayesian.cpp             # 贝叶斯采样测试
│   ├── debug_l6.cpp                  # 赖账鬼关卡调试用例
│   ├── debug_l6b.cpp                 # 不识数关卡调试用例
│   └── debug_l6d.cpp                 # 疯狗关卡调试用例
│
└── kenney_playing-cards-pack/        # 卡牌美术资源
    └── PNG/Cards (large|medium)/     # 大/中尺寸PNG卡面素材
```

---

## 游戏完整流程（⚠️ 核心！所有开发必须遵循此规范）

### 核心概念区分

| 概念 | 定义 | 范围 |
|------|------|------|
| **回合 (Round)** | 一方出牌后，双方轮流压牌，直到有人 Pass | 包含多轮"出牌→压牌" |
| **轮次 (Turn)** | 单次出牌动作 | 回合内的最小单位 |
| **先手** | 每一回合的第一个出牌者 | 新回合由上一回合赢家先出 |

**关键原则**：
- ❌ **不是**(玩家出牌→AI出牌→结束)的简单循环
- ✅ **而是**(先手出牌→对方压牌/Pass→压牌成功继续→直到有人Pass→结束回合→开始新回合)

### 完整流程图

```
═══════════════════════════════════════════════════════════════
                         🎮 一局游戏 = 多个回合(Round)
═══════════════════════════════════════════════════════════════

【阶段0: 游戏初始化】
├─ 创建两个玩家 (PlayerA=玩家A, PlayerB=电脑)
├─ 创建两个牌堆 (m_playerDeck, m_bossDeck)，各27张
├─ 双方各摸5张起始手牌
├─ 决定第一回合先手
│   ├─ 玩家选择"先手" → m_playerAIsFirst = true
│   ├─ 玩家选择"后手" → m_playerAIsFirst = false
│   └─ 玩家选择"随机" → 随机决定
└─ 调用 startNextTurn() 进入第一回合

═══════════════════════════════════════════════════════════════
                     🔁 单回合内部循环（关键！）
═══════════════════════════════════════════════════════════════

Step A: 当前回合方出牌
│
├─ 如果是玩家出牌:
│   └─ enableActionButtons() → 玩家点击选牌+出牌
│      └─ 进入 onPlayButtonClicked()
│
└─ 如果是AI出牌:
    └─ QTimer → doAITurn() → dispatchAI() → 选牌
       └─ 进入 doAITurn() 后续处理

Step B: 判定出牌结果
│
├─ ⚠️ 出牌为空 + 上一手为空(首出):
│   └─ 自动补牌 → 出最小单张（首出不允许Pass）
│
├─ ✅ 出牌为空 + 上一手不为空(Pass/不要):
│   └─ ➡️ 跳到【回合结束处理】(Step Z)
│
├─ ❌ 非法牌型 / 无法压过:
│   └─ AI: 交换current/opponent → 回到Step A
│   └─ UI: 弹窗提示 → 回到Step A
│
├─ 🏆 7gui523:
│   └─ ➡️ 游戏结束！打出者直接获胜
│
└─ ✅ 合法出牌:
    │
    ├─ 从手牌移除 → 加入桌面(tableCards)
    ├─ 记录 m_lastPlay, m_lastPlayerName
    │
    ├─ 📦 检查：出牌方手牌是否打空？
    │   ├─ 是 且 该方牌堆也空:
    │   │   └─ ➡️ 游戏结束！该方获胜
    │   │
    │   └─ 是 且 该方牌堆还有牌 且 出牌方是玩家:
    │       └─ refillToFive(玩家, 玩家牌堆)  ← 玩家回合内补牌
    │       （Boss不打空后不单独补牌，等回合结束统一 refillBoth）
    │
    └─ 🔄 交换current ↔ opponent → 回到Step A（对方轮次）
       （注意：不结束回合！只是切换出牌权）

═══════════════════════════════════════════════════════════════
              【回合结束处理】(Step Z: 有人Pass时触发)
═══════════════════════════════════════════════════════════════

仅当 某方出牌为空 且 上一手不为空 时才进入此步骤！

├─ 记录本轮赢家 winner = 上一手出牌方(lastPlayer)
│
├─ settleScoreCards(winner, tableCards):
│   └─ 赢家获得桌面上所有分值牌(5/10/K)
│
├─ m_playerWonLastRound = (winner == 玩家A)
│
├─ 📦 补牌（⚠️ 重要！）:
│   ├─ 赢家优先: refillToFive(winner, winnerDeck)
│   └─ 败方后补: refillToFive(loser, loserDeck)
│
├─ clearLastPlay() → 清空桌面+lastPlay
│
└─ ➡️ startNextTurn() → 进入新回合(N+1)

╔═════════════════════════════════════════════════════════════╗
║  ⚠️ 补牌(Refill)只在两个时机发生：                         ║
║  ① 回合内：玩家打完手牌但牌堆未空 → refillToFive(玩家)     ║
║  ② 回合结束时：赢家先补、败方后补 → refillBoth()           ║
║  ❌ 禁止：每次出牌后都 refillBoth()                        ║
║  ❌ 禁止：Boss 回合内单独补牌                               ║
╚═════════════════════════════════════════════════════════════╝

═══════════════════════════════════════════════════════════════
                         🏁 终局阶段
═══════════════════════════════════════════════════════════════

触发条件（任一满足）:
├─ 某方手牌+牌堆全部出完 → finishGame(该方)
├─ 打出7gui523 → 打出者直接获胜
└─ 双方牌堆都空 + 某回合结束 → finalSettlement(终局结算)

终局结算规则:
├─ 先出完方获得: 桌面全部分值牌
├─ 压分奖励(m_tableBonus) = 每次计算所得压分之和
├─ 另一方手牌中分值牌归胜方
└─ 比较总分: 胜方>败方=胜利, 同分=平局

═══════════════════════════════════════════════════════════════
                         📊 示例流程
═══════════════════════════════════════════════════════════════

【示例: 完整的2轮游戏】
假设: 第1轮玩家先手

第1轮 (Round 1, 玩家先手):
  ① 玩家出: 单张K              → 交换,轮到AI
  ② AI出: 单张7(压K)          → 交换,轮到玩家
  ③ 玩家Pass("不要")          → 回合结束！

  回合结束处理:
    winner=AI | settleScoreCards(AI, [K,7])
    ai补牌(refillToFive) | 玩家补牌(refillToFive)
    clearLastPlay()
    m_playerWonLastRound=false → 新回合AI先手

第2轮 (Round 2, AI先手, 因为AI上轮赢了):
  ④ AI出: 单张5              → 交换,轮到玩家
  ⑤ 玩家出: 单张7(压5)        → 交换,轮到AI
  ⑥ AI Pass("不要")          → 回合结束！

  回合结束处理:
    winner=玩家 | settleScoreCards(玩家, [5,7])
    refillBoth() | clearLastPlay()
    m_playerWonLastRound=true → 新回合玩家先手

... 继续直到游戏结束 ...
```

### UI层出牌处理规范（onPlayButtonClicked / doAITurn）

```
玩家出牌后(onPlayButtonClicked)的流程:
├─ 1. 合法性+压牌判定
├─ 2. 7gui523检查
├─ 3. 移除手牌、更新UI
├─ 4. 检查: 玩家手牌空? + 玩家牌堆空? → finishGame() ✅ 结束
├─ 5. 检查: 玩家手牌空? + 玩家牌堆有? → refillToFive(玩家,玩家牌堆) 📦 只补自己
│                                     + 禁用出牌按钮
├─ 6. 设置 m_phase=AITurn → QTimer调用doAITurn()
└─ ❌ 禁止: 调用 endRound() + refillBoth() + startNextTurn()

AI出牌后(doAITurn)的流程:
├─ 1. dispatchAI选牌
├─ 2. 若 chosen 为空(Pass):
│     ├─ 检查 Boss 特殊能力 tryForcePlay（赖账鬼/不识数强制出牌）
│     │   └─ 若出牌成功 → 跳到步骤4（视为正常出牌）
│     └─ 非首出 → 【回合结束】
│        ├─ settleScoreCards
│        ├─ refillBoth(opp=赢家, self=输家)
│        └─ startNextTurn()
├─ 3. 7gui523检查 → 立即获胜
├─ 4. 移除手牌、更新UI
├─ 5. 检查: AI手牌空? + AI牌堆空? → finishGame() ✅ 结束
├─ 6. ❌ 已移除: Boss 手牌空时不单独补牌，等回合结束统一 refillBoth
├─ 7. Boss 出炸弹/王炸首次剧情对话:
│     └─ playDialogue → return（对话结束后 onDialogueFinished → PlayerTurn → 玩家继续回应）
├─ 8. 正常情况: m_phase=PlayerTurn → enableActionButtons() → 等待玩家
└─ ❌ 禁止: AI出牌后直接endRound+refillBoth+再调用doAITurn
```

### 回合结束后的先手决定规则

```cpp
void BattleScreen::startNextTurn() {
    bool playerGoesFirst = false;

    if (m_roundCount == 1) {
        // 第一轮: 使用初始设定
        playerGoesFirst = m_playerAIsFirst;
    } else {
        // 后续轮次: 上一轮赢家先出牌
        playerGoesFirst = m_playerWonLastRound;
    }

    if (playerGoesFirst) {
        m_phase = GamePhase::PlayerTurn;
        enableActionButtons();
    } else {
        m_phase = GamePhase::AITurn;
        disableActionButtons();
        QTimer::singleShot(m_aiTurnDelayMs, this, &BattleScreen::doAITurn);
    }
}
```

### 常见Bug与修复

| Bug | 原因 | 修复 |
|-----|------|------|
| **对手无法补牌** | 每次出牌后都调用refillBoth() | 只在回合结束(Pass)时调用refillBoth() |
| **AI无限连续出牌** | doAITurn()内部直接再次调用doAITurn() | 改为调用startNextTurn() |
| **玩家无法出牌** | startNextTurn()未正确判断先手 | 基于m_playerWonLastRound决定先手 |
| **补牌后牌数不对** | Boss回合内单独补牌 | Boss手牌空不等回合结束补，等统一refillBoth |
| **分值结算错误** | 结算时机不对 | 仅Pass时settleScoreCards() |
| **七鬼523集齐误判胜** | handleSpecialVictoryCheck检查手牌是否包含 | 已删除，只保留打出时通过 parseCardType 判定；RoundResult 拆分为 playerSpecialVictory / bossSpecialVictory<br>★ `all_vs_all` / `ai_vs_ai` 出牌后立刻调用 parseCardType，不检查剩余手牌 |
| **Boss炸弹对话后跳轮** | onDialogueFinished(BossSpecial)调用startNextTurn | 改为m_phase=PlayerTurn，回合还给玩家 |
| **五同点被误杀** | parseCardType对5张同点返回Invalid，canBeat(Invalid)恒false | 统一用parseAnyCardType（标准解析失败回退parseFiveOfAKind），ai.cpp/ai_engine/position/minimax/UI校验全线替换 |
| **AI3/AI4终局判断恒真** | UI/round路径从不维护Deck.myRemaining/oppRemaining（恒0），搜索模型认为双方牌堆已空 | doAITurn/round循环在调用aiChoosePlay前刷新 selfDeck->myRemaining、oppDeck->oppRemaining |
| **关卡筹码双重发放** | handleLevelModeEnd对话框后再次addChips(playerScore×2)，而finishLevel已发getLevelRewardChips | 删除对话框后的重复addChips，奖励以finishLevel发放为准（lr.chipsDelta） |

### 参考实现

正确的回合逻辑参考: `game/round.cpp` - `playRoundDualDeck()`
这是经过**单元测试验证**的正确实现，UI层应严格遵循此逻辑。

### AI调用链
```
aiChoosePlay(player, opponent, previous, deck, tableScore, tracker)
  ├→ AI1: aiChoosePlayAI1(player, previous)                  // 纯规则，不依赖对手
  ├→ AI2: aiChoosePlayAI2(player, opponent, previous, deck, tableScore)
  │      └→ enumerateLegalPlays → playGain → 选最高分
  └→ AI3/4: aiChoosePlayAI3 / aiChoosePlayAI4                // Minimax搜索+采样
       └→ searchBestPlaySampled(minimax + sampler + tracker)
```

### 构建目标
| 目标 | 源文件 | 依赖 |
|------|--------|------|
| **game** | core_obj + game_obj + ai_obj + UI + main.cpp | Qt6 Widgets/Multimedia |
| **unit_tests** | core_obj + game_obj + ai_obj + tests/test_*.cpp | - |
| **ai_vs_ai** | core_obj + game_obj + ai_obj + tests/ai_vs_ai.cpp | - |
| **all_vs_all** | core_obj + game_obj + ai_obj + tests/all_vs_all.cpp | - |

---

## 编码规范（C++）

1. **资源安全**: 静态随机数生成器/分布使用 `thread_local` 替代 `static`，避免多线程竞争
2. **const 正确性**: 不修改的成员函数标记 `const`，参数优先 `const &` 传递大对象
3. **返回值优先**: 多输出用结构体返回 (如 `DecisionBreakdown`)，避免裸指针输出参数
4. **代码复用**: 消除重复逻辑，`paramCount()` 替代硬编码常量
5. **错误处理**: 不修改的遍历用 `const auto&`，map查询结果检查
6. **命名约定**: 文件名 snake_case，结构体名 PascalCase，函数名 camelCase
7. **获取/设置器**: `getLevelRemoveTable` 风格，简单属性允许 `public` 成员
8. **枚举**: 全局用 `enum class`，局部用普通 `enum`；枚举值使用 PascalCase

## 注意事项
- 所有Boss能力仅对Boss生效，不对玩家生效
- 测试使用固定种子 `std::mt19937 rng(12345)` 保证可复现
- 中文输出需要 `chcp 65001` 设置控制台编码
- `build/` 目录在 `.gitignore` 中，不纳入版本控制

---

## UI重构进度（国潮赌场风）

### 设计风格
- **主题**: 国潮赌场风（深红金 + 绿色赌桌 + 筹码系统）
- **主色**: 朱红 #c41e3a / 金色 #ffd700 / 赌桌绿 #0d5c2a / 深褐 #1a0f0a
- **文字色**: 米白 #fff8e7 / 金色 #ffd700 / 灰色 #c9b896

### 阶段一：主题系统搭建 ✅ 已完成
- [x] `resources/themes/theme_casino.qss` — 全局样式表（20+类控件）
- [x] `ui/theme/theme_manager.h/.cpp` — ThemeManager主题管理类
- [x] `resources/resources.qrc` — Qt资源文件（主题+SVG）
- [x] `resources/svg/*.svg` — 祥云纹/回字纹/红灯笼装饰
- [x] CMakeLists集成：`qt_add_resources` + POST_BUILD复制
- [x] MainWindow启动时自动应用Casino主题

### 阶段二：核心界面重绘 ✅ 已完成
- [x] **主菜单** — 红灯笼装饰 + 鸿运赌场名号 + 金币余额（💰，同步存档）+ 金边渐变按钮
- [x] **战斗界面** — 绿色赌桌桌面(径向渐变+金边) + 深红金顶栏 + 手牌区金边边框 + 右侧面板赌场风
- [x] **卡牌（无图模式）** — 象牙白渐变底色 + 金色边框 + 内层金边装饰
- [x] "游戏日志" → "牌局记录" 命名赌场化
- [x] 关卡选择界面（LevelSelectScreen，已实现；已入园玩家可通过 Alt+点击「闯关模式」进入选关，日常流程按序推进关卡）

### 阶段三：筹码系统融入 ✅ 已完成
- [x] ChipWidget 筹码可视化组件（四色面额：黑50/绿10/蓝5/红1）
- [x] ChipBadge 筹码余额徽章组件
- [x] 每层入场筹码配置（1/5/10/20/50/100/200/400/800）
- [x] 每层奖励筹码配置（3/12/25/50/120/250/500/1000/2000）
- [x] 战斗界面接入：开局扣入场筹码 + 胜利加奖励筹码
- [x] 结算对话框：筹码结算信息显示
- [x] 关卡模式底部筹码余额实时显示
- [x] Campaign::LevelResult 增加 chipsDelta / chipsAfter 字段
- [x] 筹码不足时无法进入关卡的拦截（battle_screen_level 入口检查）
- [ ] 筹码增减动画特效

### 阶段四：细节与打磨 🎨 已完成
- [x] **Boss头像组件** — BossAvatar 风格化圆形头像（每关独特配色+符号+光晕）
- [x] **Boss区头像接入** — 战斗界面对手区左侧显示Boss头像+名称
- [x] **炸弹特效** — 💥 橙红光晕+冲击波+震屏
- [x] **王炸特效** — 👑 金紫渐变+多道冲击波+描边文字
- [x] **7gui523特效** — ✨ 七彩旋转光芒+彩虹文字
- [x] **结算对话框重绘** — 国潮赌场风深红金渐变+金边卡片比分面板+VS布局
- [x] **卡牌选中发光** — 8层金色外发光效果+高亮边框

### 阶段五：音效BGM系统 🎵 已完成
- [x] **AudioManager音效管理器** — 单例模式，BGM循环+淡入淡出，SFX音效池，独立音量控制
- [x] **AudioGenerator程序化生成** — 纯代码合成WAV文件（无需外部音频资源）
- [x] **4首BGM** — 主菜单(爵士赌场风) / 战斗(小调紧张感) / 胜利(上升琶音) / 失败(下行音阶)
- [x] **14个SFX音效** — 点击/出牌/Pass/炸弹/王炸/7gui523/胜/负/筹码增/减/选中/取消/伤害/恢复
- [x] **战斗界面接入** — 开局BGM+出牌音效+炸弹/王炸音效+胜负BGM切换+返回停止
- [x] **主菜单接入** — 主菜单BGM+按钮点击音效
- [x] **首次启动自动生成** — 检测到audio目录为空时自动生成所有音效文件

### 阶段六：关卡选择 & Boss剧情 📖 已完成
- [x] **9关Boss命名统一** — 以ai_levels.h中getBossName()为准：新手/换牌虫/急眼雀/疯狗/赖账鬼/不识数/黑老大/千里眼/赌场老板
- [x] **对话引擎实装** — DialogueEngine单例，8种触发时机（战前/胜利/失败/首炸弹/首王炸/7gui523/玩家Pass/BossPass）
- [x] **9关Boss完整剧情** — 每关战前4-5句+胜利/失败对话，贯穿「鸿运赌场」主线叙事
- [x] **关卡选择界面** — LevelSelectScreen 9层楼纵向排列，Boss头像+称号+简介+入场/奖励筹码
- [x] **DialogueOverlay对话覆盖层** — 旁白模式（居中黑底）+ Boss对话模式（头像+气泡）+ 打字机效果 + 点击继续
- [x] **战前对话** — 关卡开局先播Boss登场对话，结束后进入猜牌
- [x] **战斗中特殊事件对话** — 双方第一次出炸弹/王炸时触发Boss反应对话（暂停流程）
- [x] **胜负结算对话** — 游戏结束播胜利/失败对话，结束后显示结算对话框
- [x] **路由集成** — 主菜单 → 关卡选择 → 战斗界面三级路由（已实现；Alt+点击「闯关模式」→ 选关 → 战斗，日常流程：主菜单 → 直接进入战斗）
- [x] 关卡进度保存与读取（clearedLevel、hasPurchasedEntry 存档）
- [x] 筹码不足拦截（battle_screen_level 入口检查）
- [x] 7-9关Boss AI能力实装（Boss 7=AI3_Search、Boss 8=AI4_Search、Boss 9=AI4_Search + 不可通关规则）