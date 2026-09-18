#include "card_picker.h"
#include "ui/widgets/selectable_card.h"
#include "core/card/deck.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QCoreApplication>
#include <QFile>

CardPickerDialog::CardPickerDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("选择起始手牌（可选 0~5 张）");
    setMinimumSize(950, 700);
    setStyleSheet("QDialog { background-color: #1a1a1a; }");

    Deck full = createStandardDeck();
    m_allCards = full.cards;

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);

    m_cardCanvas = new QWidget;
    m_cardCanvas->setStyleSheet("background: transparent;");
    mainLayout->addWidget(m_cardCanvas, 1);

    QString dir = QCoreApplication::applicationDirPath() + "/cards/";
    std::vector<std::string> suits = {"黑桃","红桃","梅花","方块"};
    std::vector<std::string> pts = {"A","2","3","4","5","6","7","8","9","10","J","Q","K"};

    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 13; ++col) {
            for (size_t i = 0; i < m_allCards.size(); ++i) {
                if (m_allCards[i].suit != suits[row]) continue;
                if (m_allCards[i].point != pts[col]) continue;
                QString sk = (suits[row]=="黑桃")?"spades":(suits[row]=="红桃")?"hearts":(suits[row]=="梅花")?"clubs":"diamonds";
                QString pk = QString::fromStdString(pts[col]);
                if (pk!="A"&&pk!="J"&&pk!="Q"&&pk!="K") pk = QString("%1").arg(pk.toInt(),2,10,QChar('0'));
                QString path = dir + QString("card_%1_%2.png").arg(sk).arg(pk);
                auto* cw = new SelectableCardWidget(m_allCards[i], path, m_cardCanvas);
                connect(cw, &SelectableCardWidget::clicked, this, &CardPickerDialog::onCardClicked);
                m_cards.push_back(cw); m_cols.push_back(col); m_rows.push_back(row);
                break;
            }
        }
    }
    int gc = 0;
    for (size_t i = 0; i < m_allCards.size(); ++i) {
        if (m_allCards[i].point != "大鬼" && m_allCards[i].point != "小鬼") continue;
        QString fn = (m_allCards[i].point=="大鬼") ? "card_joker_red.png" : "card_joker_black.png";
        auto* cw = new SelectableCardWidget(m_allCards[i], dir+fn, m_cardCanvas);
        connect(cw, &SelectableCardWidget::clicked, this, &CardPickerDialog::onCardClicked);
        m_cards.push_back(cw); m_cols.push_back(gc); m_rows.push_back(4);
        gc++;
    }

    auto* bl = new QHBoxLayout;
    bl->addStretch();
    m_cancelButton = new QPushButton("取消（随机发牌）");
    m_okButton = new QPushButton("确认");
    connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_okButton, &QPushButton::clicked, this, &QDialog::accept);
    bl->addWidget(m_cancelButton);
    bl->addWidget(m_okButton);
    mainLayout->addLayout(bl);
    layoutCards();
}

void CardPickerDialog::resizeEvent(QResizeEvent* e) {
    QDialog::resizeEvent(e);
    layoutCards();
}

void CardPickerDialog::layoutCards() {
    if (!m_cardCanvas) return;
    int W = m_cardCanvas->width(), H = m_cardCanvas->height();
    if (W <= 0 || H <= 0) return;

    double cw = W / 13.0, ch = H / 5.0;
    int cardW = (int)qMin(cw, ch * 2.0 / 3.0);
    int cardH = (int)(cardW * 3.0 / 2.0);
    if (cardW < 30) cardW = 30;
    if (cardH < 45) cardH = 45;

    int totalW = 13 * cardW, totalH = 5 * cardH;
    int sx = (W - totalW) / 2, sy = (H - totalH) / 2;

    for (size_t i = 0; i < m_cards.size(); ++i) {
        m_cards[i]->setGeometry(sx + m_cols[i]*cardW, sy + m_rows[i]*cardH, cardW, cardH);
    }
}

void CardPickerDialog::onCardClicked(SelectableCardWidget* w) {
    int idx = -1;
    for (size_t i = 0; i < m_allCards.size(); ++i) {
        if (m_allCards[i].point == w->card().point && m_allCards[i].suit == w->card().suit) { idx = (int)i; break; }
    }
    if (idx < 0) return;

    auto it = std::find(m_selectedIndices.begin(), m_selectedIndices.end(), idx);
    if (it != m_selectedIndices.end()) {
        m_selectedIndices.erase(it);
        w->setSelected(false);
    } else {
        if (m_selectedIndices.size() >= 5) {
            QMessageBox::information(this, "提示", "最多只能选择 5 张牌");
            return;
        }
        m_selectedIndices.push_back(idx);
        w->setSelected(true);
    }
}

std::vector<Card> CardPickerDialog::selectedCards() const {
    std::vector<Card> result;
    for (int idx : m_selectedIndices) result.push_back(m_allCards[idx]);
    return result;
}