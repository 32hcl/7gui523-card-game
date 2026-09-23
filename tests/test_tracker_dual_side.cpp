#include "core/tracker/cardtracker.h"
#include "core/card/card.h"
#include <iostream>
#include <cassert>
#include <string>

void test_trackerDualSide() {
    CardTracker tracker;

    // 初始状态：两侧都是干净的
    assert(tracker.playedCount("7", DeckSide::PlayerA) == 0);
    assert(tracker.playedCount("7", DeckSide::Boss) == 0);
    assert(tracker.remainingCount("7", DeckSide::PlayerA) == 4);
    assert(tracker.remainingCount("7", DeckSide::Boss) == 4);

    // PlayerA 打出一张 7
    std::vector<Card> playerA_play = {
        Card{"7", "黑桃", 7},
        Card{"7", "红桃", 7}
    };
    tracker.recordPlayed(playerA_play, DeckSide::PlayerA);

    // 验证：PlayerA 侧记录，Boss 侧不受影响
    assert(tracker.playedCount("7", DeckSide::PlayerA) == 2);
    assert(tracker.playedCount("7", DeckSide::Boss) == 0);
    assert(tracker.remainingCount("7", DeckSide::PlayerA) == 2);
    assert(tracker.remainingCount("7", DeckSide::Boss) == 4);

    // Boss 打出一张 K
    std::vector<Card> boss_play = {
        Card{"K", "梅花", 13}
    };
    tracker.recordPlayed(boss_play, DeckSide::Boss);

    // 验证：Boss 侧记录，PlayerA 侧不受影响
    assert(tracker.playedCount("K", DeckSide::Boss) == 1);
    assert(tracker.playedCount("K", DeckSide::PlayerA) == 0);
    assert(tracker.remainingCount("K", DeckSide::Boss) == 3);
    assert(tracker.remainingCount("K", DeckSide::PlayerA) == 4);

    // 验证 expectedOpponentCount（以 PlayerA 视角）
    // PlayerA 想知道 Boss 手里可能有多少个 7
    // oppSide = Boss, unknown = Boss 侧剩余 7 = 4
    // oppHandSize=6, oppDeckSize=21 -> total=27, expected = 4 * 6 / 27 ≈ 0.89
    double e = tracker.expectedOpponentCount("7", 6, 21, DeckSide::PlayerA);
    assert(e > 0.88 && e < 0.90);

    // 以 Boss 视角
    // Boss 想知道 PlayerA 手里可能有多少个 7
    // oppSide = PlayerA, unknown = PlayerA 侧剩余 7 = 2
    // oppHandSize=5, oppDeckSize=22 -> total=27, expected = 2 * 5 / 27 ≈ 0.37
    double e2 = tracker.expectedOpponentCount("7", 5, 22, DeckSide::Boss);
    assert(e2 > 0.36 && e2 < 0.38);

    // 验证 isExhausted 分侧
    assert(!tracker.isExhausted("7", DeckSide::PlayerA)); // 还剩2张
    assert(!tracker.isExhausted("7", DeckSide::Boss));    // 还剩4张

    // PlayerA 打光所有 7
    std::vector<Card> more7 = {
        Card{"7", "方块", 7},
        Card{"7", "梅花", 7}
    };
    tracker.recordPlayed(more7, DeckSide::PlayerA);
    assert(tracker.playedCount("7", DeckSide::PlayerA) == 4);
    assert(tracker.isExhausted("7", DeckSide::PlayerA));
    assert(!tracker.isExhausted("7", DeckSide::Boss)); // Boss 还能有 7

    // 验证向后兼容（默认 PlayerA）
    assert(tracker.playedCount("7") == 4);  // 默认 PlayerA
    assert(tracker.remainingCount("7") == 0);

    std::cout << "OK" << std::endl;
}