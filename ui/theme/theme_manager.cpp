#include "ui/theme/theme_manager.h"

#include <QApplication>
#include <QFile>
#include <QWidget>
#include <QDir>
#include <QStyle>
#include <QCoreApplication>
#include <QDebug>

// ============================================================
// 单例
// ============================================================

ThemeManager* ThemeManager::instance()
{
    static ThemeManager s_instance;
    return &s_instance;
}

ThemeManager::ThemeManager()
    : QObject(nullptr)
    , m_currentTheme(Theme::Classic)
{
    // 资源路径: 应用目录/themes/
    // 运行时从文件系统加载（非qrc资源），方便热切换
    m_resourcePath = QCoreApplication::applicationDirPath() + "/themes/";

    initColorTable();
}

// ============================================================
// 主题应用
// ============================================================

bool ThemeManager::applyTheme(QObject* app, Theme theme)
{
    QString qssFileName;

    switch (theme) {
    case Theme::Casino:
        qssFileName = "theme_casino.qss";
        break;
    case Theme::Classic:
    default:
        // 经典主题：空样式，使用Qt默认 + 各控件内联样式
        qssFileName = "";
        break;
    }

    QString qssContent;
    if (!qssFileName.isEmpty()) {
        // 优先从应用目录的 themes/ 子目录加载
        QString filePath = m_resourcePath + qssFileName;
        QFile file(filePath);

        if (file.open(QFile::ReadOnly | QFile::Text)) {
            qssContent = QString::fromUtf8(file.readAll());
            file.close();
            qDebug() << "[Theme] Loaded theme file:" << filePath;
        } else {
            // 回退: 从 :/themes/ 资源加载
            QString resPath = QStringLiteral(":/themes/") + qssFileName;
            QFile resFile(resPath);
            if (resFile.open(QFile::ReadOnly | QFile::Text)) {
                qssContent = QString::fromUtf8(resFile.readAll());
                resFile.close();
                qDebug() << "[Theme] Loaded theme from resource:" << resPath;
            } else {
                qWarning() << "[Theme] Failed to load theme file:" << filePath
                           << "and resource fallback:" << resPath;
                return false;
            }
        }
    }

    // 应用到 QApplication
    if (app) {
        app->setProperty("theme", themeName(theme));
        // setStyleSheet 是 QApplication 的实例方法，不能以静态方式调用
        if (auto* qtApp = qobject_cast<QApplication*>(app))
            qtApp->setStyleSheet(qssContent);
    }

    m_currentTheme = theme;
    initColorTable(); // 刷新颜色表

    qInfo() << "[Theme] Applied theme:" << currentThemeName();
    return true;
}

bool ThemeManager::applyThemeByName(QObject* app, const QString& name)
{
    Theme theme = Theme::Classic;

    QString lower = name.toLower();
    if (lower == "casino" || lower == "国潮" || lower == "赌场") {
        theme = Theme::Casino;
    } else if (lower == "classic" || lower == "经典") {
        theme = Theme::Classic;
    } else {
        qWarning() << "[Theme] Unknown theme name:" << name << ", fallback to Classic";
    }

    return applyTheme(app, theme);
}

QString ThemeManager::currentThemeName() const
{
    return themeName(m_currentTheme);
}

QString ThemeManager::themeName(Theme theme)
{
    switch (theme) {
    case Theme::Casino:  return QStringLiteral("casino");
    case Theme::Classic: return QStringLiteral("classic");
    }
    return QStringLiteral("classic");
}

// ============================================================
// 颜色系统
// ============================================================

void ThemeManager::initColorTable()
{
    m_colorTable.clear();

    switch (m_currentTheme) {
    case Theme::Casino: {
        // ---- 背景色 ----
        m_colorTable.insert(static_cast<int>(ColorToken::BgDeep),    QColor("#0a0608"));
        m_colorTable.insert(static_cast<int>(ColorToken::BgPage),    QColor("#1a0f0a"));
        m_colorTable.insert(static_cast<int>(ColorToken::BgPanel),   QColor("#2a1a14"));
        m_colorTable.insert(static_cast<int>(ColorToken::BgCard),    QColor("#3a2418"));

        // ---- 金色系 ----
        m_colorTable.insert(static_cast<int>(ColorToken::GoldLight), QColor("#fff3a0"));
        m_colorTable.insert(static_cast<int>(ColorToken::Gold),      QColor("#ffd700"));
        m_colorTable.insert(static_cast<int>(ColorToken::GoldDark),  QColor("#b8860b"));
        m_colorTable.insert(static_cast<int>(ColorToken::GoldDeep),  QColor("#8b6508"));

        // ---- 红色系 ----
        m_colorTable.insert(static_cast<int>(ColorToken::RedLight),  QColor("#e63946"));
        m_colorTable.insert(static_cast<int>(ColorToken::Red),       QColor("#c41e3a"));
        m_colorTable.insert(static_cast<int>(ColorToken::RedDark),   QColor("#7f1d1d"));
        m_colorTable.insert(static_cast<int>(ColorToken::RedDeep),   QColor("#450a0a"));

        // ---- 赌桌绿 ----
        m_colorTable.insert(static_cast<int>(ColorToken::GreenLight), QColor("#1a8f42"));
        m_colorTable.insert(static_cast<int>(ColorToken::Green),      QColor("#0d5c2a"));
        m_colorTable.insert(static_cast<int>(ColorToken::GreenDark),  QColor("#0a2915"));

        // ---- 筹码色 ----
        m_colorTable.insert(static_cast<int>(ColorToken::ChipRed),    QColor("#e63946"));
        m_colorTable.insert(static_cast<int>(ColorToken::ChipBlue),   QColor("#3b82f6"));
        m_colorTable.insert(static_cast<int>(ColorToken::ChipGreen),  QColor("#22c55e"));
        m_colorTable.insert(static_cast<int>(ColorToken::ChipBlack),  QColor("#1f2937"));

        // ---- 文字色 ----
        m_colorTable.insert(static_cast<int>(ColorToken::TextPrimary),   QColor("#fff8e7"));
        m_colorTable.insert(static_cast<int>(ColorToken::TextSecondary), QColor("#c9b896"));
        m_colorTable.insert(static_cast<int>(ColorToken::TextMuted),     QColor("#8b7355"));
        break;
    }

    case Theme::Classic:
    default: {
        // 经典主题：与项目现有配色保持一致
        // （仅供参考，经典主题下样式由各文件内联控制）
        m_colorTable.insert(static_cast<int>(ColorToken::BgDeep),    QColor("#2a1a1a"));
        m_colorTable.insert(static_cast<int>(ColorToken::BgPage),    QColor("#3a2020"));
        m_colorTable.insert(static_cast<int>(ColorToken::BgPanel),   QColor("#1e1e2a"));
        m_colorTable.insert(static_cast<int>(ColorToken::BgCard),    QColor("#2a2a40"));

        m_colorTable.insert(static_cast<int>(ColorToken::GoldLight), QColor("#ffe66d"));
        m_colorTable.insert(static_cast<int>(ColorToken::Gold),      QColor("#FFD700"));
        m_colorTable.insert(static_cast<int>(ColorToken::GoldDark),  QColor("#DAA520"));
        m_colorTable.insert(static_cast<int>(ColorToken::GoldDeep),  QColor("#B8860B"));

        m_colorTable.insert(static_cast<int>(ColorToken::RedLight),  QColor("#F0634E"));
        m_colorTable.insert(static_cast<int>(ColorToken::Red),       QColor("#E8503A"));
        m_colorTable.insert(static_cast<int>(ColorToken::RedDark),   QColor("#C43D28"));
        m_colorTable.insert(static_cast<int>(ColorToken::RedDeep),   QColor("#7f1d1d"));

        m_colorTable.insert(static_cast<int>(ColorToken::GreenLight), QColor("#4ade80"));
        m_colorTable.insert(static_cast<int>(ColorToken::Green),      QColor("#22c55e"));
        m_colorTable.insert(static_cast<int>(ColorToken::GreenDark),  QColor("#15803d"));

        m_colorTable.insert(static_cast<int>(ColorToken::ChipRed),    QColor("#E8503A"));
        m_colorTable.insert(static_cast<int>(ColorToken::ChipBlue),   QColor("#3b82f6"));
        m_colorTable.insert(static_cast<int>(ColorToken::ChipGreen),  QColor("#22c55e"));
        m_colorTable.insert(static_cast<int>(ColorToken::ChipBlack),  QColor("#555555"));

        m_colorTable.insert(static_cast<int>(ColorToken::TextPrimary),   QColor("#ffffff"));
        m_colorTable.insert(static_cast<int>(ColorToken::TextSecondary), QColor("#B0BEC5"));
        m_colorTable.insert(static_cast<int>(ColorToken::TextMuted),     QColor("#777777"));
        break;
    }
    }
}

QColor ThemeManager::color(ColorToken token) const
{
    auto it = m_colorTable.constFind(static_cast<int>(token));
    if (it != m_colorTable.constEnd()) {
        return it.value();
    }
    qWarning() << "[Theme] Color token not found:" << static_cast<int>(token);
    return QColor(Qt::white);
}

QString ThemeManager::colorHex(ColorToken token) const
{
    return color(token).name(QColor::HexRgb);
}

// ============================================================
// 工具函数
// ============================================================

void ThemeManager::polish(QWidget* widget)
{
    if (!widget) return;
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
}

QString ThemeManager::loadQssFromResource(const QString& fileName) const
{
    QString path = m_resourcePath + fileName;
    QFile file(path);
    if (file.open(QFile::ReadOnly | QFile::Text)) {
        return QString::fromUtf8(file.readAll());
    }
    return QString();
}
