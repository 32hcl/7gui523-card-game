#pragma once
#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QWidget>
#include <QTextEdit>
#include <QMessageBox>
#include <QTimer>
#include <QStackedWidget>
#include <QMediaPlayer>
#include <QAudioOutput>
#include "core/deck.h"
#include "core/player.h"
#include "core/cardtype.h"
#include "core/score.h"
#include "core/game.h"
#include "core/ai.h"
#include "core/special.h"
#include "core/cardtracker.h"

class CardWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

protected:
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void onPlayButtonClicked();
    void onPassButtonClicked();
    void onNewGameButtonClicked();
    void onDifficultyButtonClicked();
    void onPickButtonClicked();

private:
    void startNewGame();
    void updateUI();
    QWidget* createCardBack();
    void doAITurn();
    void endRound(Player& winner);
    void refillBoth(Player& winner, Player& loser);
    bool checkGameEnd(Player& finisher, Player& opponent);
    void showGameOverDialog(const QString& message);
    void disableActionButtons();
    void enableActionButtons();
    void appendLog(const QString& text);
    void initSounds();
    void playSound(QMediaPlayer* player);
    void flyCardsToTable(const std::vector<CardWidget*>& cards);
    void layoutTableCards();
    void shakeWidget(QWidget* widget);

    Deck   m_deck;
    Player m_playerA;
    Player m_playerB;
    int    m_roundCount = 0;
    CardTypeResult m_lastPlay;
    std::vector<Card> m_tableCards;
    std::string m_lastPlayerName;
    bool m_gameOver = false;
    bool m_waitingForAI = false;
    bool m_isPicking = false;
    bool m_shaking = false;

    AILevel m_aiLevel = AILevel::AI1_Simple;
    CardTracker m_tracker;
    int    m_tableBonus = 0;

    QLabel* m_deckCountLabel;
    QLabel* m_roundLabel;
    QWidget*    m_playerBHandWidget;
    QHBoxLayout* m_playerBLayout;
    QLabel* m_handTypeLabel;
    QWidget*     m_tableCardsWidget;
    std::vector<CardWidget*> m_tableCardWidgets;
    QLabel* m_tableScoreLabel;
    QWidget*    m_playerAHandWidget;
    QHBoxLayout* m_playerALayout;
    QLabel*      m_scoreALabel;
    QLabel*      m_scoreBLabel;
    QPushButton* m_playButton;
    QPushButton* m_passButton;
    QPushButton* m_pickButton;
    QPushButton* m_newGameButton;
    QPushButton* m_difficultyButton;
    QFrame* m_tableFrame = nullptr;
    QWidget*     m_deckDisplayWidget = nullptr;
    QVBoxLayout* m_deckDisplayLayout = nullptr;
    QLabel*      m_deckBackLabel = nullptr;
    QLabel*      m_deckCountBigLabel = nullptr;
    QLabel*      m_deckDisplayCountLabel = nullptr;
    std::vector<CardWidget*> m_playerACardWidgets;
    QTextEdit* m_logTextEdit;
    QStackedWidget* m_buttonStack = nullptr;

    QMediaPlayer* m_soundSuccess = nullptr;
    QMediaPlayer* m_soundFailure = nullptr;
    QMediaPlayer* m_soundCorrect = nullptr;
    QMediaPlayer* m_soundWrong   = nullptr;
    QMediaPlayer* m_soundClick   = nullptr;
    QMediaPlayer* m_soundCasino  = nullptr;
    QMediaPlayer* m_soundShine   = nullptr;

    QAudioOutput* m_audioSuccess = nullptr;
    QAudioOutput* m_audioFailure = nullptr;
    QAudioOutput* m_audioCorrect = nullptr;
    QAudioOutput* m_audioWrong   = nullptr;
    QAudioOutput* m_audioClick   = nullptr;
    QAudioOutput* m_audioCasino  = nullptr;
    QAudioOutput* m_audioShine   = nullptr;
};