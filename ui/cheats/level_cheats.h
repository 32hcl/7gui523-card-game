#pragma once
#include <functional>
#include <map>
#include <vector>

class BattleScreen;

enum class CheatWhen {
    BeforeDeal,
    AfterDeal,
    RoundStart
};

struct CheatEntry {
    CheatWhen when;
    std::function<void(BattleScreen*)> action;
};

const std::map<int, std::vector<CheatEntry>>& getCheatTable();