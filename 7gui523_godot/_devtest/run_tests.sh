#!/usr/bin/env bash
# ═══════════════════════════════════════════════════════════════
# run_tests.sh — 测试入口防假绿包装脚本 (Linux CI)
#
# 用法：
#   bash _devtest/run_tests.sh [godot_path] [project_path]
#
# 默认：godot_path=godot, project_path=.
#
# 退出码：
#   0 = 全部通过
#   1 = 编译错误（Parse Error / SCRIPT ERROR）
#   2 = 测试失败（failed > 0）
#   3 = Godot 进程异常退出或未输出汇总行
# ═══════════════════════════════════════════════════════════════

set -o pipefail

GODOT="${1:-godot}"
PROJECT="${2:-.}"

echo "[run_tests] Godot: $GODOT"
echo "[run_tests] Project: $PROJECT"

# 临时文件
TEMP_OUT=$(mktemp)
TEMP_ERR=$(mktemp)
cleanup() { rm -f "$TEMP_OUT" "$TEMP_ERR"; }
trap cleanup EXIT

# 运行测试，同时捕获退出码
"$GODOT" --headless --path "$PROJECT" --script _devtest/test_main.gd >"$TEMP_OUT" 2>"$TEMP_ERR"
GODOT_EXIT=$?

# 合并输出
COMBINED=$(cat "$TEMP_OUT" "$TEMP_ERR")
echo "$COMBINED"

# ── 检查 1：编译/脚本错误（在合并输出中搜索） ──
if echo "$COMBINED" | grep -qE "Parse Error|SCRIPT ERROR|Compile Error|Failed to load script|Parser Error"; then
	echo "[run_tests] FAIL: Compile/script error detected"
	exit 1
fi

# ── 检查 2：Godot 进程退出码 ──
if [ "$GODOT_EXIT" -ne 0 ]; then
	echo "[run_tests] FAIL: Godot exited with code $GODOT_EXIT"
	exit 2
fi

# ── 检查 3：汇总行解析 ──
RESULT_LINE=$(echo "$COMBINED" | grep "汇总 Result:" | tail -1)
if [ -z "$RESULT_LINE" ]; then
	echo "[run_tests] FAIL: No summary line found — tests may not have run"
	exit 3
fi

echo "[run_tests] $RESULT_LINE"

FAILED_COUNT=$(echo "$RESULT_LINE" | sed -n 's/.*Result:\s*\([0-9]\+\)\s*failed.*/\1/p')
if [ -z "$FAILED_COUNT" ]; then
	echo "[run_tests] FAIL: Cannot parse result line"
	exit 3
fi

if [ "$FAILED_COUNT" -gt 0 ]; then
	echo "[run_tests] FAIL: $FAILED_COUNT test(s) failed"
	exit 2
fi

echo "[run_tests] PASS: All tests passed"
exit 0