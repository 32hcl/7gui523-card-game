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
#include "ai/ai_levels.h"
#include "ai/plugins/level_rule.h"
#include "ai/plugins/cardtracker.h"
#include "game/campaign.h"
#include "ui/cheats/level_cheats.h"

class CardWidget;

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
    void onHpSwapButtonClicked();
    void onPickButtonClicked();
    void onDifficultyButtonClicked();
    void onNewGameButtonClicked();
    void onReturnMenuClicked();
    void onShopButtonClicked();
    void onFirstBtnClicked();
    void onSecondBtnClicked();
    void onRandomBtnClicked();
    void onDialogueFinished();  // 对话结束（根据m_dialogueCtx分发）

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
    void initFirstChoiceWidget();
    void initGuessWidget();
    void showGuessWidget();
    void onGuessJoker(bool guessedBig);
    void continueLevelGameAfterGuess();
    void showGameOverDialog(const QString& message);
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
    void autoPlayAllCards();
    void playDealAnimation();
    void refillBoth(Player& winner, Player& loser);
    void endRound(Player& finisher);

    // Round management helpers
    void clearLastPlay();
    void finishGame(Player& finisher, const QString& finisherName);

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

    // Audio（使用全局 AudioManager 单例）

    // UI controlshelpers
    QWidget* createCardBack();
    void layoutTableCards();
    QPoint handSlotPos(QWidget* handWidget, int index, int cardHeight = 180) const;
    void layoutHandSlots(bool playerA, bool animate);
    void resizeEvent(QResizeEvent* event) override;
    bool event(QEvent* event) override;
    void showDebugPanel();

    // Level mode methods
    void handleLevelModeEnd(const LevelResult& lr);
    void showLevelEndDialog();   // 显示关卡结算对话框（对话结束后调用）
    void showShopDialog(bool viewOnly = false);
    void executeCheats(CheatWhen when);

    // Dialogue helpers
    void resetDialogueFlags();

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
    QLabel*   m_chipsLabel        = nullptr;  // 关卡模式：筹码显示
    QLabel*   m_entryChipsLabel   = nullptr;  // 关卡模式：本局入场筹码
    QLabel*   m_playerDeckCountLabel = nullptr;
    QLabel*   m_bossDeckCountLabel   = nullptr;
    QWidget*  m_playerDeckWidget  = nullptr;
    QWidget*  m_bossDeckWidget    = nullptr;
    QPushButton* m_playButton     = nullptr;
    QPushButton* m_passButton     = nullptr;
    QPushButton* m_hpSwapButton  = nullptr;
    QPushButton* m_pickButton     = nullptr;
    QPushButton* m_difficultyButton = nullptr;
    QPushButton* m_newGameButton  = nullptr;
    QPushButton* m_returnMenuButton = nullptr;
    QPushButton* m_shopButton       = nullptr;
    QTextEdit* m_logTextEdit      = nullptr;
    QStackedWidget* m_buttonStack = nullptr;
    QWidget* m_firstChoiceWidget  = nullptr;
    QWidget* m_guessWidget        = nullptr;  // 猜牌覆盖层（关卡模式每关开局）
    QLabel*  m_guessCardLabel     = nullptr;  // 猜牌阶段的牌背/翻牌显示
    QWidget* m_tableCardsWidget   = nullptr;
    QPointer<QLabel>  m_tableHintLabel;
    QPointer<QLabel>  m_tableWatermark;   // 桌面中央水印
    QFrame*  m_tableFrame         = nullptr;
    class PlayEffectOverlay* m_effectOverlay = nullptr;  // 出牌特效层
    QWidget* m_playerAHandWidget  = nullptr;
    QWidget* m_playerBHandWidget  = nullptr;
    QWidget* m_bossAvatarWidget   = nullptr;  // Boss头像+名字容器（关卡模式）
    class BossAvatar* m_bossAvatar = nullptr; // Boss头像组件
    class DialogueOverlay* m_dialogueOverlay = nullptr;  // 对话覆盖层

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
    std::vector<Card> m_shopPurchasedCards;  // 从商店购买的卡牌，下关开局加入牌堆
    std::vector<Card> m_shopHandCards;     // 从商店购买的卡牌，下关开局加入手牌
    int    m_tableBonus          = 0;
    int    m_roundCount          = 1;
    bool   m_playerAIsFirst      = true;
    bool   m_handDirty           = true;
    bool   m_pendingPick         = true;
    bool   m_waitingForFirstChoice = false;
    bool   m_isLevelMode         = false;
    int    m_currentLevel        = 1;
    LevelRule m_levelRule;
    bool   m_playerLastRound     = false;
    bool   m_opponentLastRound   = false;
    bool   m_playerWonLastRound  = false;
    bool   m_pendingLevelRestart = false;
    bool   m_shaking             = false;
    int    m_hpSwapCost          = 15;
    std::vector<QParallelAnimationGroup*> m_activeRefillGroups;
    bool   m_pendingSpecialVictory = false;
    bool   m_playerPlayedSpecial523 = false;
    QString m_lastPlayerName;

    // Dialogue tracking
    bool   m_bossFirstBombShown   = false;  // Boss第一次出炸弹对话是否已播
    bool   m_bossFirstRocketShown = false;  // Boss第一次出王炸对话是否已播
    bool   m_playerFirstBombShown = false;  // 玩家第一次出炸弹对话是否已播
    bool   m_playerFirstRocketShown = false; // 玩家第一次出王炸对话是否已播
    bool   m_special523DialogShown = false;  // Special523对话是否已播

    enum class DialogueCtx {
        None,
        PreBattle,       // 战前 → 猜牌
        PlayerSpecial,   // 玩家出特殊牌 → 进入AI回合
        BossSpecial,     // Boss出特殊牌 → 进入玩家回合
        Victory,         // 胜利对话 → 显示结算
        Defeat,          // 失败对话 → 显示结算
    };
    DialogueCtx m_dialogueCtx = DialogueCtx::None;

    // Stress test
    int  m_stressGamesLeft = 0;
    int  m_stressTotalGames = 0;
    int  m_stressSuccessGames = 0;
    int  m_stressFailedGames = 0;
    int  m_stressDoAICallCount = 0;
    int  m_stressNoProgressCount = 0;
    int  m_stressNoProgressLimit = 20;
    std::tuple<int,int,int,int> m_lastTurnSignature = {-1,-1,-1,-1};
    bool m_stressPlaying = false;
    bool m_isStressPlayerATurn = false;

    static constexpr int kAITurnDelayNormalMs = 500;
    static constexpr int kAITurnDelayStressMs = 0;
    static constexpr int kDealWaitStressMs    = 1500;
    int m_aiTurnDelayMs = kAITurnDelayNormalMs;

    std::vector<CardWidget*> m_playerACardWidgets;
    std::vector<QWidget*> m_playerBCardWidgets;
    std::vector<CardWidget*> m_tableCardWidgets;

    AILevel m_aiLevel = AILevel::AI1_Rule;
    std::unique_ptr<class AIEngine> m_levelAIEngine;
    AIEngineConfig m_levelEngineConfig;
    LevelResult m_pendingLevelResult;  // 暂存关卡结算结果（对话结束后显示）
};

// Helper functions shared across cpp files
QString cardsToString(const std::vector<Card>& cards);
QString cardTypeToQString(CardType type);
void sortHandSmart(std::vector<Card>& hand);
void applyBtnShadow(QPushButton* btn);