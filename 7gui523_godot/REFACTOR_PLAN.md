# 七鬼523 架构重构分步实施计划

> 基于 2026-10-27 项目现状制定。交给新对话逐条执行。
> 核心约束：**附魔、商店只要不报错即可，功能完整性暂缓**。

---

## 一、现状摘要（必读）

### 已有的基础设施

| 模块 | 位置 | 状态 |
|---|---|---|
| **GameState 单一状态容器** | `global/game_state.gd` | ✅ 已实现，收拢了 campaign/shop/tracker/round/human/ai/decks/hp/phase/selected/discard 等 |
| **setget 转发层** | `scripts/ui/play_base.gd:106-183` | ✅ `_campaign`/`_round`/`_human` 等通过 setget 转发到 `_state`，旧代码引用不变 |
| **GameConfig 只读配置** | `global/game_config.gd` | ✅ 纯 const 平衡数值 |
| **SaveManager** | `global/save_manager.gd` | ✅ JSON 存读（静态方法） |
| **AudioManager** | `global/audio_manager.gd` | ✅ 纯代码合成音效 |
| **core 层纯逻辑** | `scripts/core/` | ✅ Card/Player/CardTypeResult/Deck/Enchant/Score 全 RefCounted |
| **测试** | `_devtest/` | ✅ 10 个测试文件 |

### 与目标架构的差距

| 方案目标 | 现状 | 差距 |
|---|---|---|
| RunState 状态根 | GameState 已存在 | 缺几个状态字段未收拢（见阶段 0） |
| CardDef/CardInstance 分离 | Card 是 def+instance 混合体 | 需拆分 |
| EffectRegistry | EnchantType 枚举 + 查表 | **暂缓**（附魔不接入结算，只要不报错） |
| data/ 配置外迁 | 全在代码里 | **暂缓**（数据量小） |
| Autoload 单例 | global/ 是 RefCounted，未注册 Autoload | 需注册 GameConfig/SaveManager/AudioManager |
| scenes/scripts 职责收窄 | play.gd 巨型脚本，已有分层 | 小幅优化，不大动 |

---

## 二、总体约束

1. **不报错底线**：每阶段结束必须通过 `--check-only` + 冒烟运行 + `_devtest` 0 failed
2. **附魔/商店不动功能**：只做"不报错"级别的适配，不接入结算、不补全逻辑
3. **渐进迁移**：CardDef/CardInstance 分离采用兼容层策略，不一步到位删 Card
4. **不新建无必要文件**：优先编辑现有文件

---

## 三、验证协议（每阶段结束必跑）

```powershell
$godot = "D:\app\Godot_v4.7.2-stable_win64.exe\Godot_v4.7.2-stable_win64_console.exe"

# 1. 语法检查
& $godot --headless --path "D:\code\c++\.vscode\7gui523\7gui523_godot" --check-only --script scripts/ui/play.gd

# 2. 冒烟运行（确认无 SCRIPT ERROR）
& $godot --headless --path "D:\code\c++\.vscode\7gui523\7gui523_godot" --quit-after 200

# 3. 单元测试
& $godot --headless --path "D:\code\c++\.vscode\7gui523\7gui523_godot" --script _devtest/test_main.gd
```

---

## 四、分阶段任务

### 阶段 0：补全 GameState 状态收拢（低风险，约 30 分钟）

**目标**：把散落在 `play_base.gd` 中但未进 GameState 的**游戏状态字段**收拢进去。

**现状**：以下字段直接定义在 play_base.gd，未转发到 `_state`：

| 字段 | 行号 | 类型 | 是否游戏状态 |
|---|---|---|---|
| `_player_can_discard` | 169 | bool | ✅ 是（弃牌窗口） |
| `_ai_busy` | 171 | bool | ✅ 是（AI 流程锁） |
| `_mask_player_draws` | 184 | bool | ✅ 是（关卡规则） |
| `_masked_player_cards` | 187 | Dictionary | ✅ 是（关卡规则） |
| `_initial_deal_finished` | 185 | bool | ✅ 是（发牌流程） |
| `_manual_hand_order` | 228 | bool | ✅ 是（手牌排序模式） |
| `_hand_jitter` | 190 | Dictionary | ❌ UI 视觉状态，不进 |
| `_drag_*` | 193-201 | 多个 | ❌ 拖拽 UI 状态，不进 |
| `_table_anim_*` | 215-221 | 多个 | ❌ 动画状态，不进 |
| `_dream_*` / `_deck_empty` | 207,222-223 | 多个 | ❌ 视觉状态，不进 |

**步骤**：

1. 编辑 `global/game_state.gd`，在"选牌 & 弃牌"或新增"流程状态"区块添加：
   ```gdscript
   var player_can_discard: bool = false
   var ai_busy: bool = false
   var mask_player_draws: bool = false
   var masked_player_cards: Dictionary = {}
   var initial_deal_finished: bool = false
   var manual_hand_order: bool = false
   ```

2. 编辑 `scripts/ui/play_base.gd`，把上述 6 个字段改为 setget 转发到 `_state`（仿照现有 `_round`/`_human` 的写法）：
   ```gdscript
   var _player_can_discard: bool:
       set(v): _state.player_can_discard = v
       get:     return _state.player_can_discard
   # ... 其余 5 个同理
   ```

3. 在 `game_state.gd` 的 `reset_round_state()` 中补全复位：
   ```gdscript
   player_can_discard = false
   ai_busy = false
   manual_hand_order = false
   ```
   （`mask_player_draws`/`masked_player_cards`/`initial_deal_finished` 按关卡流程复位，不在此处）

**验收**：验证协议全通过；搜索 `_player_can_discard` 等确认无遗漏的直接赋值（除 setget 外）。

---

### 阶段 1：CardDef / CardInstance 分离（中风险，核心重构）

**目标**：把 `Card`（def+instance 混合体）拆为不可变的 `CardDef` 和运行时 `CardInstance`，采用**兼容层渐进迁移**。

**现状**（`scripts/core/card.gd`）：
- `Card` 有 `point`/`suit`/`score`/`id`/`copy_mark`
- `ALL_54` 是静态模板数组
- `from_id(id)` 返回模板的浅复制
- `point`/`suit`/`score` 在运行时会被修改（如 campaign.gd:149 `card.point = ...`）

**步骤**：

#### 1.1 创建 CardDef（不可变静态定义）

新建 `scripts/core/card_def.gd`：
```gdscript
class_name CardDef
extends RefCounted

## 不可变卡牌静态定义。所有 CardDef 实例只应来自 ALL_54，不应被修改。

var id: int          ## 0-53 标准牌，-1 非标准
var point: String    ## "3" "10" "J" "大鬼" 等
var suit: String     ## "黑桃"等，鬼牌为空
var score: int       ## 5/10/20/0

static func from_id(id: int) -> CardDef:
    # 从 ALL_54 返回对应 def（不复制）
    ...
```

把 `card.gd` 中的 `RANK_MAP`、`ALL_54`、`get_rank`、`score_from_point` 等静态工具移到 `card_def.gd`。

#### 1.2 创建 CardInstance（运行时实例）

新建 `scripts/core/card_instance.gd`：
```gdscript
class_name CardInstance
extends RefCounted

## 运行时一张牌。持有 def_id 引用静态定义，附魔和临时修改在此。

var def_id: int = -1
var copy_mark: int = 0
var enchantments: Array = []   ## 暂缓使用，只要不报错
var temp_point: String = ""    ## 临时修改点数（如 boss_swap_low_cards），空=用 def 的

func get_def() -> CardDef:
    return CardDef.from_id(def_id)

func point() -> String:
    return temp_point if temp_point != "" else get_def().point

func suit() -> String:
    return get_def().suit

func score() -> int:
    return CardDef.score_from_point(point())
```

#### 1.3 Card 作为兼容层保留

修改 `scripts/core/card.gd`，让 `Card` 内部委托给 `CardDef` + 临时修改，对外接口不变：
- `card.point` → 返回 `_temp_point if _temp_point else _def.point`
- `card.suit` → 返回 `_def.suit`
- `card.score` → 重新计算
- `card.id` → 返回 `_def.id`
- `card.copy_mark` → 自身字段

**关键**：这一步只改 Card 内部实现，**不改任何外部调用点**，保证零破坏。

#### 1.4 验证 + 渐进迁移

- 跑验证协议，确认全部通过
- 此时 Card 接口不变，外部代码无感知
- 后续可逐步把 `Array[Card]` 改为 `Array[CardInstance]`，但本阶段不做（避免大面积改动）

**验收**：
- 验证协议全通过
- `card.point`/`card.suit`/`card.score`/`card.id` 行为与重构前完全一致
- `campaign.gd:149` 的 `card.point = ...` 仍能正常工作（写入临时修改）

**风险提示**：
- `Card` 的 `point` 目前是普通 var，改为 getter/setter 后，所有 `card.point = "X"` 的赋值走 setter，需确保 setter 写入 `temp_point` 而非 def
- `ALL_54` 的模板如果被外部直接修改会污染 def，需确认没有代码修改 ALL_54 中的元素（`from_id` 已返回复制，应安全）

---

### 阶段 2：注册 Autoload 单例（低风险）

**目标**：把 `global/` 中的 `GameConfig`、`SaveManager`、`AudioManager` 注册为 Godot Autoload（全局单例）。

**现状**：这三个类都是 `RefCounted`，使用时需 `GameConfig.new()` 或持实例。`GameState` 仍由 `play.gd` 持有（每局新建，不做 Autoload）。

**步骤**：

1. 编辑 `project.godot`，在 `[autoload]` 段添加：
   ```ini
   GameConfig="*res://global/game_config.gd"
   SaveManager="*res://global/save_manager.gd"
   AudioManager="*res://global/audio_manager.gd"
   ```
   （如果 `[autoload]` 段不存在则新建）

2. 修改调用点：
   - 搜索 `GameConfig.new()` → 改为直接引用 `GameConfig`（Autoload 单例）
   - 搜索 `SaveManager.new()` → 改为 `SaveManager`
   - 搜索 `AudioManager.new()` → 改为 `AudioManager`
   - 注意：`SaveManager` 的方法是 static 的，注册为 Autoload 后 static 方法仍可通过 `SaveManager.save_json(...)` 调用

3. 确认 `play.gd _ready()` 中的 `_config = GameConfig.new()` 等改为直接用 Autoload

**验收**：
- 验证协议全通过
- `GameConfig`/`SaveManager`/`AudioManager` 在任意脚本中可直接访问

**注意**：`AudioManager` 依赖 `_parent: Node`，注册为 Autoload 后它本身就是 Node，需检查其 `_ready()` 或初始化逻辑是否兼容。

---

### 阶段 3：_devtest 目录重命名 + 测试聚焦（低风险）

**目标**：把 `_devtest/` 改名为 `tests/`，确认核心测试覆盖。

**步骤**：
1. 重命名目录 `_devtest` → `tests`
2. 更新 `project.godot` 中相关路径（如有）
3. 跑测试确认通过

**验收**：测试全通过。

---

## 五、暂缓项（本次不做）

| 项 | 原因 |
|---|---|
| **EffectRegistry 注册表** | 附魔未接入结算，属待完善功能；当前 EnchantType 枚举只要不报错即可 |
| **附魔接入 round.gd 结算** | 用户明确说"不论多少BUG 后面改" |
| **data/ 配置外迁到 .tres** | 数据量小（9关、<10附魔），硬编码可维护 |
| **play.gd 大幅拆分** | 已有 play_base→play_anim→play_ui→play_input→play 分层，风险大收益小 |
| **card_view 对象池** | 性能优化，非架构必须 |

---

## 六、执行顺序建议

```
阶段 0（GameState 补全）→ 验证
阶段 1（CardDef/CardInstance）→ 验证
阶段 2（Autoload 注册）→ 验证
阶段 3（测试目录改名）→ 验证
```

阶段 0→2 之间无强依赖，但建议按顺序执行，每阶段独立验证。

---

## 七、关键文件索引

| 文件 | 作用 |
|---|---|
| `global/game_state.gd` | GameState 单一状态容器 |
| `global/game_config.gd` | 只读平衡数值 |
| `global/save_manager.gd` | JSON 存档 |
| `global/audio_manager.gd` | 音频 |
| `scripts/core/card.gd` | Card（def+instance 混合，待拆分） |
| `scripts/core/player.gd` | PlayerData（手牌） |
| `scripts/core/cardtype.gd` | CardTypeResult（纯静态牌型） |
| `scripts/core/deck.gd` | DeckManager |
| `scripts/core/enchant.gd` | EnchantType 枚举（暂缓） |
| `scripts/core/score.gd` | 伤害计算 |
| `scripts/game/round.gd` | 回合状态机 |
| `scripts/game/campaign.gd` | 战役/关卡 |
| `scripts/shop/shop.gd` | 商店（暂缓功能） |
| `scripts/ui/play_base.gd` | 状态 setget 转发 + 常量 + 节点引用 |
| `scripts/ui/play.gd` | 主流程 |
