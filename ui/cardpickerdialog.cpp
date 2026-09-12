#include "cardpickerdialog.h"
#include "core/deck.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QMessageBox>

CardPickerDialog::CardPickerDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("选择起始手牌");
    setMinimumWidth(800);

    Deck full = createStandardDeck();
    m_allCards = full.cards;

    auto* mainLayout = new QVBoxLayout(this);

    m_hintLabel = new QLabel("请选择 5 张牌作为你的起始手牌（已选 0/5）");
    QFont hintFont = m_hintLabel->font();
    hintFont.setPointSize(12);
    m_hintLabel->setFont(hintFont);
    mainLayout->addWidget(m_hintLabel);

    auto* gridWidget = new QWidget;
    auto* grid = new QGridLayout(gridWidget);
    grid->setSpacing(4);

    std::vector<std::string> suits = {"黑桃", "红桃", "梅花", "方块"};
    std::vector<std::string> points = {"A", "2", "3", "4", "5", "6", "7",
                                        "8", "9", "10", "J", "Q", "K"};

    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 13; ++col) {
            for (size_t i = 0; i < m_allCards.size(); ++i) {
                if (m_allCards[i].suit == suits[row] &&
                    m_allCards[i].point == points[col]) {
                    QString text = QString::fromStdString(suits[row] + points[col]);
                    auto* btn = new QPushButton(text);
                    btn->setFixedSize(56, 48);
                    btn->setCheckable(true);
                    btn->setProperty("cardIndex", (int)i);
                    connect(btn, &QPushButton::clicked,
                            this, &CardPickerDialog::onCardClicked);
                    grid->addWidget(btn, row, col);
                    m_cardButtons.push_back(btn);
                    break;
                }
            }
        }
    }

    {
        int row = 4;
        int col = 0;
        for (size_t i = 0; i < m_allCards.size(); ++i) {
            if (m_allCards[i].point == "大鬼" || m_allCards[i].point == "小鬼") {
                QString text = QString::fromStdString(m_allCards[i].point);
                auto* btn = new QPushButton(text);
                btn->setFixedSize(56, 48);
                btn->setCheckable(true);
                btn->setProperty("cardIndex", (int)i);
                connect(btn, &QPushButton::clicked,
                        this, &CardPickerDialog::onCardClicked);
                grid->addWidget(btn, row, col);
                m_cardButtons.push_back(btn);
                col++;
            }
        }
    }

    mainLayout->addWidget(gridWidget);

    auto* bottomLayout = new QHBoxLayout;
    bottomLayout->addStretch();

    m_cancelButton = new QPushButton("取消（随机发牌）");
    m_okButton = new QPushButton("确认");
    m_okButton->setEnabled(false);

    connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_okButton, &QPushButton::clicked, this, &QDialog::accept);

    bottomLayout->addWidget(m_cancelButton);
    bottomLayout->addWidget(m_okButton);
    mainLayout->addLayout(bottomLayout);
}

void CardPickerDialog::onCardClicked() {
    auto* btn = qobject_cast<QPushButton*>(sender());
    if (!btn) return;

    int cardIndex = btn->property("cardIndex").toInt();

    auto it = std::find(m_selectedIndices.begin(), m_selectedIndices.end(),
                        cardIndex);
    if (it != m_selectedIndices.end()) {
        m_selectedIndices.erase(it);
        btn->setStyleSheet("");
    } else {
        if (m_selectedIndices.size() >= 5) {
            QMessageBox::information(this, "提示", "最多只能选择 5 张牌");
            return;
        }
        m_selectedIndices.push_back(cardIndex);
        btn->setStyleSheet(
            "QPushButton { background-color: #FFD700;"
            " border: 2px solid #FF6600; }");
    }

    updateButtonStates();
    updateHint();
}

void CardPickerDialog::updateButtonStates() {
    m_okButton->setEnabled(m_selectedIndices.size() == 5);
}

void CardPickerDialog::updateHint() {
    m_hintLabel->setText(
        QString("请选择 5 张牌作为你的起始手牌（已选 %1/5）")
            .arg(m_selectedIndices.size()));
}

std::vector<Card> CardPickerDialog::selectedCards() const {
    std::vector<Card> result;
    for (int idx : m_selectedIndices) {
        result.push_back(m_allCards[idx]);
    }
    return result;
}