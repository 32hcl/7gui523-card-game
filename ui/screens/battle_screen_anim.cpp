#include "battle_screen.h"
#include "ui/widgets/card_widget.h"
#include "core/card/cardtype.h"
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
    if (!m_deckBackLabel || !m_playerAHandWidget || !m_playerBHandWidget) {
        emit dealAnimationFinished();
        return;
    }
    m_dealAnimating = true;

    QPoint deckPos = m_deckBackLabel->mapTo(this, QPoint(0, 0));
    QPoint startPos(deckPos.x() + m_deckBackLabel->width() / 2,
                    deckPos.y() + m_deckBackLabel->height() / 2);

    QPoint aPos = m_playerAHandWidget->mapTo(this, QPoint(0, 0));
    QPoint bPos = m_playerBHandWidget->mapTo(this, QPoint(0, 0));

    const int cw = 110, ch = 178;
    const int delayPerCard = 70;
    const int flyDuration = 400;

    for (int i = 0; i < 10; ++i) {
        bool toA = (i % 2 == 0);
        int slot = i / 2;
        QPoint target;
        if (toA) {
            target = QPoint(aPos.x() + slot * (cw + 6) + cw / 2,
                            aPos.y() + ch / 2);
        } else {
            target = QPoint(bPos.x() + m_playerBHandWidget->width() / 2,
                            bPos.y() + m_playerBHandWidget->height() / 2);
        }

        QTimer::singleShot(i * delayPerCard, this, [=]() {
            CardWidget* card = toA
                ? new CardWidget(m_playerA.hand.at(slot), this)
                : new CardWidget(Card{}, this);
            card->setFixedSize(cw, ch);
            card->setDealAnimationEnabled(true, toA);
            card->setDealTilt(QRandomGenerator::global()->generateDouble() * 20.0 - 10.0);
            card->setAttribute(Qt::WA_TransparentForMouseEvents);

            QPoint from = startPos - QPoint(cw / 2, ch / 2);
            QPoint to   = target   - QPoint(cw / 2, ch / 2);

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
            for (int frame = 1; frame <= 20; ++frame) {
                const qreal phase = static_cast<qreal>(frame) / 20.0;
                const qreal eased = 1.0 - (1.0 - phase) * (1.0 - phase);
                anim->setKeyValueAt(0.85 * phase, arc.pointAtPercent(eased).toPoint());
            }
            anim->setKeyValueAt(0.925, (toF + direction * 2.0).toPoint());
            auto* flip = new QPropertyAnimation(card, "dealProgress", flight);
            flip->setDuration(flyDuration);
            flip->setEasingCurve(QEasingCurve::Linear);
            flip->setStartValue(0.0);
            flip->setEndValue(1.0);

            if (toA) {
                connect(flight, &QParallelAnimationGroup::finished, this, [this, card]() {
                    card->setDealAnimationEnabled(false);
                    card->setFaceUp(true);
                    card->setAttribute(Qt::WA_TransparentForMouseEvents, false);
                    int insertIdx = m_playerALayout->count();
                    if (insertIdx > 0 && m_playerALayout->itemAt(insertIdx - 1)->spacerItem())
                        insertIdx--;
                    m_playerALayout->insertWidget(insertIdx, card);
                    m_playerACardWidgets.push_back(card);
                });
            } else {
                connect(flight, &QParallelAnimationGroup::finished, card, &QObject::deleteLater);
            }

            if (i == 9) {
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
                cw->resetToIdle();
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
        CardWidget* cw = new CardWidget(cards[i], this);
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

        connect(anim, &QPropertyAnimation::finished, this,
            [this, cw, endPos]() {
                cw->setParent(m_tableCardsWidget);
                QPoint relPos = endPos - m_tableCardsWidget->mapTo(this, QPoint(0, 0));
                cw->move(relPos);
                cw->resetToIdle();
                cw->show();
                cw->setFlying(false);
                cw->setAttribute(Qt::WA_TransparentForMouseEvents, true);
                m_tableCardWidgets.push_back(cw);
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