#pragma once
#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QTextEdit>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QStackedWidget>
#include <QPointer>
#include <QPropertyAnimation>
#include <QFrame>
#include <QMainWindow>
#include <tuple>

class QParallelAnimationGroup;

#include "core/card/card.h"
#include "core/player.h"
#include "core/card/deck.h"
#include "ai/engine/ai_engine.h"
#include "ai/engine/ai_levels.h"
#include "core/tracker/cardtracker.h"
#include "game/campaign.h"
#include "ui/cheats/level_cheats.h"

class CardWidget;
class QMediaPlayer;
class QAudioOutput;

class BattleScreen : public QWidget {
    Q_OBJECT
public:
    explicit BattleScreen(QWidget* parent = nullptr);

    void setLevelMode(bool isLevelMode, int startLevel);
    void startNewGame();
    void runAutoPlayTest(int totalGames);
    int  getStressTestDoAICallCount() const { return m_stressDoAICallCount; }

    // Cheat tools (called by cheat table lambdas)
    void cheat_loadVariantDeck();
    void cheat_forceFirstHand(bool playerAIsFirst);
    void cheat_setHandLimit(int limit);
    void cheat_banCard(const std::string& point);

signals:
    void gameEnded();
    void dealAnimationFinished();
    void gameStarted();
    void autoPlayTestFinished();

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
    enum class GamePhase {
        DealAnimation,
        PlayerTurn,
        AITurn,
        RefillAnimation,
        GameOver
    };
    GamePhase m_phase = GamePhase::DealAnimation;

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
    void updateUI(bool rebuildHand = true, bool hideNewWidgets = false);
    void updateLabels();
    void updateTableHint();
    void rebuildPlayerHands(bool hideNewWidgets = false);
    void updateButtonStates();
    void enableActionButtons();
    void disableActionButtons();
    void startNextTurn();
    void playDealAnimation();
    void refillBoth(Player& winner, Player& loser);
    void endRound(Player& finisher);

    // Round management helpers
    void clearLastPlay();
    bool handleSpecialVictoryCheck();
    void finishGame(Player& finisher, const QString& finisherName, QMediaPlayer* sound);

    // AI
    void doAITurn();
    std::vector<Card> dispatchAI(Player& self, Player& opp, Deck& selfDeck);

    // Animation methods
    void flyCardsToTable(const std::vector<CardWidget*>& cards);
    void flyAICardsToTable(const std::vector<Card>& cards);
    QParallelAnimationGroup* animateNewCardsToHand(const std::vector<QWidget*>& cards, QWidget* deckSource, QWidget* handWidget, bool playerA);
    void shakeWidget(QWidget* widget);
    void showSpecialVictoryEffect(const QString& winnerName, const QString& endMessage);
    void showBonusFloat(int bonus);
    void showHpDamageFloat(int damage, bool toBoss);

    // Audio
    void playSound(QMediaPlayer* player);

    // UI helpers
    QWidget* createCardBack();
    void layoutTableCards();
    QPoint handSlotPos(QWidget* handWidget, int index) const;
    void layoutHandSlots(bool playerA, bool animate);
    void resizeEvent(QResizeEvent* event) override;

    // Level mode methods
    void handleLevelModeEnd(const LevelResult& lr);
    void executeCheats(CheatWhen when);

    // Stress test
    void autoPlayOneGame();

    QString getLevelDisplayName(int level) const;
    void returnToMenu();

    // UI controls
    QLabel*   m_titleLabel        = nullptr;
    QLabel*   m_deckCountLabel    = nullptr;
    QLabel*   m_roundLabel        = nullptr;
    QLabel*   m_tableScoreLabel   = nullptr;
    QLabel*   m_handTypeLabel     = nullptr;
    QLabel*   m_scoreALabel       = nullptr;
    QLabel*   m_scoreBLabel       = nullptr;
    QLabel*   m_hpLabel           = nullptr;
    QLabel*   m_playerDeckCountLabel = nullptr;
    QLabel*   m_bossDeckCountLabel   = nullptr;
    QWidget*  m_playerDeckWidget  = nullptr;
    QWidget*  m_bossDeckWidget    = nullptr;
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
    QPointer<QLabel>  m_tableHintLabel;
    QFrame*  m_tableFrame         = nullptr;
    QWidget* m_playerAHandWidget  = nullptr;
    QWidget* m_playerBHandWidget  = nullptr;

    // Game state
    Player m_playerA;
    Player m_playerB;
    Deck   m_playerDeck;
    Deck   m_bossDeck;
    Deck   m_playerHiddenDeck;
    int    m_playerNextSeq = 0;
    std::vector<Card> m_playerPlayedCards;
    CardTracker m_tracker;
    CardTypeResult m_lastPlay;
    Campaign m_campaign;
    std::vector<Card> m_tableCards;
    std::vector<Card> m_pickedCards;
    int    m_tableBonus          = 0;
    int    m_roundCount          = 1;
    bool   m_playerAIsFirst      = true;
    bool   m_handDirty           = true;
    bool   m_pendingPick         = true;
    bool   m_waitingForFirstChoice = false;
    bool   m_isLevelMode         = false;
    int    m_currentLevel        = 1;
    bool   m_playerLastRound     = false;
    bool   m_opponentLastRound   = false;
    bool   m_pendingLevelRestart = false;
    bool   m_shaking             = false;
    bool   m_pendingSpecialVictory = false;
    QString m_lastPlayerName;

    // Stress test
    int  m_stressGamesLeft = 0;
    int  m_stressTotalGames = 0;
    int  m_stressSuccessGames = 0;
    int  m_stressFailedGames = 0;
    int  m_stressDoAICallCount = 0;
    int  m_stressDoAICallLimit = 200;  // unused, kept for reference
    int  m_stressNoProgressCount = 0;
    int  m_stressNoProgressLimit = 20;
    std::tuple<int,int,int,int> m_lastTurnSignature = {-1,-1,-1,-1};
    int  m_dealAnimFinishCount = 0;
    bool m_stressPlaying = false;
    bool m_isStressPlayerATurn = false;

    static constexpr int kAITurnDelayNormalMs = 500;
    static constexpr int kAITurnDelayStressMs = 0;
    static constexpr int kDealWaitStressMs    = 1500;
    static constexpr int kRefillCheckStressMs = 800;
    int m_aiTurnDelayMs = kAITurnDelayNormalMs;

    std::vector<CardWidget*> m_playerACardWidgets;
    std::vector<QWidget*> m_playerBCardWidgets;
    std::vector<CardWidget*> m_tableCardWidgets;

    AILevel m_aiLevel = AILevel::AI1_Simple;
    std::unique_ptr<class AIEngine> m_levelAIEngine;
    AIEngineConfig m_levelEngineConfig;

    // Audio members
    QAudioOutput* m_audioOutput   = nullptr;  // unused, kept for reference
    QAudioOutput* m_audioSuccess  = nullptr;
    QAudioOutput* m_audioFailure  = nullptr;
    QAudioOutput* m_audioCorrect  = nullptr;
    QAudioOutput* m_audioWrong    = nullptr;
    QAudioOutput* m_audioClick    = nullptr;
    QAudioOutput* m_audioCasino   = nullptr;
    QAudioOutput* m_audioShine    = nullptr;
    QMediaPlayer* m_soundClick    = nullptr;
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