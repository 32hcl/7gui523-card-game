class_name SaveManager
extends RefCounted

# ═══════════════════════════════════════════════════════════════
# SaveManager — 统一存档 I/O 层
#
# 提供通用的 JSON 文件读写能力。
# 各业务模块自行负责数据的序列化/反序列化，
# SaveManager 只负责文件系统操作和错误处理。
#
# 用法：
#   SaveManager.save_json("enchants.save", {"key": value})
#   var data := SaveManager.load_json("enchants.save")
# ═══════════════════════════════════════════════════════════════


const SAVE_DIR := "user://"

## 存档格式版本号。数据结构变更时递增。
## 读取旧版本存档时返回空 {}，避免静默错误。
const SAVE_VERSION := 1


## 保存 JSON Dictionary 到文件（自动附加 schema 版本号）
static func save_json(file_name: String, data: Dictionary) -> bool:
	var path := SAVE_DIR + file_name
	var file := FileAccess.open(path, FileAccess.WRITE)
	if not file:
		push_error("SaveManager: 无法写入文件 " + path)
		return false
	var wrapped := {"version": SAVE_VERSION, "data": data}
	file.store_string(JSON.stringify(wrapped))
	file.close()
	return true


## 从文件读取 JSON Dictionary，文件不存在或解析/版本不匹配返回空 {}
static func load_json(file_name: String) -> Dictionary:
	var path := SAVE_DIR + file_name
	if not FileAccess.file_exists(path):
		return {}

	var file := FileAccess.open(path, FileAccess.READ)
	if not file:
		push_error("SaveManager: 无法读取文件 " + path)
		return {}

	var text := file.get_as_text()
	file.close()

	var json := JSON.new()
	var err := json.parse(text)
	if err != OK:
		push_error("SaveManager: JSON 解析失败 " + path)
		return {}

	var raw = json.get_data()
	if raw == null or not raw is Dictionary:
		return {}

	var version: int = raw.get("version", 0)
	if version != SAVE_VERSION:
		push_warning("SaveManager: 存档版本不匹配 (文件=%d, 期望=%d)，忽略旧存档 %s" % [version, SAVE_VERSION, file_name])
		return {}

	var data = raw.get("data", null)
	if data == null or not data is Dictionary:
		return {}

	return data


## 检查存档文件是否存在
static func file_exists(file_name: String) -> bool:
	return FileAccess.file_exists(SAVE_DIR + file_name)


## 删除存档文件
static func delete_file(file_name: String) -> bool:
	var path := SAVE_DIR + file_name
	if not FileAccess.file_exists(path):
		return false
	var dir := DirAccess.open(SAVE_DIR)
	if not dir:
		return false
	dir.remove(file_name)
	return true