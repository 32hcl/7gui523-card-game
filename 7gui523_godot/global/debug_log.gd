class_name DebugLog
extends Node

# ═══════════════════════════════════════════════════════════════
# DebugLog — 事件监听式调试日志系统
#
# 用法：
#   DebugLog.info("input", "左键点击 pos=%s" % pos)
#   DebugLog.watch_signal("game", obj, "finished")
#   DebugLog.dump_rects(self)
#   DebugLog.hit_test(self, mouse_pos)
# ═══════════════════════════════════════════════════════════════

enum Level { DEBUG, INFO, WARN, ERROR }

static var enabled := true
static var min_level := Level.DEBUG
static var _active_watches: Array[RefCounted] = []

# ═══════════════════════════════════════════════════════════════
# 基础日志
# ═══════════════════════════════════════════════════════════════

static func debug(category: String, msg: String) -> void:
	_print(Level.DEBUG, category, msg)

static func info(category: String, msg: String) -> void:
	_print(Level.INFO, category, msg)

static func warn_msg(category: String, msg: String) -> void:
	_print(Level.WARN, category, msg)

static func error_msg(category: String, msg: String) -> void:
	_print(Level.ERROR, category, msg)

static func _print(level: Level, category: String, msg: String) -> void:
	if not enabled or level < min_level:
		return
	var tag: String = _level_tag(level)
	var ts: String = _timestamp()
	print("[%s] [%s] [%s] %s" % [ts, tag, category, msg])

static func _level_tag(level: Level) -> String:
	match level:
		Level.DEBUG: return "DBG"
		Level.INFO:  return "INF"
		Level.WARN:  return "WRN"
		Level.ERROR: return "ERR"
	return "???"

static func _timestamp() -> String:
	var t := Time.get_time_dict_from_system()
	return "%02d:%02d:%02d" % [t.hour, t.minute, t.second]

# ═══════════════════════════════════════════════════════════════
# 事件监听
# ═══════════════════════════════════════════════════════════════

class WatchedSignal:
	extends RefCounted
	var category: String
	var source: Object
	var signal_name: String

	func _init(p_category: String, p_source: Object, p_signal: String) -> void:
		category = p_category
		source = p_source
		signal_name = p_signal

static func watch_signal(category: String, source: Object, sig: String) -> void:
	if source == null or not is_instance_valid(source) or not source.has_signal(sig):
		warn_msg("debug_log", "watch_signal fail: no signal %s on %s" % [sig, source])
		return
	var ws := WatchedSignal.new(category, source, sig)
	var cb: Callable = _make_watch_callback(ws)
	if not source.is_connected(sig, cb):
		if source.connect(sig, cb) == OK:
			_active_watches.append(ws)
		else:
			warn_msg("debug_log", "connect fail: %s.%s" % [source, sig])

static func _make_watch_callback(ws: WatchedSignal) -> Callable:
	return func():
		info(ws.category, "[WATCH] %s.%s" % [ws.source, ws.signal_name])

static func unwatch_all() -> void:
	for ws_ref in _active_watches:
		var ws: WatchedSignal = ws_ref as WatchedSignal
		if ws != null and is_instance_valid(ws.source) and ws.source.has_signal(ws.signal_name):
			for conn in ws.source.get_signal_connection_list(ws.signal_name):
				ws.source.disconnect(ws.signal_name, conn.callable)
	_active_watches.clear()

# ═══════════════════════════════════════════════════════════════
# 运行时诊断：节点树布局快照
# ═══════════════════════════════════════════════════════════════

static func dump_rects(root: Node, depth: int = 0) -> void:
	if root == null:
		return
	var indent := ""
	for _i in depth:
		indent += "  "
	if root is Control:
		var ctrl := root as Control
		info("layout", "%s%s  rect=%s size=%s visible=%s" % [indent, ctrl.name, ctrl.get_global_rect(), ctrl.size, ctrl.visible])
	for child in root.get_children():
		dump_rects(child, depth + 1)

static func hit_test(root: Node, pos: Vector2) -> void:
	_hit_test_recursive(root, pos, 0)

static func _hit_test_recursive(node: Node, pos: Vector2, depth: int) -> bool:
	if not node is Control:
		for child in node.get_children():
			if _hit_test_recursive(child, pos, depth + 1):
				return true
		return false
	var ctrl := node as Control
	if ctrl.get_global_rect().has_point(pos) and ctrl.visible:
		var indent := ""
		for _i in depth:
			indent += "  "
		info("input", "%s* %s  rect=%s mouse_filter=%d" % [indent, ctrl.get_path(), ctrl.get_global_rect(), ctrl.mouse_filter])
		for child in node.get_children():
			_hit_test_recursive(child, pos, depth + 1)
		return true
	return false