#include "cardpickerdialog.h"
#include "core/deck.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QCoreApplication>
#include <QFile>
#include <QMouseEvent>

CardPickerDialog::CardPickerDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("选择起始手牌（可选 0~5 张）");
    setMinimumSize(950, 700);
    setStyleSheet("QDialog { background-color: #1a1a1a; }");

    Deck full = createStandardDeck();
    m_allCards = full.cards;

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(4);

    m_cardCanvas = new QWidget;
    m_cardCanvas->setStyleSheet("background: transparent;");
    mainLayout->addWidget(m_cardCanvas, 1);

    QString cardsDir = QCoreApplication::applicationDirPath() + "/cards/";

    std::vector<std::string> suits = {"黑桃", "红桃", "梅花", "方块"};
    std::vector<std::string> points = {"A","2","3","4","5","6","7","8","9","10","J","Q","K"};

    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 13; ++col) {
            for (size_t i = 0; i < m_allCards.size(); ++i) {
                if (m_allCards[i].suit != suits[row]) continue;
                if (m_allCards[i].point != points[col]) continue;

                QString suitKey;
                if (suits[row] == "黑桃") suitKey = "spades";
                else if (suits[row] == "红桃") suitKey = "hearts";
                else if (suits[row] == "梅花") suitKey = "clubs";
                else suitKey = "diamonds";

                QString pointKey = QString::fromStdString(points[col]);
                if (pointKey != "A" && pointKey != "J" &&
                    pointKey != "Q" && pointKey != "K") {
                    int n = pointKey.toInt();
                    pointKey = QString("%1").arg(n, 2, 10, QChar('0'));
                }
                QString path = cardsDir + QString("card_%1_%2.png").arg(suitKey).arg(pointKey);

                QPixmap pix;
                if (QFile::exists(path)) pix.load(path);

                auto* lbl = new QLabel(m_cardCanvas);
                lbl->setAlignment(Qt::AlignCenter);
                lbl->setScaledContents(true);
                lbl->setProperty("cardIndex", (int)i);
                lbl->setProperty("selected", false);
                lbl->setProperty("srcPixmap", QVariant::fromValue(pix));
                lbl->setStyleSheet(
                    "QLabel { border: 2px solid transparent; background: transparent; padding: 0; }"
                    "QLabel[selected=\"true\"] { border: 2px solid transparent; }"
                );
                lbl->installEventFilter(this);
                auto* overlay = new QWidget(lbl);
                overlay->setAttribute(Qt::WA_TransparentForMouseEvents);
                overlay->setStyleSheet(
                    "QWidget { background: transparent; border: 2px solid #FFD700; border-radius: 4px; }"
                );
                overlay->hide();
                lbl->setProperty("overlay", QVariant::fromValue(overlay));


                m_cardCol.push_back(col);
                m_cardRow.push_back(row);
                m_cardLabels.push_back(lbl);
                break;
            }
        }
    }

    {
        int ghostCol = 0;
        for (size_t i = 0; i < m_allCards.size(); ++i) {
            if (m_allCards[i].point != "大鬼" && m_allCards[i].point != "小鬼") continue;
            QString fn = (m_allCards[i].point == "大鬼")
                ? "card_joker_red.png" : "card_joker_black.png";
            QString path = cardsDir + fn;
            QPixmap pix;
            if (QFile::exists(path)) pix.load(path);

            auto* lbl = new QLabel(m_cardCanvas);
            lbl->setAlignment(Qt::AlignCenter);
            lbl->setScaledContents(true);
            lbl->setProperty("cardIndex", (int)i);
            lbl->setProperty("selected", false);
            lbl->setProperty("srcPixmap", QVariant::fromValue(pix));
            lbl->setStyleSheet(
                "QLabel { border: 2px solid transparent; background: transparent; padding: 0; }"
                "QLabel[selected=\"true\"] { border: 2px solid #FFD700; }"
            );
            lbl->installEventFilter(this);

            m_cardCol.push_back(ghostCol);
            m_cardRow.push_back(4);
            m_cardLabels.push_back(lbl);
            ghostCol++;
        }
    }

    auto* bottomLayout = new QHBoxLayout;
    bottomLayout->addStretch();
    m_cancelButton = new QPushButton("取消（随机发牌）");
    m_okButton = new QPushButton("确认");
    connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_okButton, &QPushButton::clicked, this, &QDialog::accept);
    bottomLayout->addWidget(m_cancelButton);
    bottomLayout->addWidget(m_okButton);
    mainLayout->addLayout(bottomLayout);

    layoutCards();
}

void CardPickerDialog::resizeEvent(QResizeEvent* event) {
    QDialog::resizeEvent(event);
    layoutCards();
}

bool CardPickerDialog::eventFilter(QObject* obj, QEvent* ev) {
    if (ev->type() == QEvent::MouseButtonPress) {
        QLabel* lbl = qobject_cast<QLabel*>(obj);
        if (lbl) { onCardClicked(lbl); return true; }
    }
    return QDialog::eventFilter(obj, ev);
}

void CardPickerDialog::onCardClicked(QLabel* lbl) {
    int cardIndex = lbl->property("cardIndex").toInt();
    bool selected = lbl->property("selected").toBool();

    auto it = std::find(m_selectedIndices.begin(), m_selectedIndices.end(), cardIndex);
    if (it != m_selectedIndices.end()) {
        m_selectedIndices.erase(it);
        lbl->setProperty("selected", false);
    } else {
        if (m_selectedIndices.size() >= 5) {
            QMessageBox::information(this, "提示", "最多只能选择 5 张牌");
            return;
        }
        m_selectedIndices.push_back(cardIndex);
        lbl->setProperty("selected", true);
    }
    lbl->style()->unpolish(lbl);
    lbl->style()->polish(lbl);
    lbl->update();
    QWidget* overlay = lbl->property("overlay").value<QWidget*>();
    if (overlay) {
        overlay->setVisible(lbl->property("selected").toBool());
        overlay->raise();
    }
}

void CardPickerDialog::layoutCards() {
    if (!m_cardCanvas) return;
    int W = m_cardCanvas->width();
    int H = m_cardCanvas->height();
    if (W <= 0 || H <= 0) return;

    double cellW = W / 13.0;
    double cellH = H / 5.0;
    double ratio = 2.0 / 3.0;

    int cardW = (int)qMin(cellW, cellH * ratio);
    int cardH = (int)(cardW / ratio);
    if (cardW < 30) cardW = 30;
    if (cardH < 45) cardH = 45;

    int totalW = 13 * cardW;
    int totalH = 5 * cardH;
    int startX = (W - totalW) / 2;
    int startY = (H - totalH) / 2;

    for (size_t i = 0; i < m_cardLabels.size(); ++i) {
        QLabel* lbl = m_cardLabels[i];
        QPixmap pix = lbl->property("srcPixmap").value<QPixmap>();
        if (!pix.isNull()) {
            lbl->setPixmap(pix.scaled(cardW, cardH,
                           Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
        }
        int x = startX + m_cardCol[i] * cardW;
        int y = startY + m_cardRow[i] * cardH;
        lbl->setGeometry(x, y, cardW, cardH);
        QWidget* overlay = lbl->property("overlay").value<QWidget*>();
        if (overlay) {
            overlay->setGeometry(2, 2, cardW - 4, cardH - 4);
        }
    }
}

std::vector<Card> CardPickerDialog::selectedCards() const {
    std::vector<Card> result;
    for (int idx : m_selectedIndices) result.push_back(m_allCards[idx]);
    return result;
}