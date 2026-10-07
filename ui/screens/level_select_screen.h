#ifndef LEVEL_SELECT_SCREEN_H
#define LEVEL_SELECT_SCREEN_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QList>

class BossAvatar;

/**
 * @brief 关卡选择界面
 *
 * 国潮赌场风设计：
 * - 顶部：标题+筹码余额+返回按钮
 * - 中间：9层楼纵向排列（从下到上1→9层）
 *   - 已通关：金色高亮 + ✓标记
 *   - 当前可挑战：金色边框闪烁 + 可点击
 *   - 未解锁：灰色 + 🔒锁定
 * - 每层卡片：Boss头像 + 名字 + 称号 + 入场筹码 + 简介
 */
class LevelSelectScreen : public QWidget
{
    Q_OBJECT

public:
    explicit LevelSelectScreen(QWidget* parent = nullptr);

    /**
     * @brief 设置玩家当前进度（已通关到第几层）
     * @param clearedLevel 已通关的最高层数（0=未开始，1=通关了1层可以挑战2层…）
     */
    void setProgress(int clearedLevel);

    /**
     * @brief 设置玩家筹码余额
     */
    void setChips(int chips);

signals:
    void backToMenu();          // 返回主菜单
    void levelSelected(int level);  // 选择了某一关

private slots:
    void onLevelClicked(int level);

private:
    void buildUi();
    void updateLevelStates();

    int m_clearedLevel = 0;  // 已通关的最高层
    int m_chips = 100;       // 筹码余额

    // UI
    QLabel*        m_titleLabel   = nullptr;
    QLabel*        m_chipsLabel   = nullptr;
    QPushButton*   m_backButton   = nullptr;
    QScrollArea*   m_scrollArea   = nullptr;
    QWidget*       m_levelsWidget = nullptr;
    QVBoxLayout*   m_levelsLayout = nullptr;

    struct LevelCard {
        int level;
        QWidget*     card;
        BossAvatar*  avatar;
        QLabel*      nameLabel;
        QLabel*      titleLabel;
        QLabel*      introLabel;
        QLabel*      chipsLabel;
        QPushButton* startBtn;
        QLabel*      lockLabel;
        QLabel*      clearedLabel;
    };

    QList<LevelCard> m_levelCards;
};

#endif // LEVEL_SELECT_SCREEN_H
