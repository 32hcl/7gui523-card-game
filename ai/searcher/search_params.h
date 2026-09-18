#pragma once

struct SearchParams {
    int scoreDiffWeight = 10;
    int tableScoreWeight = 8;
    int handRankWeight = 2;
    int handScoreWeight = 3;
    int handSizeWeight = 20;
    int special523Weight = 0;
    int pressureBonusWeight = 0;

    int searchDepth = 6;
};