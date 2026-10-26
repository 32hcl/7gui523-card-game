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


## 保存 JSON Dictionary 到文件
static func save_json(file_name: String, data: Dictionary) -> bool:
	var path := SAVE_DIR + file_name
	var file := FileAccess.open(path, FileAccess.WRITE)
	if not file:
		push_error("SaveManager: 无法写入文件 " + path)
		return false
	file.store_string(JSON.stringify(data))
	file.close()
	return true


## 从文件读取 JSON Dictionary，文件不存在或解析失败返回空 {}
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

	var data = json.get_data()
	if data == null:
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