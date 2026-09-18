#pragma once
#include <string>
#include <vector>

struct SaveData {
    int version = 1;
    std::string playerName;
    int totalScore = 0;
    int currency = 0;
    std::vector<int> unlockedLevels;
    std::vector<std::string> inventory;
    std::vector<std::string> codexEntries;
    std::string dialogueState;
    int aiLevel = 0;
};