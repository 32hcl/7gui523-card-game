#pragma once
#include <QString>

class DialogueEngine {
public:
    DialogueEngine() = default;
    ~DialogueEngine() = default;

    QString getLine(const QString& trigger, const QString& aiLevel);
};