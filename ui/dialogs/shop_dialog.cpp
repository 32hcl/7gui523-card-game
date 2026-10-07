#include "shop_dialog.h"
#include "ui/widgets/card_widget.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QScrollArea>
#include <QMessageBox>
#include <QCoreApplication>
#include <QFile>
#include <QGraphicsDropShadowEffect>
#include <QFrame>

static const int kCardW = 80;
static const int kCardH = 112;

ShopDialog::ShopDialog(int currentChips, int currentHp, int maxHp, bool viewOnly, QWidget* parent)
    : QDialog(parent)
    , m_currentChips(currentChips)
    , m_currentHp(currentHp)
    , m_maxHp(maxHp)
    , m_viewOnly(viewOnly)
{
    setWindowTitle("商店");
    setMinimumSize(700, 520);
    setStyleSheet(R"(
        QDialog {
            background-color: #1a1a2e;
        }
        QScrollArea {
            background: transparent;
            border: none;
        }
        QScrollBar:vertical {
            background: #2a2a3e;
            width: 10px;
            border-radius: 5px;
        }
        QScrollBar::handle:vertical {
            background: #4a4a6e;
            border-radius: 5px;
            min-height: 30px;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0px;
        }
    )");

    buildUI();
}

void ShopDialog::buildUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 16, 20, 16);
    mainLayout->setSpacing(12);

    // ── 顶栏：标题 + 筹码 + HP ──
    auto* topBar = new QHBoxLayout;

    auto* title = new QLabel("商店");
    QFont tf = title->font();
    tf.setPointSize(22);
    tf.setBold(true);
    title->setFont(tf);
    title->setStyleSheet("color: #FFD700; background: transparent;");
    topBar->addWidget(title);

    topBar->addStretch();

    // 筹码显示
    m_chipsLabel = new QLabel();
    m_chipsLabel->setStyleSheet(R"(
        QLabel {
            color: #FFD700;
            background: #2a2a3e;
            border: 2px solid #FFD700;
            border-radius: 10px;
            padding: 6px 16px;
            font-size: 16px;
            font-weight: bold;
        }
    )");
    topBar->addWidget(m_chipsLabel);

    topBar->addSpacing(12);

    // HP 显示
    m_hpLabel = new QLabel();
    m_hpLabel->setStyleSheet(R"(
        QLabel {
            color: #FF6B6B;
            background: #2a2a3e;
            border: 2px solid #FF6B6B;
            border-radius: 10px;
            padding: 6px 16px;
            font-size: 16px;
            font-weight: bold;
        }
    )");
    topBar->addWidget(m_hpLabel);

    mainLayout->addLayout(topBar);

    // ── 分隔线 ──
    auto* sep = new QFrame;
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet("QFrame { color: #3a3a5e; }");
    mainLayout->addWidget(sep);

    // ── 浏览模式提示（对局中不可购买） ──
    if (m_viewOnly) {
        auto* banner = new QLabel("    对局进行中，结算后可在商店购买    ");
        banner->setAlignment(Qt::AlignCenter);
        banner->setStyleSheet(R"(
            QLabel {
                color: #FFAB40;
                background: #2a2020;
                border: 1px solid #FFAB40;
                border-radius: 6px;
                padding: 8px;
                font-size: 14px;
                font-weight: bold;
            }
        )");
        mainLayout->addWidget(banner);
    }

    // ── 物品网格（可滚动） ──
    m_scroll = new QScrollArea;
    m_scroll->setWidgetResizable(true);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto* gridWidget = new QWidget;
    gridWidget->setStyleSheet("background: transparent;");
    m_itemGrid = new QGridLayout(gridWidget);
    m_itemGrid->setSpacing(16);
    m_itemGrid->setContentsMargins(4, 8, 4, 8);

    const auto& items = m_shop.items();
    for (size_t i = 0; i < items.size(); ++i) {
        int row = static_cast<int>(i) / 3;
        int col = static_cast<int>(i) % 3;
        m_itemGrid->addWidget(createItemCard(items[i]), row, col);
    }

    m_scroll->setWidget(gridWidget);
    mainLayout->addWidget(m_scroll, 1);

    // ── 底栏按钮 ──
    auto* bottomBar = new QHBoxLayout;
    bottomBar->addStretch();

    // 刷新费用标签
    m_refreshCostLabel = new QLabel();
    m_refreshCostLabel->setStyleSheet(R"(
        QLabel {
            color: #80CBC4;
            background: transparent;
            font-size: 13px;
            padding: 0 8px;
        }
    )");
    bottomBar->addWidget(m_refreshCostLabel);

    // 刷新按钮
    m_refreshButton = new QPushButton("刷新商店");
    m_refreshButton->setStyleSheet(R"(
        QPushButton {
            background-color: #26A69A;
            color: #FFFFFF;
            border: 2px solid #4DB6AC;
            border-radius: 8px;
            padding: 8px 18px;
            font-size: 14px;
            font-weight: bold;
        }
        QPushButton:hover { background-color: #4DB6AC; }
        QPushButton:disabled {
            background-color: #3a4a48;
            color: #668888;
            border-color: #446666;
        }
    )");
    bottomBar->addWidget(m_refreshButton);

    bottomBar->addSpacing(16);

    m_doneButton = new QPushButton("完成购买 (离开商店)");
    m_doneButton->setStyleSheet(R"(
        QPushButton {
            background-color: #4CAF50;
            color: #FFFFFF;
            border: 2px solid #66BB6A;
            border-radius: 8px;
            padding: 10px 28px;
            font-size: 15px;
            font-weight: bold;
        }
        QPushButton:hover { background-color: #66BB6A; }
    )");
    bottomBar->addWidget(m_doneButton);

    bottomBar->addSpacing(12);

    m_skipButton = new QPushButton("跳过商店");
    m_skipButton->setStyleSheet(R"(
        QPushButton {
            background-color: #555566;
            color: #CCCCCC;
            border: 2px solid #777788;
            border-radius: 8px;
            padding: 10px 24px;
            font-size: 14px;
        }
        QPushButton:hover { background-color: #666677; }
    )");
    bottomBar->addWidget(m_skipButton);

    bottomBar->addStretch();
    mainLayout->addLayout(bottomBar);

    connect(m_refreshButton, &QPushButton::clicked, this, &ShopDialog::onRefresh);
    connect(m_doneButton, &QPushButton::clicked, this, &QDialog::accept);
    connect(m_skipButton, &QPushButton::clicked, this, &QDialog::accept);

    updateDisplay();

    if (m_viewOnly) {
        m_refreshButton->hide();
        m_refreshCostLabel->hide();
    }
}

QWidget* ShopDialog::createItemCard(const ShopItem& item)
{
    auto* card = new QFrame;
    card->setFixedSize(200, 250);
    card->setStyleSheet(R"(
        QFrame {
            background-color: #252540;
            border: 2px solid #3a3a5e;
            border-radius: 12px;
        }
        QFrame:hover {
            border: 2px solid #FFD700;
        }
    )");

    auto* shadow = new QGraphicsDropShadowEffect(card);
    shadow->setBlurRadius(12);
    shadow->setOffset(0, 4);
    shadow->setColor(QColor(0, 0, 0, 120));
    card->setGraphicsEffect(shadow);

    auto* lay = new QVBoxLayout(card);
    lay->setContentsMargins(12, 12, 12, 12);
    lay->setSpacing(8);

    // ── 图标区域 ──
    auto* iconWidget = new QLabel;
    iconWidget->setFixedSize(kCardW, kCardH);
    iconWidget->setAlignment(Qt::AlignCenter);
    iconWidget->setScaledContents(false);

    QString cardsDir = QCoreApplication::applicationDirPath() + "/cards/";

    switch (item.type) {
        case ShopItemType::CardJokerBig: {
            QString path = cardsDir + "card_joker_red.png";
            if (QFile::exists(path)) {
                iconWidget->setPixmap(QPixmap(path).scaled(kCardW, kCardH, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            }
            break;
        }
        case ShopItemType::CardJokerSmall: {
            QString path = cardsDir + "card_joker_black.png";
            if (QFile::exists(path)) {
                iconWidget->setPixmap(QPixmap(path).scaled(kCardW, kCardH, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            }
            break;
        }
        case ShopItemType::Card5: {
            QString path = cardsDir + "card_spades_05.png";
            if (QFile::exists(path)) {
                iconWidget->setPixmap(QPixmap(path).scaled(kCardW, kCardH, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            }
            break;
        }
        case ShopItemType::Card2: {
            QString path = cardsDir + "card_spades_02.png";
            if (QFile::exists(path)) {
                iconWidget->setPixmap(QPixmap(path).scaled(kCardW, kCardH, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            }
            break;
        }
        case ShopItemType::Card3: {
            QString path = cardsDir + "card_spades_03.png";
            if (QFile::exists(path)) {
                iconWidget->setPixmap(QPixmap(path).scaled(kCardW, kCardH, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            }
            break;
        }
        case ShopItemType::CardA: {
            QString path = cardsDir + "card_spades_01.png";
            if (QFile::exists(path)) {
                iconWidget->setPixmap(QPixmap(path).scaled(kCardW, kCardH, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            }
            break;
        }
        case ShopItemType::CardK: {
            QString path = cardsDir + "card_spades_13.png";
            if (QFile::exists(path)) {
                iconWidget->setPixmap(QPixmap(path).scaled(kCardW, kCardH, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            }
            break;
        }
        case ShopItemType::CardQ: {
            QString path = cardsDir + "card_spades_12.png";
            if (QFile::exists(path)) {
                iconWidget->setPixmap(QPixmap(path).scaled(kCardW, kCardH, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            }
            break;
        }
        case ShopItemType::CardJ: {
            QString path = cardsDir + "card_spades_11.png";
            if (QFile::exists(path)) {
                iconWidget->setPixmap(QPixmap(path).scaled(kCardW, kCardH, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            }
            break;
        }
        case ShopItemType::Card10: {
            QString path = cardsDir + "card_spades_10.png";
            if (QFile::exists(path)) {
                iconWidget->setPixmap(QPixmap(path).scaled(kCardW, kCardH, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            }
            break;
        }
        case ShopItemType::Card9: {
            QString path = cardsDir + "card_spades_09.png";
            if (QFile::exists(path)) {
                iconWidget->setPixmap(QPixmap(path).scaled(kCardW, kCardH, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            }
            break;
        }
        case ShopItemType::Card8: {
            QString path = cardsDir + "card_spades_08.png";
            if (QFile::exists(path)) {
                iconWidget->setPixmap(QPixmap(path).scaled(kCardW, kCardH, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            }
            break;
        }
        case ShopItemType::Card7: {
            QString path = cardsDir + "card_spades_07.png";
            if (QFile::exists(path)) {
                iconWidget->setPixmap(QPixmap(path).scaled(kCardW, kCardH, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            }
            break;
        }
        case ShopItemType::Card6: {
            QString path = cardsDir + "card_spades_06.png";
            if (QFile::exists(path)) {
                iconWidget->setPixmap(QPixmap(path).scaled(kCardW, kCardH, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            }
            break;
        }
        case ShopItemType::Card4: {
            QString path = cardsDir + "card_spades_04.png";
            if (QFile::exists(path)) {
                iconWidget->setPixmap(QPixmap(path).scaled(kCardW, kCardH, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            }
            break;
        }
        case ShopItemType::HealSmall:
        case ShopItemType::HealLarge: {
            // 回血药水：红心图标
            iconWidget->setStyleSheet(R"(
                QLabel {
                    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                        stop:0 #FF4444, stop:1 #CC0000);
                    border: 3px solid #FF8888;
                    border-radius: 40px;
                    font-size: 36px;
                }
            )");
            iconWidget->setText(item.type == ShopItemType::HealLarge ? "♥♥" : "♥");
            break;
        }
    }

    iconWidget->setAlignment(Qt::AlignCenter);
    lay->addWidget(iconWidget, 0, Qt::AlignCenter);

    // ── 名称 ──
    auto* nameLabel = new QLabel(QString::fromStdString(item.name));
    nameLabel->setAlignment(Qt::AlignCenter);
    nameLabel->setStyleSheet("color: #FFFFFF; font-size: 15px; font-weight: bold; background: transparent;");
    lay->addWidget(nameLabel);

    // ── 描述 ──
    auto* descLabel = new QLabel(QString::fromStdString(item.desc));
    descLabel->setAlignment(Qt::AlignCenter);
    descLabel->setWordWrap(true);
    descLabel->setStyleSheet("color: #AAAACC; font-size: 11px; background: transparent;");
    lay->addWidget(descLabel);

    // ── 目标（仅卡牌物品显示） ──
    if (item.type != ShopItemType::HealSmall && item.type != ShopItemType::HealLarge) {
        QString destText;
        switch (item.dest) {
            case CardDestination::ToPlayDeck:  destText = "→ 洗入游玩牌堆"; break;
            case CardDestination::ToHiddenDeck: destText = "→ 进入隐藏牌堆"; break;
            case CardDestination::ToHand:      destText = "→ 进入下关手牌"; break;
        }
        auto* destLabel = new QLabel(destText);
        destLabel->setAlignment(Qt::AlignCenter);
        destLabel->setStyleSheet("color: #66DDAA; font-size: 10px; background: transparent;");
        lay->addWidget(destLabel);
    }

    lay->addStretch();

    // ── 购买按钮 ──
    auto* buyBtn = new QPushButton(QString("购买  %1 筹码").arg(item.price));
    buyBtn->setStyleSheet(R"(
        QPushButton {
            background-color: #E8A840;
            color: #1a1a2e;
            border: none;
            border-radius: 6px;
            padding: 8px 16px;
            font-size: 14px;
            font-weight: bold;
        }
        QPushButton:hover { background-color: #FFC050; }
        QPushButton:disabled {
            background-color: #555566;
            color: #888899;
        }
    )");

    ShopItemType type = item.type;
    QObject::connect(buyBtn, &QPushButton::clicked, this, [this, type]() {
        onBuy(type);
    });

    // 存储按钮指针以便后续禁用
    buyBtn->setProperty("itemType", static_cast<int>(type));
    lay->addWidget(buyBtn);

    return card;
}

void ShopDialog::onBuy(ShopItemType type)
{
    if (m_viewOnly) {
        QMessageBox::information(this, "提示", "对局进行中无法购买，请关卡结算后再来！");
        return;
    }

    ShopPurchaseResult result = m_shop.buy(type, m_currentChips, m_currentHp, m_maxHp);

    if (!result.success) {
        QMessageBox::information(this, "购买失败", QString::fromStdString(result.message));
        return;
    }

    int price = Shop::basePrice(type);
    m_currentChips -= price;
    m_chipsSpent += price;

    if (result.hpHealed > 0) {
        m_currentHp = std::min(m_currentHp + result.hpHealed, m_maxHp);
        m_hpHealed += result.hpHealed;
    }

    if (!result.card.point.empty()) {
        m_purchasedCards.push_back(result.card);
        m_purchasedDests.push_back(result.dest);
    }

    QMessageBox::information(this, "购买成功", QString::fromStdString(result.message));
    updateDisplay();  // 内部已更新所有按钮状态、筹码/HP/刷新费用
}

void ShopDialog::updateDisplay()
{
    // 刷新费用 = 2^(refreshCount+1)
    int refreshCost = 1 << (m_refreshCount + 1);

    m_chipsLabel->setText(QString(" 筹码: %1").arg(m_currentChips));
    int hpPercent = m_currentHp * 100 / m_maxHp;
    m_hpLabel->setText(QString(" HP: %1/%2 (%3%)").arg(m_currentHp).arg(m_maxHp).arg(hpPercent));

    // 刷新按钮状态
    if (m_currentChips < refreshCost) {
        m_refreshButton->setEnabled(false);
        m_refreshCostLabel->setText(QString("刷新需 %1 筹码 (不足)").arg(refreshCost));
    } else {
        m_refreshButton->setEnabled(true);
        m_refreshCostLabel->setText(QString("刷新需 %1 筹码").arg(refreshCost));
    }

    // 筹码不够或售罄时禁用对应按钮
    auto buttons = findChildren<QPushButton*>();
    for (auto* btn : buttons) {
        bool ok = false;
        int t = btn->property("itemType").toInt(&ok);
        if (ok) {
            auto tp = static_cast<ShopItemType>(t);
            int p = Shop::basePrice(tp);
            int stock = m_shop.stockOf(tp);
            btn->setEnabled(m_currentChips >= p && stock != 0);
            if (stock == 0) btn->setText("售罄");
        }
    }
}

void ShopDialog::onRefresh()
{
    if (m_viewOnly) {
        QMessageBox::information(this, "提示", "对局中无法刷新商店！");
        return;
    }
    int refreshCost = 1 << (m_refreshCount + 1);
    if (m_currentChips < refreshCost) {
        QMessageBox::information(this, "刷新失败", "筹码不足，无法刷新商店！");
        return;
    }

    m_currentChips -= refreshCost;
    m_chipsSpent += refreshCost;
    m_refreshCount++;

    m_shop.resetStock();
    rebuildItemGrid();
    updateDisplay();

    QMessageBox::information(this, "刷新成功",
        QString("商店已刷新！消耗 %1 筹码\n下次刷新需要 %2 筹码")
            .arg(refreshCost)
            .arg(1 << (m_refreshCount + 1)));
}

void ShopDialog::rebuildItemGrid()
{
    QLayoutItem* item;
    while ((item = m_itemGrid->takeAt(0)) != nullptr) {
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }

    const auto& items = m_shop.items();
    for (size_t i = 0; i < items.size(); ++i) {
        int row = static_cast<int>(i) / 3;
        int col = static_cast<int>(i) % 3;
        m_itemGrid->addWidget(createItemCard(items[i]), row, col);
    }
}