#include "ai/cfr/hand_encoder.h"
#include "ai/cfr/infoset.h"
#include "ai/cfr/terminal.h"
#include "ai/cfr/cfr_types.h"
#include "ai/cfr/endgame_db.h"
#include "ai/cfr/cfr_trainer.h"
#include "ai/cfr/strategy.h"
#include "core/card/cardtype.h"
#include <iostream>
#include <cassert>
#include <set>
#include <random>

static void test_hand_encoder_basic() {
    auto empty = encodeHand({});
    assert(empty == 0);
    auto decoded = decodeHand(empty);
    assert(decoded.empty());

    auto single4 = encodeHand({"4"});
    assert(single4 > 0 && single4 < 15504);
    auto dec4 = decodeHand(single4);
    assert(dec4.size() == 1 && dec4[0] == "4");

    auto pair7 = encodeHand({"7", "7"});
    auto dec7 = decodeHand(pair7);
    assert(dec7.size() == 2);

    std::cout << "[PASS] test_hand_encoder_basic" << std::endl;
}

static void test_hand_encoder_bijection() {
    std::set<uint16_t> seen;
    for (uint16_t code = 0; code < 15504; ++code) {
        auto hand = decodeHand(code);
        assert(hand.size() <= 5);
        uint16_t back = encodeHand(hand);
        assert(back == code);
        assert(seen.insert(code).second);
    }
    std::cout << "[PASS] test_hand_encoder_bijection (" << seen.size() << " codes)" << std::endl;
}

static void test_hand_encoder_size_distribution() {
    int countBySize[6] = {};
    for (uint16_t code = 0; code < 15504; ++code) {
        countBySize[decodeHand(code).size()]++;
    }
    std::cout << "  hand size distribution: ";
    for (int i = 0; i <= 5; ++i) std::cout << i << ":" << countBySize[i] << " ";
    std::cout << std::endl;
    assert(countBySize[0] == 1);
    assert(countBySize[1] == 15);
    std::cout << "[PASS] test_hand_encoder_size_distribution" << std::endl;
}

static void test_infoset_key() {
    InfoSetKey key1 = buildInfoSetKey({"7", "5"}, 3, 10, 12, 25, emptyLastPlay(), 0);
    InfoSetKey key2 = buildInfoSetKey({"7", "5"}, 3, 10, 12, 25, emptyLastPlay(), 0);
    assert(key1 == key2);
    assert(InfoSetKeyHash()(key1) == InfoSetKeyHash()(key2));

    InfoSetKey key3 = buildInfoSetKey({"7", "5"}, 3, 10, 12, 26, emptyLastPlay(), 0);
    assert(!(key1 == key3));

    std::cout << "[PASS] test_infoset_key" << std::endl;
}

static void test_deck_bucket() {
    assert(deckCountToBucket(0) == 0);
    assert(deckCountToBucket(5) == 5);
    assert(deckCountToBucket(14) == 14);
    assert(deckCountToBucket(15) == 15);
    assert(deckCountToBucket(20) == 15);
    assert(deckCountToBucket(27) == 15);
    std::cout << "[PASS] test_deck_bucket" << std::endl;
}

static void test_endgame_boundary() {
    auto key_in = buildInfoSetKey({"7"}, 1, 0, 0, 10, emptyLastPlay(), 0);
    assert(isEndgameBoundary(key_in));

    auto key_out1 = buildInfoSetKey({"7"}, 3, 0, 0, 10, emptyLastPlay(), 0);
    assert(!isEndgameBoundary(key_out1));

    auto key_out2 = buildInfoSetKey({"7"}, 1, 1, 0, 10, emptyLastPlay(), 0);
    assert(!isEndgameBoundary(key_out2));

    std::cout << "[PASS] test_endgame_boundary" << std::endl;
}

static void test_terminal_utility() {
    Card c5{"5", "黑桃", 5};
    Card cK{"K", "黑桃", 20};
    Card c8{"8", "黑桃", 0};

    std::vector<Card> oppHand = {c5, cK, c8};
    std::vector<Card> emptyHand;

    double u = computeTerminalUtility(emptyHand, oppHand, 30, 20, 0, 12, 15, false, true);
    assert(u > -1.0 && u < 1.0);
    double u2 = computeTerminalUtility(emptyHand, oppHand, 30, 20, 0, 12, 15, false, false);
    assert(std::abs(u + u2) < 1e-9);

    double special = computeTerminalUtility(emptyHand, emptyHand, 0, 0, 0, 0, 0, true, true);
    assert(special == 1.0);

    std::cout << "[PASS] test_terminal_utility (u=" << u << ")" << std::endl;
}

static void test_endgame_db_build_and_query() {
    CFRParams params;
    params.iterations = 100;

    EndgameDB db;
    db.build(params);
    std::cout << "  endgame DB size: " << db.size() << " info sets" << std::endl;
    assert(db.size() > 0);

    for (uint16_t p0 = 0; p0 < 15504; ++p0) {
        auto h0 = decodeHand(p0);
        if (h0.size() > 2) continue;
        for (uint16_t p1 = 0; p1 < 15504; ++p1) {
            auto h1 = decodeHand(p1);
            if (h1.size() > 2) continue;
            if (h0.empty() && h1.empty()) continue;

            InfoSetKey key = buildInfoSetKey(h0, (int)h1.size(), 0, 0, 0, emptyLastPlay(), 0);
            std::vector<double> strat;
            if (db.lookup(key, strat)) {
                double sum = 0.0;
                for (double s : strat) { assert(s >= 0.0 && s <= 1.0); sum += s; }
                assert(std::abs(sum - 1.0) < 1e-3);
                break;
            }
        }
        break;
    }

    std::cout << "[PASS] test_endgame_db_build_and_query" << std::endl;
}

static void test_endgame_db_persist() {
    CFRParams params;
    params.iterations = 50;

    EndgameDB db;
    db.build(params);
    assert(db.save("data/cfr/test_endgame_db.bin"));

    EndgameDB db2;
    assert(db2.load("data/cfr/test_endgame_db.bin"));
    assert(db2.size() == db.size());

    std::cout << "[PASS] test_endgame_db_persist" << std::endl;
}

static void test_cfr_trainer_basic() {
    CFRParams params;
    params.iterations = 5;
    params.checkpointInterval = 5;

    EndgameDB db;
    db.build(params);

    CFRTrainer trainer(params, &db);
    std::mt19937 rng(42);

    CFRTrainingState state;
    state.myHandPoints = {"7"};
    state.oppHandPoints = {"K"};
    state.myDeck = {};
    state.oppDeck = {};
    state.myCollectedScore = 30;
    state.oppCollectedScore = 20;
    state.tableScore = 0;
    state.tableBonus = 0;
    state.lastPlay = emptyLastPlay();
    state.currentPlayer = 0;
    state.specialWin = false;
    state.gameOver = false;

    for (int iter = 0; iter < 5; ++iter) {
        trainer.dcfrTraverse(state, 1.0, 1.0, iter, rng);
    }
    trainer.computeStats();

    auto& stats = trainer.stats();
    std::cout << "  nodes: " << (int)stats.infosetCoverage
              << "  entropy: " << stats.avgStrategyEntropy << std::endl;
    (void)stats;

    std::cout << "[PASS] test_cfr_trainer_basic" << std::endl;
}

static void test_strategy_store() {
    StrategyStore store;

    InfoSetKey key = buildInfoSetKey({"7"}, 1, 0, 0, 0, emptyLastPlay(), 0);
    store.setStrategy(key, {0.3, 0.7});

    auto q = store.query(key);
    assert(q.size() == 2);
    assert(std::abs(q[0] - 0.3) < 1e-6);
    assert(std::abs(q[1] - 0.7) < 1e-6);

    auto missing = store.query(buildInfoSetKey({"K"}, 1, 0, 0, 0, emptyLastPlay(), 0));
    assert(missing.empty());

    assert(store.save("data/cfr/test_strategy.bin"));

    StrategyStore store2;
    assert(store2.load("data/cfr/test_strategy.bin"));
    assert(store2.size() == 1);

    std::cout << "[PASS] test_strategy_store" << std::endl;
}

int main() {
    std::cout << "=== CFR Unit Tests ===" << std::endl;

    test_hand_encoder_basic();
    test_hand_encoder_bijection();
    test_hand_encoder_size_distribution();
    test_infoset_key();
    test_deck_bucket();
    test_endgame_boundary();
    test_terminal_utility();
    test_endgame_db_build_and_query();
    test_endgame_db_persist();
    test_cfr_trainer_basic();
    test_strategy_store();

    std::cout << "=== All CFR Tests Passed ===" << std::endl;
    return 0;
}