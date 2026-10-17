extends Control

## 画廊场景：一次性展示所有可用卡牌视觉资源，方便肉眼验收素材

@onready var _grid: GridContainer = $Scroll/VBox/CardGrid
@onready var _aux: GridContainer = $Scroll/VBox/AuxGrid

## 非牌面附属资源（各加独立标签栏）
const AUX_GROUPS := [
	{
		"label": "牌背 / 牌堆",
		"paths": [
			"res://art/Top-Down/Cards/Card_Back-88x124.png",
			"res://art/Top-Down/Cards/Card_DeckA-88x140.png",
			"res://art/Top-Down/Cards/Card_DeckB-124x104.png",
			"res://art/Top-Down/Cards/CardsBack/0.png",
			"res://art/Top-Down/Cards/CardsBack/1.png",
		],
	},
	{
		"label": "筹码",
		"paths": [
			"res://art/Top-Down/Chips/ChipsA_Flat-64x72.png",
			"res://art/Top-Down/Chips/ChipsA_Outline-64x72.png",
			"res://art/Top-Down/Chips/ChipsB_Flat-64x72.png",
			"res://art/Top-Down/Chips/ChipsB_Outline-64x72.png",
		],
	},
]


func _ready() -> void:
	_build()


# ---------- 构建 ----------

func _build() -> void:
	# 1) 全部 54 张牌面（每花色 13 张，严格13列，等宽等距）
	for id in 54:
		var item := _make_card_item(id, CardVisual.texture_for_id(id), "%s  %d" % [_rank_name(id), id])
		_grid.add_child(item)

	# 2) 附属资源放独立 AuxGrid（6列），不干扰卡牌对齐
	for group in AUX_GROUPS:
		_aux.add_child(_make_section_header(group["label"]))
		for path: String in group["paths"]:
			var tex: Texture2D = load(path)
			var fname := path.get_file()
			_aux.add_child(_make_tex_item(tex, fname))


# ---------- 组装 ----------

func _make_card_item(id: int, tex: Texture2D, label: String) -> Control:
	var tex_item := _make_tex_item(tex, label)
	# 保留 card_id 以备后续调试点击
	tex_item.set_meta("card_id", id)
	return tex_item


func _make_tex_item(tex: Texture2D, label: String) -> Control:
	var box := VBoxContainer.new()
	box.custom_minimum_size = Vector2(88, 124 + 20)  # 卡高 + 标签高，等宽等距
	box.add_theme_constant_override("separation", 2)

	var lbl := Label.new()
	lbl.text = label
	lbl.custom_minimum_size = Vector2(88, 20)  # 固定高度避免换行错位
	lbl.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	lbl.vertical_alignment = VERTICAL_ALIGNMENT_TOP
	box.add_child(lbl)

	var visual := CardVisual.new(Vector2(88, 124))
	visual.size_flags_horizontal = Control.SIZE_SHRINK_BEGIN
	if tex:
		visual.show_texture(tex)
	else:
		visual.show_back()
	box.add_child(visual)

	return box


func _make_section_header(text: String) -> Control:
	var header := Label.new()
	header.text = "—— %s ——" % text
	header.size_flags_horizontal = Control.SIZE_SHRINK_BEGIN
	header.add_theme_font_size_override("font_size", 18)
	# 占整行：给 GridContainer 一条特殊 cell，宽度撑满
	header.set_meta("full_row", true)
	return header


## 用 Card 的牌点名显示（黑桃/红桃…A~K）
func _rank_name(id: int) -> String:
	var c := Card.from_id(id)
	if not c:
		return "?"
	return c._to_string()