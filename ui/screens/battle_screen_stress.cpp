#include "battle_screen.h"
#include <QDebug>
#include <QTimer>

void BattleScreen::runAutoPlayTest(int totalGames)
{
    m_stressTotalGames = totalGames;
    m_stressGamesLeft = totalGames;
    m_stressSuccessGames = 0;
    m_stressFailedGames = 0;
    m_stressPlaying = true;
    m_aiTurnDelayMs = kAITurnDelayStressMs;

    qDebug() << "[STRESS] ============ START ============";
    qDebug() << "[STRESS] total games=" << totalGames;

    autoPlayOneGame();
}

void BattleScreen::autoPlayOneGame()
{
    if (m_stressGamesLeft <= 0) {
        qDebug() << "[STRESS] ============ DONE ============";
        qDebug() << "[STRESS] total=" << m_stressTotalGames
                 << "success=" << m_stressSuccessGames
                 << "failed=" << m_stressFailedGames;
        m_stressPlaying = false;
        emit autoPlayTestFinished();
        return;
    }

    m_stressGamesLeft--;
    m_stressDoAICallCount   = 0;
    m_stressNoProgressCount = 0;
    m_lastTurnSignature     = {-1,-1,-1,-1};

    int gameNum = m_stressTotalGames - m_stressGamesLeft;

    m_isLevelMode = false;
    m_currentLevel = 0;
    m_playerAIsFirst = false;
    m_isStressPlayerATurn = false;

    startNewGame();

    m_playerA.isHuman = false;
    m_playerA.aiLevel = AILevel::AI1_Simple;
    m_playerB.aiLevel = AILevel::AI1_Simple;

    qDebug() << "[STRESS] game" << gameNum << "dealing...";

    startGameWithFirst(false);
}