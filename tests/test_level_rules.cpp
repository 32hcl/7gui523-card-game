#include <cassert>
#include <vector>
#include <random>
#include <string>
#include "core/card/card.h"
#include "core/card/cardtype.h"
#include "core/card/rank.h"
#include "core/player.h"
#include "ai/plugins/level_rule.h"
#include "ai/ai.h"

static Card makeCard(const std::string& point, const std::string& suit = "") {
    Card c;
    c.point = point;
    c.suit = suit;
    if (point == "5")       c.score = 5;
    else if (point == "10") c.score = 10;
    else if (point == "K")  c.score = 20;
    else                    c.score = 0;
    return c;
}

static Player makePlayer(std::vector<Card> cards) {
    Player p;
    p.hand = cards;
    return p;
}

static CardTypeResult makeLast(CardType t, const std::string& keyPoint,
                                std::vector<Card> cards) {
    CardTypeResult r;
    r.type = t;
    r.keyPoint = keyPoint;
    r.cards = cards;
    return r;
}

static CardTypeResult makeSingleLast(const std::string& keyPoint,
                                      const std::string& point,
                                      const std::string& suit) {
    return makeLast(CardType::Single, keyPoint, {makeCard(point, suit)});
}

// ── 工厂 ──

void test_levelRule_level1_noops() {
    LevelRule r = createLevelRule(1);
    assert(!r.tryForcePlay);
    assert(!r.canBeatOverride);
    assert(!r.blockPlayer);
    assert(!r.bossDamage);
}

void test_levelRule_level4_hasBossDamage() {
    LevelRule r = createLevelRule(4);
    assert(!r.tryForcePlay);
    assert(!r.blockPlayer);
    assert(r.bossDamage);
}

void test_levelRule_level5_hasAll() {
    LevelRule r = createLevelRule(5);
    assert(r.tryForcePlay);
    assert(r.canBeatOverride);
    assert(r.blockPlayer);
    assert(!r.bossDamage);
}

void test_levelRule_level6_hasAll() {
    LevelRule r = createLevelRule(6);
    assert(r.tryForcePlay);
    assert(r.canBeatOverride);
    assert(!r.blockPlayer);
    assert(!r.bossDamage);
}

// ── 疯狗 damage ──

void test_madDog_damage() {
    LevelRule r = createLevelRule(4);
    assert(r.bossDamage);
    assert(r.bossDamage({}) == 0);

    std::vector<Card> six = {{}, {}, {}, {}, {}, {}};
    assert(r.bossDamage(six) == 18);

    std::vector<Card> two = {{}, {}};
    assert(r.bossDamage(two) == 6);

    std::vector<Card> one = {{}};
    assert(r.bossDamage(one) == 3);
}

// ── 赖账鬼 blockPlayer ──

void test_debtor_blockPlayer() {
    LevelRule r = createLevelRule(5);
    assert(r.blockPlayer);

    auto player  = makeSingleLast("7", "7", "红桃");
    auto boss    = makeSingleLast("7", "7", "梅花");
    assert(r.blockPlayer(player, boss));  // 同点数 → 阻止

    auto player2 = makeSingleLast("8", "8", "红桃");
    assert(!r.blockPlayer(player2, boss));  // 不同点数 → 允许

    auto pairP   = makeLast(CardType::Pair, "7", {makeCard("7", "红桃"), makeCard("7", "方块")});
    assert(r.blockPlayer(pairP, boss));  // 对子同点数 → 阻止
}

// ── 赖账鬼 tryForcePlay ──

void test_debtor_tryForcePlay() {
    LevelRule r = createLevelRule(5);
    std::mt19937 rng(42);

    Player boss = makePlayer({
        makeCard("J", "梅花"), makeCard("5", "红桃"),
        makeCard("K", "红桃"), makeCard("7", "黑桃")
    });

    // 上家出梅花4 → Boss有梅花J → 强制出J梅花
    auto lp = makeSingleLast("4", "4", "梅花");
    auto result = r.tryForcePlay(boss, lp, rng);
    assert(!result.empty());
    assert(result[0].point == "J" || result[0].point == "J");
    assert(result[0].suit == "梅花");

    // 无同花色的牌 → 返回空
    auto lp2 = makeSingleLast("4", "4", "方块");
    auto result2 = r.tryForcePlay(boss, lp2, rng);
    assert(result2.empty());

    // 上家单张 >= J → 返回空
    auto lp3 = makeSingleLast("J", "J", "黑桃");
    auto result3 = r.tryForcePlay(boss, lp3, rng);
    assert(result3.empty());

    // 上家不是单张 → 返回空
    auto lp4 = makeLast(CardType::Pair, "4", {makeCard("4", "梅花"), makeCard("4", "红桃")});
    auto result4 = r.tryForcePlay(boss, lp4, rng);
    assert(result4.empty());
}

// ── 赖账鬼 canBeatOverride ──

void test_debtor_canBeatOverride() {
    LevelRule r = createLevelRule(5);
    std::mt19937 rng(42);

    auto boss     = makeSingleLast("9", "9", "梅花");
    auto lastPlay = makeSingleLast("6", "6", "梅花");
    assert(r.canBeatOverride(boss, lastPlay, rng));  // 同花 <J

    auto boss2    = makeSingleLast("9", "9", "方块");
    assert(!r.canBeatOverride(boss2, lastPlay, rng));  // 不同花

    auto lastPlay2 = makeSingleLast("J", "J", "梅花");
    assert(!r.canBeatOverride(boss, lastPlay2, rng));  // >=J 不限

    auto boss3     = makeLast(CardType::Pair, "5", {makeCard("5", "梅花"), makeCard("5", "方块")});
    auto lastPlay3 = makeSingleLast("5", "5", "梅花");
    assert(!r.canBeatOverride(boss3, lastPlay3, rng));  // 不是单张
}

// ── 不识数 tryForcePlay ──

void test_blind_tryForcePlay() {
    LevelRule r = createLevelRule(6);
    // seed 0 → first rng() % 2 = 0 → 触发
    std::mt19937 rngYes(0);
    Player boss = makePlayer({
        makeCard("6"), makeCard("7"), makeCard("8"),
        makeCard("9"), makeCard("4"), makeCard("5"), makeCard("3")
    });
    auto lp = makeSingleLast("K", "K", "黑桃");

    auto result = r.tryForcePlay(boss, lp, rngYes);
    // 最小单张应该是 4 (rank=1)
    assert(!result.empty());
    CardTypeResult parsed = parseCardType(result);
    assert(parsed.type == CardType::Single);
    assert(parsed.keyPoint == "4");

    // seed 1 → first rng() % 2 = 1 → 不触发 → 返回空
    std::mt19937 rngNo(1);
    auto result2 = r.tryForcePlay(boss, lp, rngNo);
    assert(result2.empty());

    // 无同牌型 → 返回空（boss 没有对子）
    std::mt19937 rngYes2(0);
    auto lp2 = makeLast(CardType::Pair, "K", {makeCard("K", "黑桃"), makeCard("K", "红桃")});
    auto result3 = r.tryForcePlay(boss, lp2, rngYes2);
    assert(result3.empty());
}

// ── 不识数 tryForcePlay 最小对子 ──

void test_blind_tryForcePlay_pair() {
    LevelRule r = createLevelRule(6);
    std::mt19937 rng(0);  // 触发
    Player boss = makePlayer({
        makeCard("7"), makeCard("7"),
        makeCard("4"), makeCard("4"),
        makeCard("9"), makeCard("Q")
    });
    auto lp = makeLast(CardType::Pair, "K", {makeCard("K", "黑桃"), makeCard("K", "红桃")});

    auto result = r.tryForcePlay(boss, lp, rng);
    assert(!result.empty());
    CardTypeResult parsed = parseCardType(result);
    assert(parsed.type == CardType::Pair);
    // 4(rank=1) < 7(rank=15) → 应出44
    assert(parsed.keyPoint == "4");
}

// ── 不识数 canBeatOverride ──

void test_blind_canBeatOverride() {
    LevelRule r = createLevelRule(6);

    std::mt19937 rngYes(0);
    auto boss     = makeSingleLast("4", "4", "黑桃");
    auto lastPlay = makeSingleLast("K", "K", "红桃");
    assert(r.canBeatOverride(boss, lastPlay, rngYes));  // 同牌型 50%→放行

    std::mt19937 rngNo(1);
    assert(!r.canBeatOverride(boss, lastPlay, rngNo));  // 50%→不放

    auto boss2 = makeLast(CardType::Pair, "4", {makeCard("4", "黑桃"), makeCard("4", "红桃")});
    std::mt19937 rngYes2(0);
    assert(!r.canBeatOverride(boss2, lastPlay, rngYes2));  // 不同牌型→不放
}