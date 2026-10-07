#ifndef BOSS_AVATAR_H
#define BOSS_AVATAR_H

#include <QWidget>
#include <QString>

/**
 * @brief Boss风格化头像组件
 *
 * 用QPainter绘制的圆形风格化头像，每个Boss有独特的：
 * - 主色调（背景渐变）
 * - 符号（代表角色特征）
 * - 边框颜色
 *
 * 无需美术资源，纯代码生成。
 */
class BossAvatar : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(int level READ level WRITE setLevel NOTIFY levelChanged)
    Q_PROPERTY(int size READ avatarSize WRITE setAvatarSize)

public:
    explicit BossAvatar(QWidget* parent = nullptr);
    explicit BossAvatar(int level, QWidget* parent = nullptr);

    int level() const { return m_level; }
    void setLevel(int level);

    int avatarSize() const { return m_size; }
    void setAvatarSize(int size);

    /**
     * @brief 获取Boss显示名称
     */
    QString bossName() const;

    QSize sizeHint() const override;

    // 样式描述：需公开可见，.cpp 中以命名空间作用域定义全局样式表 g_bossStyles
    struct BossStyle {
        QColor primary;    // 主色
        QColor secondary;  // 辅助色
        QColor accent;     // 强调色（边框/高光）
        QString symbol;    // 符号
        QString title;     // 称号
    };

signals:
    void levelChanged(int level);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    static const BossStyle& getStyle(int level);
    void drawAvatar(QPainter& p, int cx, int cy, int radius);
    void drawSymbol(QPainter& p, int cx, int cy, int radius, const BossStyle& style);

    int m_level;
    int m_size;  // 头像直径
};

#endif // BOSS_AVATAR_H
