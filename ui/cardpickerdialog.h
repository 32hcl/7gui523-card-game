#pragma once

#include <QDialog>
#include <QPushButton>
#include <QLabel>
#include <vector>
#include "core/card.h"

class CardPickerDialog : public QDialog {
    Q_OBJECT
public:
    explicit CardPickerDialog(QWidget* parent = nullptr);

    std::vector<Card> selectedCards() const;

private slots:
    void onCardClicked();

private:
    std::vector<Card> m_allCards;
    std::vector<int>  m_selectedIndices;
    std::vector<QPushButton*> m_cardButtons;
    QPushButton* m_okButton = nullptr;
    QPushButton* m_cancelButton = nullptr;
    QLabel* m_hintLabel = nullptr;

    void updateButtonStates();
    void updateHint();
};