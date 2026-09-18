#pragma once

class Wallet {
public:
    Wallet();
    explicit Wallet(int initialBalance);

    void add(int amount);
    bool spend(int amount);
    int balance() const;

private:
    int m_balance = 0;
};