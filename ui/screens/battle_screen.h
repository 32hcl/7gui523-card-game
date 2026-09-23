#pragma once
#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QTextEdit>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QStackedWidget>
#include <QPropertyAnimation>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QFrame>
#include <QMainWindow>

#include "core/card/card.h"
#include "core/player.h"
#include "core/card/deck.h"
#include "ai/engine/ai_engine.h"
#include "ai/engine/ai_levels.h"
#include "core/tracker/cardtracker.h"

class CardWidget;

class BattleScreen : public QWidget {
    Q_OBJECT
public:
    explicit BattleScreen(QWidget* parent = nullptr);

    void setLevelMode(bool isLevelMode, int startLevel);
    void startNewGame();

signals:
    void gameEnded();
    void dealAnimationFinished();
    void gameStarted();

private slots:
    void onPlayButtonClicked();
    void onPassButtonClicked();
    void onPickButtonClicked();
    void onDifficultyButtonClicked();
    void onNewGameButtonClicked();
    void onReturnMenuClicked();
    void onFirstBtnClicked();
    void onSecondBtnClicked();
    void onRandomBtnClicked();

private:
    void createTopBar(QVBoxLayout* leftLay);
    void createOpponentArea(QVBoxLayout* leftLay);
    void createTableArea(QVBoxLayout* leftLay);
    void createPlayerArea(QVBoxLayout* leftLay);
    void createRightPanel(QHBoxLayout* rootLayout, QWidget* bottomBar);
    QWidget* createBottomBar();
    void connectSignals();
    void initSounds();
    void initFirstChoiceWidget();
    void showGameOverDialog(const QString& message);
    bool checkGameEnd(Player& finisher, Player& opponent);
    void startGameWithFirst(bool playerAFirst);
    void onDealAnimationFinished();
    void onPlayerTurnTimeout();
    void appendLog(const QString& text);
    void updateUI(bool rebuildHand = true);
    void enableActionButtons();
    void disableActionButtons();
    void playDealAnimation();
    void refillBoth(Player& winner, Player& loser);
    void endRound(Player& finisher);

    // AI / Level mode
    void doAITurn();

    // Animation methods
    void flyCardsToTable(const std::vector<CardWidget*>& cards);
    void flyAICardsToTable(const std::vector<Card>& cards);
    void shakeWidget(QWidget* widget);
    void showSpecialVictoryEffect(const QString& winnerName, const QString& endMessage);
    void showBonusFloat(int bonus);

    // Audio
    void playSound(QMediaPlayer* player);

    // UI helpers
    QWidget* createCardBack();
    void layoutTableCards();
    void resizeEvent(QResizeEvent* event) override;

    // Level mode methods
    void handleLevelModeEnd(bool playerWon);
    int getLevelAILevel(int level) const;
    QString getLevelDisplayName(int level) const;
    void returnToMenu();

    struct PlayerData {
        Player player;
        std::vector<CardWidget*> cardWidgets;
        std::vector<CardWidget*> tableWidgets;
    };

    // UI controls
    QLabel*   m_deckCountLabel    = nullptr;
    QLabel*   m_roundLabel        = nullptr;
    QLabel*   m_tableScoreLabel   = nullptr;
    QLabel*   m_handTypeLabel     = nullptr;
    QLabel*   m_scoreALabel       = nullptr;
    QLabel*   m_scoreBLabel       = nullptr;
    QLabel*   m_deckCountBigLabel = nullptr;
    QLabel*   m_deckBackLabel     = nullptr;
    QPushButton* m_playButton     = nullptr;
    QPushButton* m_passButton     = nullptr;
    QPushButton* m_pickButton     = nullptr;
    QPushButton* m_difficultyButton = nullptr;
    QPushButton* m_newGameButton  = nullptr;
    QPushButton* m_returnMenuButton = nullptr;
    QTextEdit* m_logTextEdit      = nullptr;
    QStackedWidget* m_buttonStack = nullptr;
    QWidget* m_firstChoiceWidget  = nullptr;
    QWidget* m_tableCardsWidget   = nullptr;
    QFrame*  m_tableFrame         = nullptr;
    QWidget* m_deckDisplayWidget  = nullptr;
    QVBoxLayout* m_deckDisplayLayout = nullptr;
    QWidget* m_playerAHandWidget  = nullptr;
    QHBoxLayout* m_playerALayout  = nullptr;
    QWidget* m_playerBHandWidget  = nullptr;
    QHBoxLayout* m_playerBLayout  = nullptr;

    // Game state
    Player m_playerA;
    Player m_playerB;
    Deck   m_deck;
    CardTracker m_tracker;
    CardTypeResult m_lastPlay;
    std::vector<Card> m_tableCards;
    std::vector<Card> m_pickedCards;
    int    m_tableBonus          = 0;
    int    m_roundCount          = 1;
    bool   m_playerAIsFirst      = true;
    bool   m_gameOver            = false;
    bool   m_waitingForAI        = false;
    bool   m_dealAnimating       = false;
    bool   m_pendingPick         = true;
    bool   m_waitingForFirstChoice = false;
    bool   m_isLevelMode         = false;
    int    m_currentLevel        = 1;
    bool   m_playerLastRound     = false;
    bool   m_opponentLastRound   = false;
    bool   m_pendingLevelRestart = false;
    bool   m_shaking             = false;
    QString m_lastPlayerName;

    std::vector<CardWidget*> m_playerACardWidgets;
    std::vector<CardWidget*> m_playerBCardWidgets;
    std::vector<CardWidget*> m_tableCardWidgets;

    AILevel m_aiLevel = AILevel::AI1_Simple;
    std::unique_ptr<class AIEngine> m_levelAIEngine;
    AIEngineConfig m_levelEngineConfig;

    // Audio members
    QAudioOutput* m_audioOutput   = nullptr;
    QAudioOutput* m_audioSuccess  = nullptr;
    QAudioOutput* m_audioFailure  = nullptr;
    QAudioOutput* m_audioCorrect  = nullptr;
    QAudioOutput* m_audioWrong    = nullptr;
    QAudioOutput* m_audioClick    = nullptr;
    QAudioOutput* m_audioCasino   = nullptr;
    QAudioOutput* m_audioShine    = nullptr;
    QMediaPlayer* m_soundPlay     = nullptr;
    QMediaPlayer* m_soundPass     = nullptr;
    QMediaPlayer* m_soundPick     = nullptr;
    QMediaPlayer* m_soundClick    = nullptr;
    QMediaPlayer* m_soundDeal     = nullptr;
    QMediaPlayer* m_soundWrong    = nullptr;
    QMediaPlayer* m_soundSuccess  = nullptr;
    QMediaPlayer* m_soundFailure  = nullptr;
    QMediaPlayer* m_soundCorrect  = nullptr;
    QMediaPlayer* m_soundCasino   = nullptr;
    QMediaPlayer* m_soundShine    = nullptr;
};

// Helper functions shared across cpp files
QString cardsToString(const std::vector<Card>& cards);
QString cardTypeToQString(CardType type);
void sortHandSmart(std::vector<Card>& hand);
void applyBtnShadow(QPushButton* btn);