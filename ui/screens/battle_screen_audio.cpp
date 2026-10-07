#include "battle_screen.h"
#include "ui/audio/audio_manager.h"
#include "ui/audio/audio_generator.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QDebug>

// BattleScreen 的音频相关辅助
// 所有音效统一通过 AudioManager 单例播放
// 此文件保留 AudioManager 的后备初始化
