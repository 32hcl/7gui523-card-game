#include "codex.h"
#include <algorithm>

void Codex::unlock(const std::string& entryId) {
    if (!has(entryId)) {
        m_unlocked.push_back(entryId);
    }
}

bool Codex::has(const std::string& entryId) const {
    return std::find(m_unlocked.begin(), m_unlocked.end(), entryId) != m_unlocked.end();
}

std::vector<std::string> Codex::unlockedEntries() const {
    return m_unlocked;
}