#include "battle_screen.h"
#include "ui/widgets/card_widget.h"
#include "core/card/cardtype.h"
#include "core/variant/variant_registry.h"
#include <QPropertyAnimation>
#include <QParallelAnimationGroup>
#include <QPainterPath>
#include <QRandomGenerator>
#include <QLineF>
#include <QTimer>
#include <QVBoxLayout>
#include <QGraphicsOpacityEffect>
#include <QGraphicsDropShadowEffect>
#include <QCoreApplication>
#include <QFile>
#include <QLabel>
#include <QPixmap>
#include <random>

void BattleScreen::playDealAnimation()
{
    if (!m_playerDeckWidget || !m_playerAHandWidget || !m_playerBHandWidget) {
        emit dealAnimationFinished();
        return;
    }
    m_dealAnimating = true;

    // 1) 清空旧手牌 widget
    for (CardWidget* cw : m_playerACardWidgets) { cw->deleteLater(); }
    m_playerACardWidgets.clear();
    for (QWidget* w : m_playerBCardWidgets) { w->deleteLater(); }
    m_playerBCardWidgets.clear();

    // 发牌起点
    QPoint playerDeckPos = m_playerDeckWidget->mapTo(this, QPoint(0, 0));
    QPoint startPosPlayer(playerDeckPos.x() + m_playerDeckWidget->width() / 2,
                          playerDeckPos.y() + m_playerDeckWidget->height() / 2);
    QPoint bossDeckPos = m_bossDeckWidget->mapTo(this, QPoint(0, 0));
    QPoint startPosBoss(bossDeckPos.x() + m_bossDeckWidget->width() / 2,
                        bossDeckPos.y() + m_bossDeckWidget->height() / 2);

    const int delayPerCard = 70;
    const int flyDuration = 400;

    for (int i = 0; i < 2 * kMaxHandSize; ++i) {
        bool toA = (i % 2 == 0);
        const size_t handIdx = static_cast<size_t>(i / 2);
        const Card dealtCard = (toA && handIdx < m_playerA.hand.size())
            ? m_playerA.hand[handIdx]
            : Card{};

        QTimer::singleShot(i * delayPerCard, this, [=]() {
            // 2) 创建真实手牌 widget
            CardWidget* card = new CardWidget(dealtCard, this);
            card->setAttribute(Qt::WA_TransparentForMouseEvents);
            card->setDealAnimationEnabled(true, toA);
            card->setDealTilt(QRandomGenerator::global()->generateDouble() * 20.0 - 10.0);

            const int cw = card->width();
            const int ch = card->height();
            QPoint start = toA ? startPosPlayer : startPosBoss;
            QPoint from = start - QPoint(cw / 2, ch / 2);

            // 目标 = 槽位坐标（相对 this）
            QWidget* handWidget = toA ? m_playerAHandWidget : m_playerBHandWidget;
            QPoint slotRel = handSlotPos(handWidget, handIdx);
            QPoint target = handWidget->mapTo(this, slotRel);
            QPoint to = target;

            card->move(from);
            card->show();
            card->raise();

            auto* flight = new QParallelAnimationGroup(card);
            auto* anim = new QPropertyAnimation(card, "pos", flight);
            anim->setDuration(flyDuration);
            anim->setEasingCurve(QEasingCurve::Linear);
            anim->setStartValue(from);
            anim->setEndValue(to);

            const QPointF fromF(from);
            const QPointF toF(to);
            const qreal distance = QLineF(fromF, toF).length();
            const QPointF direction = distance > 0.0
                ? (toF - fromF) / distance : QPointF(0.0, 1.0);
            const QPointF overshoot = toF + direction * 5.0;
            const QPointF control = (fromF + toF) / 2.0 + QPointF(toA ? -24.0 : 24.0, -55.0);
            QPainterPath arc(fromF);
            arc.quadTo(control, overshoot);
            for (int frame = 1; frame <= 8; ++frame) {
                const qreal phase = static_cast<qreal>(frame) / 8.0;
                const qreal eased = 1.0 - (1.0 - phase) * (1.0 - phase);
                anim->setKeyValueAt(0.85 * phase, arc.pointAtPercent(eased).toPoint());
            }
            anim->setKeyValueAt(0.925, (toF + direction * 2.0).toPoint());
            auto* flip = new QPropertyAnimation(card, "dealProgress", flight);
            flip->setDuration(flyDuration);
            flip->setEasingCurve(QEasingCurve::Linear);
            flip->setStartValue(0.0);
            flip->setEndValue(1.0);

            // 3) 动画结束后：直接设在槽位，不进 layout
            connect(flight, &QParallelAnimationGroup::finished, this,
                [this, card, handIdx, toA, target]() {
                    card->setAttribute(Qt::WA_TransparentForMouseEvents, false);
                    card->setDealAnimationEnabled(false, false);
                    card->setFlying(false);

                    QWidget* handWidget = toA ? m_playerAHandWidget : m_playerBHandWidget;
                    QPoint relPos = target - handWidget->mapTo(this, QPoint(0, 0));
                    card->setParent(handWidget);
                    card->move(relPos);
                    card->show();

                    if (toA) {
                        // 玩家侧：保留 CardWidget
                        connect(card, &CardWidget::clicked, this, [this]() { update(); });
                        m_playerACardWidgets.push_back(card);
                    } else {
                        // Boss 侧：CardWidget → 替换为卡背 QLabel
                        card->hide();
                        card->deleteLater();
                        QWidget* back = createCardBack();
                        back->setParent(handWidget);
                        back->move(relPos);
                        back->show();
                        m_playerBCardWidgets.push_back(back);
                    }
                });

            if (i == 2 * kMaxHandSize - 1) {
                connect(flight, &QParallelAnimationGroup::finished,
                        this, &BattleScreen::dealAnimationFinished);
            }
            flight->start(QAbstractAnimation::DeleteWhenStopped);
        });
    }
}

void BattleScreen::flyCardsToTable(const std::vector<CardWidget*>& cards) {
    if (cards.empty()) return;

    int n = (int)cards.size();
    int cardW = 100;
    int spacing = 20;
    int totalWidth = n * cardW + (n - 1) * spacing;
    int tableW = m_tableCardsWidget->width();
    int tableH = m_tableCardsWidget->height();

    QPoint tableOrigin = m_tableCardsWidget->mapTo(this, QPoint(0, 0));
    int startX = tableOrigin.x() + (tableW - totalWidth) / 2;
    int targetY = tableOrigin.y() + (tableH - 150) / 2;

    for (int i = 0; i < n; ++i) {
        CardWidget* cw = cards[i];

        QPoint startPos = cw->mapTo(this, QPoint(0, 0));

        cw->setParent(this);
        cw->move(startPos);
        cw->show();
        cw->raise();
        cw->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        cw->setFlying(true);

        QPoint endPos(startX + i * (cardW + spacing), targetY);

        QPropertyAnimation* anim = new QPropertyAnimation(cw, "pos");
        anim->setDuration(400);
        anim->setEasingCurve(QEasingCurve::OutCubic);
        anim->setStartValue(startPos);
        anim->setEndValue(endPos);

        connect(anim, &QPropertyAnimation::finished, this,
            [this, cw, endPos]() {
                cw->setParent(m_tableCardsWidget);
                QPoint relPos = endPos - m_tableCardsWidget->mapTo(this, QPoint(0, 0));
                cw->move(relPos);
                cw->show();
                cw->setFlying(false);
                cw->setAttribute(Qt::WA_TransparentForMouseEvents, true);
                m_tableCardWidgets.push_back(cw);
            });

        anim->start(QAbstractAnimation::DeleteWhenStopped);
    }
}

void BattleScreen::flyAICardsToTable(const std::vector<Card>& cards) {
    if (cards.empty()) return;
    if (!m_playerBHandWidget || !m_tableCardsWidget) return;

    QWidget* playArea = m_tableCardsWidget->parentWidget();
    QPoint playPos = playArea->mapTo(this, QPoint(0, 0));
    QPoint aiPos = m_playerBHandWidget->mapTo(this, QPoint(0, 0));
    QPoint startPos(playPos.x() + playArea->width() / 2,
                    aiPos.y() + m_playerBHandWidget->height() / 2);

    QPoint tablePos = m_tableCardsWidget->mapTo(this, QPoint(0, 0));
    int tableW = m_tableCardsWidget->width();
    int tableH = m_tableCardsWidget->height();

    int n = (int)cards.size();
    int cardW = 100;
    int spacing = 20;
    int totalWidth = n * cardW + (n - 1) * spacing;
    int startX = tablePos.x() + (tableW - totalWidth) / 2;
    int targetY = tablePos.y() + (tableH - 150) / 2;

    for (int i = 0; i < n; ++i) {
        Card displayCard = cards[i];
        bool hasVariant = VariantRegistry::has(cards[i].seq);
        if (hasVariant) {
            displayCard = VariantRegistry::lookup(cards[i].seq);
        }
        CardWidget* cw = new CardWidget(displayCard, this);
        cw->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        cw->move(startPos);
        cw->show();
        cw->raise();
        cw->setFlying(true);

        QPoint endPos(startX + i * (cardW + spacing), targetY);

        QPropertyAnimation* anim = new QPropertyAnimation(cw, "pos");
        anim->setDuration(400);
        anim->setEasingCurve(QEasingCurve::OutCubic);
        anim->setStartValue(startPos);
        anim->setEndValue(endPos);

        Card variantCardData = cards[i];
        connect(anim, &QPropertyAnimation::finished, this,
            [this, cw, endPos, hasVariant, variantCardData]() {
                cw->setParent(m_tableCardsWidget);
                QPoint relPos = endPos - m_tableCardsWidget->mapTo(this, QPoint(0, 0));
                cw->move(relPos);
                cw->show();
                cw->setFlying(false);
                cw->setAttribute(Qt::WA_TransparentForMouseEvents, true);
                m_tableCardWidgets.push_back(cw);

                if (hasVariant) {
                    shakeWidget(cw);
                    QTimer::singleShot(250, this, [cw, variantCardData]() {
                        cw->setCard(variantCardData);
                    });
                }
            });

        anim->start(QAbstractAnimation::DeleteWhenStopped);
    }
}

void BattleScreen::shakeWidget(QWidget* widget)
{
    if (!widget || m_shaking) return;
    m_shaking = true;

    QPoint orig = widget->pos();
    QTimer* timer = new QTimer(this);
    int* counter = new int(0);

    connect(timer, &QTimer::timeout, this, [=]() mutable {
        if (*counter >= 8) {
            timer->stop();
            widget->move(orig);
            delete counter;
            timer->deleteLater();
            m_shaking = false;
            return;
        }
        int dx = ((*counter % 2 == 0) ? 8 : -8);
        widget->move(orig.x() + dx, orig.y());
        (*counter)++;
    });
    timer->start(30);
}

void BattleScreen::playDrawAnimationSimple(bool forPlayerA, const QPoint& targetPos)
{
    // 从对应牌堆位置开始动画（简化版：只用 QLabel 卡背，不重建手牌）
    QWidget* deckSource = forPlayerA ? m_playerDeckWidget : m_bossDeckWidget;
    if (!deckSource) return;

    QPoint deckPos = deckSource->mapTo(this, QPoint(0, 0));
    QPoint startPos(deckPos.x() + deckSource->width() / 2 - 48,
                    deckPos.y() + deckSource->height() / 2 - 60);

    const int cw = 96, ch = 120;
    QWidget* flyingCard = new QLabel(this);
    flyingCard->setFixedSize(cw, ch);
    flyingCard->setAttribute(Qt::WA_TransparentForMouseEvents);

    QString backPath = QCoreApplication::applicationDirPath() + "/cards/card_back.png";
    static QPixmap cachedBack;
    if (cachedBack.isNull())
        cachedBack.load(backPath);
    static_cast<QLabel*>(flyingCard)->setPixmap(cachedBack.scaled(cw, ch, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    static_cast<QLabel*>(flyingCard)->setScaledContents(true);

    flyingCard->move(startPos);
    flyingCard->show();
    flyingCard->raise();

    auto* posAnim = new QPropertyAnimation(flyingCard, "pos");
    posAnim->setDuration(450);
    posAnim->setEasingCurve(QEasingCurve::OutCubic);
    posAnim->setStartValue(startPos);
    posAnim->setEndValue(targetPos);
    connect(posAnim, &QPropertyAnimation::finished, this, [flyingCard]() {
        flyingCard->deleteLater();
    });
    posAnim->start(QAbstractAnimation::DeleteWhenStopped);
}

void BattleScreen::showSpecialVictoryEffect(const QString& winnerName, const QString& endMessage)
{
    m_gameOver = true;
    disableActionButtons();

    QWidget* glow = new QWidget(this);
    glow->setAttribute(Qt::WA_TransparentForMouseEvents);
    glow->setGeometry(rect());
    glow->setStyleSheet("background-color: rgba(255, 215, 0, 50);");
    glow->show();
    glow->raise();

    QTimer* pulseTimer = new QTimer(this);
    int* pulseCnt = new int(0);
    connect(pulseTimer, &QTimer::timeout, this, [glow, pulseCnt]() {
        (*pulseCnt)++;
        int a = (*pulseCnt) % 2 == 0 ? 30 : 70;
        glow->setStyleSheet(
            QString("background-color: rgba(255, 215, 0, %1);").arg(a));
    });
    pulseTimer->start(300);

    for (CardWidget* cw : m_tableCardWidgets) {
        QRect g = cw->geometry();
        QRect big(g.x() - g.width() / 4, g.y() - g.height() / 4,
                  g.width() * 3 / 2, g.height() * 3 / 2);
        QPropertyAnimation* a = new QPropertyAnimation(cw, "geometry");
        a->setDuration(400);
        a->setEasingCurve(QEasingCurve::OutBack);
        a->setEndValue(big);
        a->start(QAbstractAnimation::DeleteWhenStopped);
    }

    QLabel* bigText = new QLabel("\u4e03\u9b3c523\uff01", this);
    bigText->setAlignment(Qt::AlignCenter);
    bigText->setStyleSheet(
        "QLabel { color: #FFD700; font-size: 64px; font-weight: bold; background: transparent; }");
    bigText->setAttribute(Qt::WA_TransparentForMouseEvents);
    bigText->setGeometry(0, height() / 3, width(), 100);

    QGraphicsDropShadowEffect* shadow = new QGraphicsDropShadowEffect;
    shadow->setBlurRadius(20);
    shadow->setColor(QColor(0, 0, 0, 200));
    shadow->setOffset(4, 4);
    bigText->setGraphicsEffect(shadow);
    bigText->show();
    bigText->raise();

    bigText->resize(0, 0);
    QPropertyAnimation* textPop = new QPropertyAnimation(bigText, "geometry");
    textPop->setDuration(500);
    textPop->setEasingCurve(QEasingCurve::OutElastic);
    textPop->setStartValue(QRect(width() / 2, height() / 3, 0, 0));
    textPop->setEndValue(QRect(0, height() / 3, width(), 100));
    textPop->start(QAbstractAnimation::DeleteWhenStopped);

    QTimer::singleShot(1500, this, [this, endMessage, glow, bigText, pulseTimer, pulseCnt, shadow]() {
        pulseTimer->stop();
        delete pulseTimer;
        delete pulseCnt;
        glow->deleteLater();
        bigText->deleteLater();
        shadow->deleteLater();
        showGameOverDialog(endMessage);
    });
}

void BattleScreen::showBonusFloat(int bonus) {
    if (bonus <= 0) return;
    if (!m_tableScoreLabel) return;

    QLabel* floatLabel = new QLabel(QString("+%1").arg(bonus), this);
    floatLabel->setStyleSheet(
        "QLabel {"
        "  color: #FFD700;"
        "  font-size: 32px;"
        "  font-weight: bold;"
        "  background: transparent;"
        "}"
    );
    floatLabel->setAlignment(Qt::AlignCenter);
    floatLabel->adjustSize();

    QPoint labelPos = m_tableScoreLabel->mapTo(this, QPoint(0, 0));
    int x = labelPos.x() + m_tableScoreLabel->width() / 2 - floatLabel->width() / 2;
    int y = labelPos.y() - floatLabel->height() - 5;
    floatLabel->move(x, y);
    floatLabel->show();
    floatLabel->raise();

    QPropertyAnimation* moveAnim = new QPropertyAnimation(floatLabel, "pos");
    moveAnim->setDuration(1000);
    moveAnim->setStartValue(QPoint(x, y));
    moveAnim->setEndValue(QPoint(x, y - 80));
    moveAnim->setEasingCurve(QEasingCurve::OutCubic);

    QGraphicsOpacityEffect* effect = new QGraphicsOpacityEffect(floatLabel);
    floatLabel->setGraphicsEffect(effect);
    QPropertyAnimation* fadeAnim = new QPropertyAnimation(effect, "opacity");
    fadeAnim->setDuration(1000);
    fadeAnim->setStartValue(1.0);
    fadeAnim->setEndValue(0.0);

    connect(moveAnim, &QPropertyAnimation::finished, floatLabel, &QLabel::deleteLater);
    moveAnim->start(QAbstractAnimation::DeleteWhenStopped);
    fadeAnim->start(QAbstractAnimation::DeleteWhenStopped);
}

void BattleScreen::showHpDamageFloat(int damage, bool toBoss) {
    if (damage <= 0) return;
    if (!m_hpLabel || !m_hpLabel->isVisible()) return;

    QString text = toBoss ? QString("Boss -%1").arg(damage)
                          : QString("玩家 -%1").arg(damage);

    QLabel* floatLabel = new QLabel(text, this);
    floatLabel->setStyleSheet(
        "QLabel {"
        "  color: #E8503A;"
        "  font-size: 24px;"
        "  font-weight: bold;"
        "  background: transparent;"
        "}"
    );
    floatLabel->setAlignment(Qt::AlignCenter);
    floatLabel->adjustSize();

    QPoint labelPos = m_hpLabel->mapTo(this, QPoint(0, 0));
    int x = labelPos.x() + m_hpLabel->width() / 2 - floatLabel->width() / 2;
    int y = labelPos.y() - floatLabel->height() - 5;
    floatLabel->move(x, y);
    floatLabel->show();
    floatLabel->raise();

    QPropertyAnimation* moveAnim = new QPropertyAnimation(floatLabel, "pos");
    moveAnim->setDuration(1200);
    moveAnim->setStartValue(QPoint(x, y));
    moveAnim->setEndValue(QPoint(x, y - 60));
    moveAnim->setEasingCurve(QEasingCurve::OutCubic);

    QGraphicsOpacityEffect* effect = new QGraphicsOpacityEffect(floatLabel);
    floatLabel->setGraphicsEffect(effect);
    QPropertyAnimation* fadeAnim = new QPropertyAnimation(effect, "opacity");
    fadeAnim->setDuration(1200);
    fadeAnim->setStartValue(1.0);
    fadeAnim->setEndValue(0.0);

    connect(moveAnim, &QPropertyAnimation::finished, floatLabel, &QLabel::deleteLater);
    moveAnim->start(QAbstractAnimation::DeleteWhenStopped);
    fadeAnim->start(QAbstractAnimation::DeleteWhenStopped);
}