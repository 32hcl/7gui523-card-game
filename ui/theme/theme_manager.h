#ifndef THEME_MANAGER_H
#define THEME_MANAGER_H

#include <QString>
#include <QColor>
#include <QHash>
#include <QObject>

/**
 * @brief 主题管理类
 *
 * 负责统一管理应用的主题样式，包括：
 * - 加载/切换 QSS 样式表
 * - 提供颜色常量访问
 * - 动态属性辅助（设置自定义QSS属性）
 *
 * 使用方式：
 *   ThemeManager::instance()->applyTheme(qApp, "casino");
 */
class ThemeManager : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 获取单例实例
     */
    static ThemeManager* instance();

    /**
     * @brief 可用主题枚举
     */
    enum class Theme {
        Classic = 0,  // 经典主题（原默认样式）
        Casino  = 1,  // 国潮赌场风（推荐）
    };
    Q_ENUM(Theme)

    /**
     * @brief 颜色令牌枚举
     */
    enum class ColorToken {
        // 背景色
        BgDeep,
        BgPage,
        BgPanel,
        BgCard,

        // 金色系
        GoldLight,
        Gold,
        GoldDark,
        GoldDeep,

        // 红色系
        RedLight,
        Red,
        RedDark,
        RedDeep,

        // 赌桌绿
        GreenLight,
        Green,
        GreenDark,

        // 筹码色
        ChipRed,
        ChipBlue,
        ChipGreen,
        ChipBlack,

        // 文字色
        TextPrimary,
        TextSecondary,
        TextMuted,
    };
    Q_ENUM(ColorToken)

    /**
     * @brief 对 QApplication 应用主题
     * @param app  目标应用（通常传 qApp）
     * @param theme 主题类型
     * @return 是否加载成功
     */
    bool applyTheme(QObject* app, Theme theme);

    /**
     * @brief 按名称加载主题
     * @param app   目标应用
     * @param name  主题名称: "classic" / "casino"
     * @return 是否加载成功
     */
    bool applyThemeByName(QObject* app, const QString& name);

    /**
     * @brief 获取当前主题
     */
    Theme currentTheme() const { return m_currentTheme; }

    /**
     * @brief 获取当前主题名称
     */
    QString currentThemeName() const;

    /**
     * @brief 获取颜色值（按当前主题）
     */
    QColor color(ColorToken token) const;

    /**
     * @brief 获取颜色十六进制字符串（按当前主题）
     * @param token 颜色令牌
     * @return #RRGGBB 格式字符串
     */
    QString colorHex(ColorToken token) const;

    /**
     * @brief 刷新指定控件的样式（修改 dynamicProperty 后调用）
     * @param widget 目标控件
     */
    static void polish(QWidget* widget);

    /**
     * @brief 获取主题资源根路径
     */
    QString themeResourcePath() const { return m_resourcePath; }

private:
    ThemeManager();
    ~ThemeManager() override = default;
    Q_DISABLE_COPY(ThemeManager)

    /**
     * @brief 从资源文件加载 QSS 内容
     */
    QString loadQssFromResource(const QString& fileName) const;

    /**
     * @brief 初始化颜色表
     */
    void initColorTable();

    /**
     * @brief 主题枚举转名称字符串
     */
    static QString themeName(Theme theme);

    Theme              m_currentTheme;
    QString            m_resourcePath;   // 主题资源目录路径
    QHash<int, QColor> m_colorTable;     // 当前主题的颜色表
};

// ---- 便捷宏 ----
#define THEME ThemeManager::instance()
#define THEME_COLOR(token) ThemeManager::instance()->color(ThemeManager::ColorToken::token)
#define THEME_HEX(token)   ThemeManager::instance()->colorHex(ThemeManager::ColorToken::token)

#endif // THEME_MANAGER_H
