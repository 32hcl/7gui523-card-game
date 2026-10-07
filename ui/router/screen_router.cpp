#include "screen_router.h"

ScreenRouter::ScreenRouter(QStackedWidget* stack, QObject* parent)
    : QObject(parent), m_stack(stack)
{
}

void ScreenRouter::registerScreen(ScreenId id, QWidget* screen) {
    m_stack->addWidget(screen);
    screen->setProperty("screenId", static_cast<int>(id));
}

void ScreenRouter::switchTo(ScreenId id) {
    for (int i = 0; i < m_stack->count(); ++i) {
        QWidget* w = m_stack->widget(i);
        if (w && w->property("screenId").toInt() == static_cast<int>(id)) {
            m_stack->setCurrentIndex(i);
            m_current = id;
            return;
        }
    }
}

ScreenId ScreenRouter::currentScreen() const {
    return m_current;
}