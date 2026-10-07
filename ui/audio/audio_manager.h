#ifndef AUDIO_MANAGER_H
#define AUDIO_MANAGER_H

#include <QObject>
#include <QString>
#include <QHash>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QTimer>

/**
 * @brief 音效管理器
 *
 * 统一管理游戏所有音效和背景音乐：
 * - BGM: 背景音乐（循环播放，淡入淡出切换）
 * - SFX: 短音效（点击/出牌/炸弹等，支持并发）
 * - 音量控制：独立的BGM音量和SFX音量
 * - 资源路径：从应用目录/audio/ 加载
 *
 * 单例模式，全局共享。
 */
class AudioManager : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief BGM曲目
     */
    enum class BgmTrack {
        None,
        MainMenu,    // 主菜单BGM
        Battle,      // 战斗BGM
        Victory,     // 胜利BGM
        Defeat,      // 失败BGM
    };

    /**
     * @brief SFX音效
     */
    enum class Sfx {
        Click,       // 按钮点击
        PlayCard,    // 出牌
        Pass,        // Pass/不要
        Bomb,        // 炸弹
        Rocket,      // 王炸
        Special523,  // 七鬼523
        Win,         // 回合胜利
        Lose,        // 回合失败
        ChipGain,    // 筹码增加
        ChipLose,    // 筹码减少
        CardSelect,  // 卡牌选中
        CardDeselect,// 卡牌取消选中
        HpDamage,    // HP扣减
        HpHeal,      // HP恢复
    };

    static AudioManager& instance();

    // ── 初始化 ──
    /**
     * @brief 初始化音频系统，加载所有音效
     * @param audioDir 音频文件目录（默认: 应用目录/audio/）
     */
    void init(const QString& audioDir = QString());

    bool isInitialized() const { return m_initialized; }

    // ── BGM 控制 ──
    void playBgm(BgmTrack track);
    void stopBgm(int fadeOutMs = 500);
    void pauseBgm();
    void resumeBgm();

    // ── SFX 控制 ──
    void playSfx(Sfx sfx);
    void playSfxByName(const QString& name);

    // ── 音量控制 ──
    float bgmVolume() const { return m_bgmVolume; }
    void setBgmVolume(float volume);  // 0.0 ~ 1.0

    float sfxVolume() const { return m_sfxVolume; }
    void setSfxVolume(float volume);  // 0.0 ~ 1.0

    void setMuted(bool muted);
    bool isMuted() const { return m_muted; }

    // ── 资源路径 ──
    QString audioDir() const { return m_audioDir; }

signals:
    void bgmVolumeChanged(float volume);
    void sfxVolumeChanged(float volume);
    void mutedChanged(bool muted);

private:
    explicit AudioManager(QObject* parent = nullptr);
    ~AudioManager();
    Q_DISABLE_COPY(AudioManager)

    void ensureSfxPlayer(const QString& name);
    QString bgmFileName(BgmTrack track) const;
    QString sfxFileName(Sfx sfx) const;

    bool m_initialized = false;
    QString m_audioDir;

    // BGM
    QMediaPlayer*   m_bgmPlayer   = nullptr;
    QAudioOutput*   m_bgmOutput   = nullptr;
    BgmTrack        m_currentBgm  = BgmTrack::None;
    float           m_bgmVolume   = 0.4f;
    QTimer*         m_fadeTimer   = nullptr;
    float           m_fadeTarget  = 0.0f;
    float           m_fadeStep    = 0.0f;

    // SFX
    QHash<QString, QMediaPlayer*> m_sfxPlayers;
    QHash<QString, QAudioOutput*> m_sfxOutputs;
    float m_sfxVolume = 0.7f;

    // 全局静音
    bool m_muted = false;
};

// 便捷宏
#define AUDIO_MGR AudioManager::instance()

#endif // AUDIO_MANAGER_H
