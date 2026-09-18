#pragma once
#include <QObject>
#include <QStackedWidget>
#include <QWidget>

enum class ScreenId {
    Battle,
};

class ScreenRouter : public QObject {
    Q_OBJECT
public:
    explicit ScreenRouter(QStackedWidget* stack, QObject* parent = nullptr);

    void registerScreen(ScreenId id, QWidget* screen);
    void switchTo(ScreenId id);
    ScreenId currentScreen() const;

private:
    QStackedWidget* m_stack;
    ScreenId m_current = ScreenId::Battle;
};