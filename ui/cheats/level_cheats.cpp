#include "ui/cheats/level_cheats.h"
#include "ui/screens/battle_screen.h"

static std::map<int, std::vector<CheatEntry>> buildCheatTable()
{
    std::map<int, std::vector<CheatEntry>> table;

    table[2].push_back({
        CheatWhen::BeforeDeal,
        [](BattleScreen* screen) { screen->cheat_loadVariantDeck(); }
    });

    table[3].push_back({
        CheatWhen::RoundStart,
        [](BattleScreen* screen) { screen->cheat_forceFirstHand(false); }
    });

    return table;
}

const std::map<int, std::vector<CheatEntry>>& getCheatTable()
{
    static auto table = buildCheatTable();
    return table;
}