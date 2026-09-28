#include "position.h"
#include "core/rule/score.h"
#include <algorithm>
#include <stdexcept>

namespace {
void finish(GamePosition& s, bool me) {
    auto& other = me ? s.oppHand : s.myHand;
    int gain = s.tableScore + s.tableBonus + calculateScore(other);
    (me ? s.myScore : s.oppScore) += gain;
    other.clear(); s.tableScore = s.tableBonus = 0;
    s.terminal = true;
    s.winner = s.myScore > s.oppScore ? 1 : s.myScore < s.oppScore ? -1 : 0;
}
void refill(std::vector<Card>& hand, std::vector<Card>& deck, int& remaining) {
    while (hand.size() < 5 && !deck.empty() && remaining > 0) {
        hand.push_back(deck.back()); deck.pop_back(); remaining--;
    }
}
}
GamePosition advancePosition(const GamePosition& s, const std::vector<Card>& move) {
    if (s.terminal) return s;
    GamePosition n = s;
    auto& hand = n.myTurn ? n.myHand : n.oppHand;
    if (move.empty()) {
        if (n.lastPlay.type == CardType::Invalid) {
            throw std::invalid_argument("Cannot pass while leading");
        }
        const bool winnerMe = !n.myTurn;
        (winnerMe ? n.myScore : n.oppScore) += n.tableScore + n.tableBonus;
        n.tableScore = n.tableBonus = 0; n.lastPlay = CardTypeResult{};
        if (n.myDeckRemaining == 0) n.myFinalPhase = true;
        if (n.oppDeckRemaining == 0) n.oppFinalPhase = true;
        auto& winnerHand = winnerMe ? n.myHand : n.oppHand;
        auto& loserHand  = winnerMe ? n.oppHand : n.myHand;
        int& winnerRemaining = winnerMe ? n.myDeckRemaining : n.oppDeckRemaining;
        int& loserRemaining  = winnerMe ? n.oppDeckRemaining : n.myDeckRemaining;
        if (!n.myFinalPhase) { refill(n.myHand, n.deckCards, n.myDeckRemaining); }
        if (!n.oppFinalPhase) { refill(n.oppHand, n.deckCards, n.oppDeckRemaining); }
        if (n.myHand.empty() && n.myDeckRemaining == 0) { finish(n, true); return n; }
        if (n.oppHand.empty() && n.oppDeckRemaining == 0) { finish(n, false); return n; }
        if (n.myHand.empty() && n.oppHand.empty() && n.myDeckRemaining == 0 && n.oppDeckRemaining == 0) {
            bool finisherMe = n.firstEmpty != -1;
            finish(n, finisherMe); return n;
        }
        n.firstEmpty = 0; n.myTurn = winnerMe;
        return n;
    }
    const auto parsed = parseCardType(move);
    if (parsed.type == CardType::Invalid || !canBeat(parsed,n.lastPlay))
        throw std::invalid_argument("Illegal play");
    for (const auto& c : move) {
        auto it=std::find_if(hand.begin(),hand.end(),[&](const Card& h){return h.point==c.point && h.suit==c.suit;});
        if(it==hand.end()) throw std::invalid_argument("Card not held");
        hand.erase(it);
    }
    if (parsed.type == CardType::Special523) { n.terminal=true; n.winner=n.myTurn?1:-1; n.lastPlay=parsed; return n; }
    n.tableBonus += calculatePressureBonus(parsed,n.lastPlay);
    n.tableScore += calculateScore(move); n.lastPlay=parsed;
    if (hand.empty()) {
        if (!n.firstEmpty) n.firstEmpty=n.myTurn?1:-1;
        auto& playerRemaining = n.myTurn ? n.myDeckRemaining : n.oppDeckRemaining;
        auto& playerPhase     = n.myTurn ? n.myFinalPhase : n.oppFinalPhase;
        if (playerRemaining == 0) playerPhase = true;
        if (playerPhase && playerRemaining == 0) { finish(n, n.myTurn); return n; }
    }
    n.myTurn=!n.myTurn; return n;
}