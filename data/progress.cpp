#include "progress.h"
#include <algorithm>

void Progress::unlockLevel(int level) {
    if (level >= (int)m_unlocked.size()) {
        m_unlocked.resize(level + 1, false);
    }
    m_unlocked[level] = true;
}

bool Progress::isUnlocked(int level) const {
    if (level < 0 || level >= (int)m_unlocked.size()) return false;
    return m_unlocked[level];
}

int Progress::currentLevel() const {
    for (int i = 0; i < (int)m_unlocked.size(); ++i) {
        if (!m_unlocked[i]) return i;
    }
    return (int)m_unlocked.size();
}