#pragma once
#include <QDialog>
#include <QPushButton>
#include <QLabel>
#include <QGridLayout>
#include <QScrollArea>
#include <vector>
#include "shop/shop.h"
#include "core/card/card.h"

class ShopDialog : public QDialog {
    Q_OBJECT
public:
    explicit ShopDialog(int currentChips, int currentHp, int maxHp, bool viewOnly = false, QWidget* parent = nullptr);

    int  chipsSpent() const { return m_chipsSpent; }
    int  hpHealed() const { return m_hpHealed; }
    std::vector<Card> purchasedCards() const { return m_purchasedCards; }
    std::vector<CardDestination> purchasedDests() const { return m_purchasedDests; }

private slots:
    void onBuy(ShopItemType type);
    void onRefresh();

private:
    void buildUI();
    void rebuildItemGrid();
    void updateDisplay();
    QWidget* createItemCard(const ShopItem& item);

    Shop m_shop;
    int  m_currentChips;
    int  m_currentHp;
    int  m_maxHp;
    bool m_viewOnly = false;
    int  m_chipsSpent = 0;
    int  m_hpHealed   = 0;
    int  m_refreshCount = 0;  // 本店已刷新次数，决定下次刷新费用 2^(n+1)
    std::vector<Card> m_purchasedCards;
    std::vector<CardDestination> m_purchasedDests;

    QLabel*       m_chipsLabel     = nullptr;
    QLabel*       m_hpLabel        = nullptr;
    QLabel*       m_refreshCostLabel = nullptr;
    QPushButton*  m_refreshButton  = nullptr;
    QPushButton*  m_doneButton     = nullptr;
    QPushButton*  m_skipButton     = nullptr;
    QScrollArea*  m_scroll         = nullptr;
    QGridLayout*  m_itemGrid       = nullptr;
};