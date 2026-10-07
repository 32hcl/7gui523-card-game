#include "ui/audio/audio_manager.h"

#include <QCoreApplication>
#include <QFile>
#include <QDir>
#include <QUrl>
#include <QDebug>

AudioManager& AudioManager::instance()
{
    static AudioManager s_instance;
    return s_instance;
}

AudioManager::AudioManager(QObject* parent)
    : QObject(parent)
{
}

AudioManager::~AudioManager()
{
    // 清理SFX播放器
    for (auto* p : m_sfxPlayers) {
        p->stop();
        p->deleteLater();
    }
    for (auto* o : m_sfxOutputs) {
        o->deleteLater();
    }
    m_sfxPlayers.clear();
    m_sfxOutputs.clear();

    if (m_bgmPlayer) {
        m_bgmPlayer->stop();
        m_bgmPlayer->deleteLater();
    }
    if (m_bgmOutput) {
        m_bgmOutput->deleteLater();
    }
    if (m_fadeTimer) {
        m_fadeTimer->stop();
        m_fadeTimer->deleteLater();
    }
}

void AudioManager::init(const QString& audioDir)
{
    if (m_initialized) return;

    if (audioDir.isEmpty()) {
        m_audioDir = QCoreApplication::applicationDirPath() + "/audio/";
    } else {
        m_audioDir = audioDir;
        if (!m_audioDir.endsWith('/') && !m_audioDir.endsWith('\\')) {
            m_audioDir += '/';
        }
    }

    // 确保目录存在
    QDir().mkpath(m_audioDir);

    // BGM播放器
    m_bgmPlayer = new QMediaPlayer(this);
    m_bgmOutput = new QAudioOutput(this);
    m_bgmPlayer->setAudioOutput(m_bgmOutput);
    m_bgmPlayer->setLoops(QMediaPlayer::Infinite);
    m_bgmOutput->setVolume(m_muted ? 0.0f : m_bgmVolume);

    // 淡入淡出定时器
    m_fadeTimer = new QTimer(this);
    m_fadeTimer->setInterval(30);
    connect(m_fadeTimer, &QTimer::timeout, this, [this]() {
        float current = m_bgmOutput->volume();
        if (qAbs(current - m_fadeTarget) < 0.01f) {
            m_bgmOutput->setVolume(m_fadeTarget);
            m_fadeTimer->stop();
            if (m_fadeTarget <= 0.001f && m_bgmPlayer->playbackState() == QMediaPlayer::PlayingState) {
                m_bgmPlayer->stop();
            }
            return;
        }
        m_bgmOutput->setVolume(current + m_fadeStep);
    });

    m_initialized = true;
    qDebug() << "[AudioManager] initialized, audioDir =" << m_audioDir;
}

QString AudioManager::bgmFileName(BgmTrack track) const
{
    switch (track) {
    case BgmTrack::MainMenu: return "bgm_menu.wav";
    case BgmTrack::Battle:   return "bgm_battle.wav";
    case BgmTrack::Victory:  return "bgm_victory.wav";
    case BgmTrack::Defeat:   return "bgm_defeat.wav";
    default: return QString();
    }
}

QString AudioManager::sfxFileName(Sfx sfx) const
{
    switch (sfx) {
    case Sfx::Click:        return "sfx_click.wav";
    case Sfx::PlayCard:     return "sfx_playcard.wav";
    case Sfx::Pass:         return "sfx_pass.wav";
    case Sfx::Bomb:         return "sfx_bomb.wav";
    case Sfx::Rocket:       return "sfx_rocket.wav";
    case Sfx::Special523:   return "sfx_special523.wav";
    case Sfx::Win:          return "sfx_win.wav";
    case Sfx::Lose:         return "sfx_lose.wav";
    case Sfx::ChipGain:     return "sfx_chipgain.wav";
    case Sfx::ChipLose:     return "sfx_chiplose.wav";
    case Sfx::CardSelect:   return "sfx_select.wav";
    case Sfx::CardDeselect: return "sfx_deselect.wav";
    case Sfx::HpDamage:     return "sfx_damage.wav";
    case Sfx::HpHeal:       return "sfx_heal.wav";
    default: return QString();
    }
}

void AudioManager::playBgm(BgmTrack track)
{
    if (!m_initialized) return;
    if (track == BgmTrack::None) {
        stopBgm();
        return;
    }

    QString fileName = bgmFileName(track);
    QString fullPath = m_audioDir + fileName;

    if (!QFile::exists(fullPath)) {
        qDebug() << "[AudioManager] BGM not found:" << fullPath;
        return;
    }

    // 如果已经是同一首BGM且正在播放，不重复播放
    if (m_currentBgm == track && m_bgmPlayer->playbackState() == QMediaPlayer::PlayingState) {
        return;
    }

    m_currentBgm = track;
    m_bgmPlayer->setSource(QUrl::fromLocalFile(fullPath));

    // 淡入
    if (!m_muted) {
        m_bgmOutput->setVolume(0.0f);
        m_bgmPlayer->play();
        m_fadeTarget = m_bgmVolume;
        m_fadeStep = m_bgmVolume / 15.0f;  // 约450ms淡入
        m_fadeTimer->start();
    } else {
        m_bgmPlayer->play();
    }

    qDebug() << "[AudioManager] BGM start:" << fileName;
}

void AudioManager::stopBgm(int fadeOutMs)
{
    if (!m_initialized) return;
    if (m_bgmPlayer->playbackState() != QMediaPlayer::PlayingState) return;

    if (fadeOutMs <= 0) {
        m_bgmPlayer->stop();
        m_currentBgm = BgmTrack::None;
        return;
    }

    // 淡出
    m_fadeTarget = 0.0f;
    float currentVol = m_bgmOutput->volume();
    int steps = fadeOutMs / 30;
    m_fadeStep = -currentVol / steps;
    m_fadeTimer->start();
}

void AudioManager::pauseBgm()
{
    if (!m_initialized) return;
    m_bgmPlayer->pause();
}

void AudioManager::resumeBgm()
{
    if (!m_initialized) return;
    if (m_currentBgm == BgmTrack::None) return;
    if (m_muted) return;
    m_bgmPlayer->play();
}

void AudioManager::playSfx(Sfx sfx)
{
    QString name = sfxFileName(sfx);
    if (name.isEmpty()) return;
    playSfxByName(name);
}

void AudioManager::playSfxByName(const QString& name)
{
    if (!m_initialized) return;
    if (m_muted) return;

    ensureSfxPlayer(name);

    QMediaPlayer* player = m_sfxPlayers.value(name, nullptr);
    if (!player) return;
    if (player->source().isEmpty()) return;

    // 重置位置并播放
    player->setPosition(0);
    player->play();
}

void AudioManager::ensureSfxPlayer(const QString& name)
{
    if (m_sfxPlayers.contains(name)) return;

    QString fullPath = m_audioDir + name;
    if (!QFile::exists(fullPath)) {
        return;
    }

    QMediaPlayer* player = new QMediaPlayer(this);
    QAudioOutput* output = new QAudioOutput(this);
    player->setAudioOutput(output);
    output->setVolume(m_sfxVolume);
    player->setSource(QUrl::fromLocalFile(fullPath));

    m_sfxPlayers.insert(name, player);
    m_sfxOutputs.insert(name, output);
}

void AudioManager::setBgmVolume(float volume)
{
    volume = qBound(0.0f, volume, 1.0f);
    if (qFuzzyCompare(m_bgmVolume, volume)) return;

    m_bgmVolume = volume;
    if (m_bgmOutput && !m_muted) {
        m_bgmOutput->setVolume(volume);
    }
    emit bgmVolumeChanged(volume);
}

void AudioManager::setSfxVolume(float volume)
{
    volume = qBound(0.0f, volume, 1.0f);
    if (qFuzzyCompare(m_sfxVolume, volume)) return;

    m_sfxVolume = volume;
    for (auto* output : m_sfxOutputs) {
        output->setVolume(volume);
    }
    emit sfxVolumeChanged(volume);
}

void AudioManager::setMuted(bool muted)
{
    if (m_muted == muted) return;
    m_muted = muted;

    if (m_bgmOutput) {
        m_bgmOutput->setVolume(muted ? 0.0f : m_bgmVolume);
    }
    for (auto* output : m_sfxOutputs) {
        output->setVolume(muted ? 0.0f : m_sfxVolume);
    }

    emit mutedChanged(muted);
}
