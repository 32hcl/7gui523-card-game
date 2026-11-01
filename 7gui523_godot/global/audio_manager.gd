class_name AudioManager
extends Node

# ═══════════════════════════════════════════════════════════════
# AudioManager — 最小音频实现（纯代码合成，零外部资源）
#
# 声音生成：AudioStreamGenerator 合成正弦波 → 出牌/胜利/伤害
# ═══════════════════════════════════════════════════════════════


const SAMPLE_RATE := 44100

var _sfx_player: AudioStreamPlayer = null
var _bgm_player: AudioStreamPlayer = null
var _parent: Node = null

var master_volume: float = 1.0:
	set(v):
		master_volume = clampf(v, 0.0, 1.0)
		_apply_volume()

var bgm_volume: float = 0.8:
	set(v):
		bgm_volume = clampf(v, 0.0, 1.0)
		_apply_volume()

var sfx_volume: float = 1.0:
	set(v):
		sfx_volume = clampf(v, 0.0, 1.0)
		_apply_volume()

var muted: bool = false:
	set(v):
		muted = v
		_apply_volume()


# ═══════════════════════════════════════════════════════════════
# 初始化（Autoload AudioSrv 进入场景树时自动调用；也可手动调用 setup）
# ═══════════════════════════════════════════════════════════════

## 作为 Autoload（AudioSrv）进入场景树时自动完成初始化
func _ready() -> void:
	setup(self)


## 初始化播放器；重复调用无副作用
func setup(parent: Node) -> void:
	if _sfx_player != null:
		return
	_parent = parent
	_sfx_player = AudioStreamPlayer.new()
	_sfx_player.bus = "SFX"
	parent.add_child(_sfx_player)
	_bgm_player = AudioStreamPlayer.new()
	_bgm_player.bus = "Music"
	parent.add_child(_bgm_player)


# ═══════════════════════════════════════════════════════════════
# 公开接口
# ═══════════════════════════════════════════════════════════════

func play_sfx(sound_name: String) -> void:
	if muted or not _sfx_player:
		return
	match sound_name:
		"card_play":   _synth_beep(880.0, 0.04, 0.25)
		"round_win":   _synth_ascend(440.0, 880.0, 0.15, 0.20)
		"damage":      _synth_beep(180.0, 0.10, 0.30)
		"card_pass":   _synth_beep(440.0, 0.03, 0.15)
		"game_over":   _synth_ascend(330.0, 660.0, 0.30, 0.25)
		_:             _synth_beep(660.0, 0.05, 0.20)


func play_bgm(resource_name: String) -> void:
	if muted or not _bgm_player:
		return
	_synth_ambient(220.0, 2.0, 0.08)


func stop_bgm() -> void:
	if _bgm_player and _bgm_player.playing:
		_bgm_player.stop()


# ═══════════════════════════════════════════════════════════════
# 内部合成器
# ═══════════════════════════════════════════════════════════════

func _synth_beep(freq: float, duration: float, amp: float) -> void:
	var gen := _make_generator(duration)
	_sfx_player.stop()
	_sfx_player.stream = gen
	_sfx_player.play()
	var pb := _sfx_player.get_stream_playback() as AudioStreamGeneratorPlayback
	var frames := int(SAMPLE_RATE * duration)
	for i in frames:
		var t := float(i) / SAMPLE_RATE
		var envelope := (1.0 - t / duration) * amp
		var sample := sin(TAU * freq * t) * envelope
		pb.push_frame(Vector2(sample, sample))


func _synth_ascend(freq_start: float, freq_end: float, duration: float, amp: float) -> void:
	var gen := _make_generator(duration)
	_sfx_player.stop()
	_sfx_player.stream = gen
	_sfx_player.play()
	var pb := _sfx_player.get_stream_playback() as AudioStreamGeneratorPlayback
	var frames := int(SAMPLE_RATE * duration)
	for i in frames:
		var t := float(i) / SAMPLE_RATE
		var envelope := amp * (1.0 - t / duration)
		var freq := lerpf(freq_start, freq_end, t / duration)
		var sample := sin(TAU * freq * t) * envelope
		pb.push_frame(Vector2(sample, sample))


func _synth_ambient(freq: float, duration: float, amp: float) -> void:
	var gen := _make_generator(duration)
	_bgm_player.stop()
	_bgm_player.stream = gen
	_bgm_player.play()
	var pb := _bgm_player.get_stream_playback() as AudioStreamGeneratorPlayback
	var frames := int(SAMPLE_RATE * duration)
	for i in frames:
		var t := float(i) / SAMPLE_RATE
		var mod := sin(TAU * 0.5 * t) * 0.5 + 0.5
		var sample := sin(TAU * freq * t) * mod * amp
		pb.push_frame(Vector2(sample, sample))


func _make_generator(duration: float) -> AudioStreamGenerator:
	var gen := AudioStreamGenerator.new()
	gen.mix_rate = SAMPLE_RATE
	gen.buffer_length = maxf(duration, 0.01)
	return gen


# ═══════════════════════════════════════════════════════════════
# 内部
# ═══════════════════════════════════════════════════════════════

func _apply_volume() -> void:
	var db := linear_to_db(master_volume)
	if _sfx_player:
		_sfx_player.volume_db = db if not muted else -80.0
	if _bgm_player:
		_bgm_player.volume_db = db if not muted else -80.0