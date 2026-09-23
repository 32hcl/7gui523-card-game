# 7鬼523斗地主变体

Copyright © 2026 32%Hcl 陈龙大王

一个两人对战的扑克牌游戏，支持 AI 对战。

---

## 如何运行

### 方式一：下载 Release

1. 下载 `7gui523-win64.zip`
2. 解压到任意目录
3. 双击 `game.exe` 运行

### 方式二：从源码编译

需要 CMake 3.16+、C++17 编译器，以及 Qt 6 Widgets / Multimedia 开发包。
Qt 与编译器必须使用匹配的工具链（例如 MinGW 版 Qt 配套 MinGW）。
请在已配置编译器的终端中运行，Qt 路径按实际安装位置替换：

```powershell
cmake -S . -B build-package -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="C:/Qt/6.x/mingw_64"
cmake --build build-package --target game unit_tests
ctest --test-dir build-package --output-on-failure
```

不需要界面时，可以仅构建规则测试和 AI 训练环境，无需安装 Qt：

```powershell
cmake -S . -B build-native -G Ninja -DBUILD_GUI=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build-native --target unit_tests env_server
ctest --test-dir build-native --output-on-failure
python new_ai/test_env.py ./build-native/env_server.exe
```

Windows 使用 MSVC 时，请先打开 Visual Studio 的 x64 Native Tools 命令行。
复制项目到新目录后应使用新的构建目录；旧 `build/`、`build2/` 缓存包含原机器的绝对路径。
`unit_tests` 在 Release 模式也会执行断言，CTest 的 `core_rules` 包含 10 组规则测试。

## 操作

### 开局先手选择
游戏开始时弹出三个按钮：
- **先手** — 玩家先出牌
- **随机** — 随机决定谁先出
- **后手** — AI 先出牌

### 选卡
点击"选卡"打开卡牌选择界面，点选卡牌加入起始手牌。
- 可选 **0~5 张**，未选满的位置由牌堆随机补全
- 点击已选卡牌可移除

### 出牌
点击手牌选中，点"出牌"。支持的牌型：
单张、对子、三张、三带一、三带二、炸弹（四张相同点数）、王炸（大鬼+小鬼），Special523（7+鬼+5+2+3，最高优先级）。

### 不要
点"不要"跳过本回合。

### 难度
循环切换四档 AI 难度：
AI1 简单 → AI2 规则 → AI3 记牌 → AI4 专家 → AI1 简单

### 重新开始
重开一局。

## 牌序

从大到小：7 > 大鬼 > 小鬼 > 5 > 2 > 3 > A > K > Q > J > 10 > 9 > 8 > 6 > 4

## 牌型

单张、对子、三张、三带一、三带二、炸弹（四张相同点数）、王炸（大鬼+小鬼），Special523（7+鬼+5+2+3，最高优先级）。

顺子、连对、飞机等不合法。

## 玩法

- 每人起始 5 张手牌，从牌堆补牌
- 一方出牌后，另一方出同点数或更大的同牌型，或出炸弹/王炸，或选择"不要"
- 一方要不起，本回合结束。胜方优先补牌至 5 张，败方后补
- 牌堆抽空后当前回合继续；回合结束发现牌堆空，进入终局阶段。终局阶段出完立即结算。补牌后败者无牌时，以先出完者结算
- 终局结算：出完牌者获得桌面分值卡、累计压分奖励与对方手牌分值卡
- 分值卡：5 = 5 分，10 = 10 分，K = 20 分
- 最终比较总分，同分平局

## 特殊胜利

**一次性打出** 7 + 鬼（大鬼/小鬼均可）+ 5 + 2 + 3 这 **5 张牌**，直接获胜，不结算分数。

（注意：不是手牌里凑齐就算，必须一次性打出。）

## 压分奖励

出 5 / 10 / K 的单张或对子（含三带一、三带二中的 5/10/K）时，如果和上一手**同牌型、同点数**，额外获得本次出牌的分值。

例如：上一手出了单张 5，这一手也出单张 5，则额外获得 5 分。

## 功能

- 四档 AI 难度：AI1 简单 / AI2 规则 / AI3 记牌 / AI4 专家
- 选卡开局：自定义 0~5 张起始手牌，缺的随机补
- 先手选择：开局可选先手/随机/后手
- 出牌动画、AI 出牌动画、特殊胜利特效
- 压分飘字动画、终局结算界面
- 游戏日志面板、音效

## 项目结构

```text
core/       卡牌、牌型、计分、特殊胜利和记牌
game/       对局流程
ai/         规则 AI、搜索器、均匀/贝叶斯采样
new_ai/     Python 强化学习环境、训练评估脚本和已有模型
ui/         Qt 主窗口、对战界面、卡牌控件
data/       存档、钱包、库存和进度
dialogue/   对话逻辑
shop/       商店逻辑
tests/      核心测试、AI 对战及 env_server
music/      音效
kenney_playing-cards-pack/  牌面素材
```

`new_ai` 的训练脚本使用 NumPy、Gymnasium、PyTorch、stable-baselines3 和 sb3-contrib；
基础通信测试 `new_ai/test_env.py` 仅需 Python 标准库。
现有训练脚本部分路径依赖工作目录，启动训练前应检查脚本中的 EXE、模型和输出路径。

## 如何打包

在项目目录运行，替换为本机安装路径：

```powershell
./package.ps1 -QtRoot 'C:/Qt/6.x/mingw_64' -MinGWBin 'C:/Qt/Tools/mingw/bin'
```

CMake 和 Ninja 需在 PATH 中，也可使用 `-CMakeExe`、`-NinjaExe` 指定完整路径。
Qt 和 MinGW 路径还可以通过 `QT_ROOT`、`MINGW_BIN` 环境变量提供。

脚本在 `build-package/` 编译 Release，执行核心测试后，将程序、牌面、音效、许可证和运行库放入
`release/package-<唯一编号>/`。每次创建独立目录，保留已有发布包及存档；构建或测试失败即停止。

## AI 难度说明

| 难度 | 策略 |
|------|------|
| AI1 简单 | 随机出牌，仅遵循牌型规则 |
| AI2 规则 | 按固定优先级出最小的可出牌型 |
| AI3 记牌 | 记录已出牌，推测剩余牌分布 |
| AI4 专家 | 综合记牌、拆牌、骗牌策略 |

## 许可

本项目使用 PolyForm Noncommercial License 1.0.0 授权。

允许个人学习、研究、修改，禁止任何商业用途。
转载或使用代码时必须保留作者署名。

## 规则 v2 研究环境
三带一允许带鬼。同牌型、同 keyPoint 的单张/对子/三张/三带一/三带二按本次全部分值牌加压分奖励；炸弹/王炸/Special523 不触发。
搜索与 env_server 使用 game/position.cpp。JSON reset 可传 seed；同种子成对交换座位，终局奖励胜+1/平0/负-1。auto_play 标签版本 terminal_wdl_v2，旧模型标签不兼容。peek 返回真实隐藏世界的结果，标记 privileged，仅用于调试，禁止公平 AI 使用。
