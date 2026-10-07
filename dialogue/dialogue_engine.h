#ifndef DIALOGUE_ENGINE_H
#define DIALOGUE_ENGINE_H

#include <QString>
#include <QVector>
#include <QHash>

/**
 * @brief 对话触发时机
 */
enum class DialogueTrigger {
    PreBattle,     // 战前（Boss登场）
    OnFirstBomb,   // 第一次出炸弹
    OnFirstRocket, // 第一次出王炸
    OnSpecial523,  // 玩家凑齐七鬼523
    OnPlayerWin,   // 玩家胜利
    OnBossWin,     // Boss胜利
    OnPlayerPass,  // 玩家Pass
    OnBossPass,    // BossPass
};

/**
 * @brief 单条对话
 */
struct DialogueLine {
    QString speaker;  // 说话人（空=旁白）
    QString text;     // 对话内容
    bool    isBoss = false;  // 是否是Boss说的
};

/**
 * @brief 对话引擎
 *
 * 管理所有Boss的剧情对话，支持多种触发时机：
 * - 战前登场对话（2-4句）
 * - 胜利/失败对话
 * - 特殊事件触发（炸弹/王炸/523等）
 *
 * 所有对话数据内置，无需外部资源。
 */
class DialogueEngine
{
public:
    static DialogueEngine& instance();

    /**
     * @brief 获取指定关卡、指定触发的对话
     * @param level 关卡（1-9）
     * @param trigger 触发时机
     * @return 对话列表（可能为空）
     */
    QVector<DialogueLine> getDialogue(int level, DialogueTrigger trigger) const;

    /**
     * @brief 获取Boss简介（关卡选择界面用）
     */
    QString getBossIntro(int level) const;

    /**
     * @brief 获取Boss称号
     */
    QString getBossTitle(int level) const;

    /**
     * @brief 获取关卡背景描述
     */
    QString getLevelDescription(int level) const;

private:
    DialogueEngine();
    ~DialogueEngine() = default;
    Q_DISABLE_COPY(DialogueEngine)

    void initData();

    // key: "level_trigger"
    QHash<QString, QVector<DialogueLine>> m_dialogues;
    QHash<int, QString> m_bossIntros;
    QHash<int, QString> m_bossTitles;
    QHash<int, QString> m_levelDescs;
};

#endif // DIALOGUE_ENGINE_H
