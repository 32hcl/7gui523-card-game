#pragma once
#include <QWidget>
#include <QPushButton>
#include <QLabel>

class MainMenuScreen : public QWidget {
    Q_OBJECT
public:
    explicit MainMenuScreen(QWidget* parent = nullptr);

signals:
    void practiceModeSelected();
    void levelModeSelected();
};