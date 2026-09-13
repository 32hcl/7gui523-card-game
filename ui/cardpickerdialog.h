#pragma once
#include <QDialog>
#include <QPushButton>
#include <vector>
#include "core/card.h"

class SelectableCardWidget;

class CardPickerDialog : public QDialog {
    Q_OBJECT
public:
    explicit CardPickerDialog(QWidget* parent = nullptr);
    std::vector<Card> selectedCards() const;

protected:
    void resizeEvent(QResizeEvent* e) override;

private slots:
    void onCardClicked(SelectableCardWidget* w);

private:
    void layoutCards();
    std::vector<Card> m_allCards;
    std::vector<int> m_selectedIndices;
    std::vector<SelectableCardWidget*> m_cards;
    std::vector<int> m_cols, m_rows;
    QWidget* m_cardCanvas = nullptr;
    QPushButton *m_okButton = nullptr, *m_cancelButton = nullptr;
};