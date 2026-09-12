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
    QPushButton* m_okButton;
    QPushButton* m_cancelButton;
    QLabel* m_hintLabel;

    void updateButtonStates();
    void updateHint();
};