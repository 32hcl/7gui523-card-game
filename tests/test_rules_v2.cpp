#include "ai/searcher/minimax.h"
#include <cassert>
void test_rulesV2() {
    const Card five{"5","",5}, king{"K","",20}, four{"4","",0}, seven{"7","",0};
    assert(parseCardType({four,four,four,{"大鬼","",0}}).type==CardType::TripleWithOne);
    const Card fives{"5","S",5}, fiveh{"5","H",5}, fivec{"5","C",5}, fived{"5","D",5};
    auto bomb=parseCardType({fives,fiveh,fivec,fived});
    assert(bomb.type == CardType::Bomb);
    auto single=parseCardType({five}); assert(calculatePressureBonus(single,single)==5);
    SearchState s; s.myHand={five}; s.oppHand={king}; s.lastPlay=single;
    s.tableScore=5; s.tableBonus=10; s.myFinalPhase=true; s.oppFinalPhase=true; s.oppScore=80;
    auto end=applyMove(s,{five}); assert(end.terminal && end.myScore==45 && end.winner==-1);
    // Deck exhaustion is only acted on at a boundary until finalPhase.
    s=SearchState{}; s.myHand={four}; s.oppHand={seven};
    s.myDeckRemaining = 0; s.oppDeckRemaining = 0;
    auto a=applyMove(s,{four}); assert(a.terminal && a.firstEmpty==1 && a.winner==0);
    // Opponent wins round and receives last cards first; empty loser finishes.
    s=SearchState{}; s.myHand={}; s.oppHand={seven}; s.myTurn=true;
    s.lastPlay=parseCardType({king}); s.tableScore=20; s.firstEmpty=1;
    s.deckCards={five,king}; s.myDeckRemaining=0; s.oppDeckRemaining=2; auto d=applyMove(s,{});
    assert(d.terminal && d.oppScore==20 && d.myScore==25 && d.winner==1);
    // Holding Special523 does not finish, but playing it does.
    s=SearchState{}; s.myHand={{"7","",0},{"大鬼","",0},five,{"2","",0},{"3","",0}};
    s.oppHand={four}; s.oppScore=1000;
    auto moves=genLegalMoves(s); assert(!moves.empty());
    auto chosen=searchBestPlayCheat(Player{"me",s.myHand},Player{"opp",s.oppHand},CardTypeResult{},Deck{},0,2);
    assert(parseCardType(chosen).type==CardType::Special523);
    auto special=applyMove(s,s.myHand); assert(special.terminal && special.winner==1);
    assert(canBeat(parseCardType(s.myHand),parseCardType(s.myHand)));
    assert(canBeat(CardTypeResult{},CardTypeResult{}));
    bool rejected=false; try {applyMove(s,{});} catch(...) {rejected=true;} assert(rejected);
}