#pragma once

#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QResizeEvent>
#include <QPixmap>
#include <vector>
#include "core/card.h"

class CardPickerDialog : public QDialog {
    Q_OBJECT
public:
    explicit CardPickerDialog(QWidget* parent = nullptr);

    std::vector<Card> selectedCards() const;

protected:
    void resizeEvent(QResizeEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* ev) override;

private:
    void layoutCards();
    void onCardClicked(QLabel* lbl);

    std::vector<Card> m_allCards;
    std::vector<int>  m_selectedIndices;
    std::vector<QLabel*> m_cardLabels;
    std::vector<int>  m_cardCol;
    std::vector<int>  m_cardRow;
    QPushButton* m_okButton = nullptr;
    QPushButton* m_cancelButton = nullptr;
    QWidget* m_cardCanvas = nullptr;
};
