#pragma once
#include "ai/observation.h"
#include <cstdint>
struct EndgameResult {std::vector<Card> action;bool solved=false;std::uint64_t nodes=0;int value=0;};
EndgameResult solveEndgame(const Observation&,int budgetMs=900);
