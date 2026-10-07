#pragma once
#include <vector>

class Progress {
public:
    Progress() = default;

    void unlockLevel(int level);
    bool isUnlocked(int level) const;
    int currentLevel() const;

private:
    std::vector<bool> m_unlocked;
};