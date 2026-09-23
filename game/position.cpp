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
void refill(std::vector<Card>& hand, std::vector<Card>& deck) {
    while (hand.size() < 5 && !deck.empty()) { hand.push_back(deck.back()); deck.pop_back(); }
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
        // An empty deck is noticed at a round boundary; not in mid-round.
        if (n.deckCards.empty()) n.finalPhase = true;
        auto& winner = winnerMe ? n.myHand : n.oppHand;
        auto& loser = winnerMe ? n.oppHand : n.myHand;
        if (!n.finalPhase) { refill(winner,n.deckCards); refill(loser,n.deckCards); }
        if (n.deckCards.empty() && (n.myHand.empty() || n.oppHand.empty())) {
            bool finisherMe = n.myHand.empty();
            if (n.myHand.empty() && n.oppHand.empty()) finisherMe = n.firstEmpty != -1;
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
        if (n.finalPhase && n.deckCards.empty()) { finish(n,n.myTurn); return n; }
    }
    n.myTurn=!n.myTurn; return n;
}