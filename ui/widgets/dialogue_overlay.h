#ifndef DIALOGUE_OVERLAY_H
#define DIALOGUE_OVERLAY_H

#include <QWidget>
#include <QVector>
#include <QString>
#include <QTimer>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPropertyAnimation>

#include "dialogue/dialogue_engine.h"

class BossAvatar;

/**
 * @brief 对话覆盖层
 *
 * 国潮赌场风对话框，支持：
 * - 旁白模式：居中文字，半透明深色遮罩
 * - Boss对话模式：顶部Boss头像 + 气泡文字
 * - 打字机效果（逐字显示）
 * - 点击/按空格继续下一句
 * - 全部播放完毕后发出 finished 信号
 *
 * 作为覆盖层悬浮在战斗界面上方。
 */
class DialogueOverlay : public QWidget
{
    Q_OBJECT

public:
    explicit DialogueOverlay(QWidget* parent = nullptr);

    /**
     * @brief 播放一组对话
     * @param lines 对话列表
     * @param bossLevel Boss关卡（用于头像和名字，0=无Boss）
     */
    void playDialogue(const QVector<DialogueLine>& lines, int bossLevel = 0);

    /**
     * @brief 是否正在播放对话
     */
    bool isPlaying() const { return m_isPlaying; }

    /**
     * @brief 立即跳过当前对话（直接跳到最后一句并结束）
     */
    void skipAll();

signals:
    /**
     * @brief 所有对话播放完毕
     */
    void dialogueFinished();

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void showEvent(QShowEvent* event) override;

private slots:
    void onTypewriterTick();
    void advanceLine();

private:
    void showCurrentLine();
    void startTypewriter(const QString& text);
    void finishTypewriter();
    void setupNarrationMode(const QString& text);
    void setupBossMode(const QString& bossName, const QString& text);
    void clearLayout();

    // 状态
    bool                m_isPlaying   = false;
    int                 m_bossLevel   = 0;
    QVector<DialogueLine> m_lines;
    int                 m_currentIdx  = 0;

    // 打字机
    QTimer*             m_typeTimer   = nullptr;
    QString             m_fullText;
    int                 m_typePos     = 0;
    int                 m_typeSpeedMs = 35;  // 每字间隔

    // UI
    QWidget*            m_dialogBox   = nullptr;  // 对话框容器
    QVBoxLayout*        m_mainLayout  = nullptr;
    QLabel*             m_speakerLabel = nullptr; // 说话人名字
    QLabel*             m_textLabel   = nullptr;  // 对话内容
    QLabel*             m_continueLabel = nullptr; // "点击继续"提示
    BossAvatar*         m_bossAvatar  = nullptr;
    QHBoxLayout*        m_bubbleLayout = nullptr;

    // 出现动画
    QPropertyAnimation* m_fadeAnim    = nullptr;
};

#endif // DIALOGUE_OVERLAY_H
