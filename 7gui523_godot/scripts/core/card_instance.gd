class_name CardInstance
extends RefCounted

## 运行时的一张牌。
## 持有 def_id 引用不可变的 CardDef，所有临时修改（改点/改色/改分）只落在本实例。

## 静态定义 id（-1 = 非标准牌，无 CardDef）
var def_id: int = -1

## 副本标记：0=牌库初始牌，1+=商店等途径产生的额外复制品，不可变
var copy_mark: int = 0

## 附魔列表（暂缓使用，目前仅占位保证序列化不报错）
var enchantments: Array = []

var _def: CardDef = null
var _temp_point: String = ""
var _temp_suit: String = ""
var _temp_score: int = 0
var _score_dirty: bool = false


## 取静态定义；非标准牌返回 null
func get_def() -> CardDef:
	if _def == null and def_id >= 0:
		_def = CardDef.from_id(def_id)
	return _def


## 不可变身份锚（只读，写入请用 def_id）
var id: int:
	get:
		return def_id


## 点数：优先临时修改，否则取 CardDef
var point: String:
	set(v):
		_temp_point = v
	get:
		if _temp_point != "":
			return _temp_point
		var d := get_def()
		return d.point if d != null else ""


## 花色：优先临时修改，否则取 CardDef
var suit: String:
	set(v):
		_temp_suit = v
	get:
		if _temp_suit != "":
			return _temp_suit
		var d := get_def()
		return d.suit if d != null else ""


## 分值：优先临时修改，否则按当前点数重算
var score: int:
	set(v):
		_temp_score = v
		_score_dirty = true
	get:
		if _score_dirty:
			return _temp_score
		return CardDef.score_from_point(point)


## 压牌筹码：同牌型 + 同点数压牌时，每张牌额外加到桌面的筹码。
## 只读，完全由当前点数决定（大小鬼统一按 "鬼" 取值）。
var pressure_chips: int:
	get:
		return CardDef.pressure_chips_from_point(point)


## 清除所有临时修改，回到 CardDef 的定义
func reset_to_def() -> void:
	_temp_point = ""
	_temp_suit = ""
	_temp_score = 0
	_score_dirty = false


## 判断是否为鬼牌
func is_joker() -> bool:
	return point == "大鬼" or point == "小鬼"


func _to_string() -> String:
	if suit.is_empty():
		return point
	return suit + point