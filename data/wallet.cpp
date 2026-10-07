#include "wallet.h"

Wallet::Wallet() = default;
Wallet::Wallet(int initialBalance) : m_balance(initialBalance) {}

void Wallet::add(int amount) {
    m_balance += amount;
}

bool Wallet::spend(int amount) {
    if (m_balance < amount) return false;
    m_balance -= amount;
    return true;
}

int Wallet::balance() const {
    return m_balance;
}