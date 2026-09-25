#include "battle_screen.h"
#include <QCoreApplication>
#include <QFile>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QUrl>

void BattleScreen::initSounds()
{
    auto createPlayer = [](const QString& path,
                           QAudioOutput*& out,
                           QMediaPlayer*& player) {
        player = new QMediaPlayer();
        out = new QAudioOutput();
        player->setAudioOutput(out);
        out->setVolume(0.6);
        if (QFile::exists(path)) {
            player->setSource(QUrl::fromLocalFile(path));
        }
    };

    QString base = QCoreApplication::applicationDirPath() + "/music/";
    createPlayer(base + "\u6210\u529f.mp3", m_audioSuccess, m_soundSuccess);
    createPlayer(base + "\u5931\u8d25.mp3", m_audioFailure, m_soundFailure);
    createPlayer(base + "\u6b63\u786e.mp3", m_audioCorrect, m_soundCorrect);
    createPlayer(base + "\u9519\u8bef.mp3", m_audioWrong,   m_soundWrong);
    createPlayer(base + "\u70b9\u51fb.mp3", m_audioClick,   m_soundClick);
    createPlayer(base + "\u8d4c\u573a-\u5f39\u73e0\u673a-\u8001\u864e\u673a.mp3", m_audioCasino, m_soundCasino);
    createPlayer(base + "\u95ea\u4eae.mp3", m_audioShine,   m_soundShine);
}

void BattleScreen::playSound(QMediaPlayer* player)
{
    if (m_stressPlaying) return;
    if (!player) return;
    if (player->source().isEmpty()) return;
    player->stop();
    player->setPosition(0);
    player->play();
}