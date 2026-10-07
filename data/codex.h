#pragma once
#include <string>
#include <vector>

class Codex {
public:
    Codex() = default;

    void unlock(const std::string& entryId);
    bool has(const std::string& entryId) const;
    std::vector<std::string> unlockedEntries() const;

private:
    std::vector<std::string> m_unlocked;
};