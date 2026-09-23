#include "ai/fair_engine.h"
#include "ai/fair_common.h"
#include "ai/endgame/solver.h"
#include "ai/searcher/minimax.h"
#include <cassert>
#include <algorithm>
#include <climits>
void test_fairAI(){
 std::mt19937 rng(4242);
 for(int trial=0;trial<100;++trial){auto d=createStandardDeck();std::shuffle(d.cards.begin(),d.cards.end(),rng);Observation o;o.hand.assign(d.cards.begin(),d.cards.begin()+3);std::vector<Card> opponent(d.cards.begin()+3,d.cards.begin()+6);o.deckCount=0;o.opponentCount=3;o.finalPhase=true;
  for(int i=0;i<15;++i)o.played[i]=i==12||i==13?1:4;for(const auto& c:o.hand)--o.played[fair::index(c.point)];for(const auto& c:opponent)--o.played[fair::index(c.point)];o.myScore=trial%4*5;o.opponentScore=trial%7*5;
  auto s=fair::sampleWorld(o,rng);assert(fair::actionKey(s.oppHand)==fair::actionKey(opponent));auto result=solveEndgame(o,900);assert(result.solved);
  int exact=minimax(s,30,-20000,20000,true,SearchParams{});assert(result.value==exact);auto child=applyMove(s,result.action);assert(minimax(child,29,-20000,20000,false,SearchParams{})==exact);
 }
 auto deck=createStandardDeck();Observation o;o.hand.assign(deck.cards.begin(),deck.cards.begin()+5);o.initialHand=o.hand;
 FairConfig config;config.simulations=8;config.horizon=4;config.budgetMs=900;FairEngine a(config),b(config);
 assert(fair::actionKey(a.choosePlay(o,2))==fair::actionKey(b.choosePlay(o,2)));
 for(int n=0;n<50;++n){auto s=fair::sampleWorld(o,rng);std::array<int,15> counts{};for(const auto* h:{&s.myHand,&s.oppHand,&s.deckCards})for(const auto& c:*h)++counts[fair::index(c.point)];for(int i=0;i<15;++i)assert(counts[i]==(i==12||i==13?1:4));}
 ParticleFilter belief(config);belief.update(o);assert(belief.particles().size()==static_cast<size_t>(config.particles));assert(belief.ess()>0);
 auto state=fair::sampleWorld(o,rng);auto move=fair::policy(state,0,rng);auto next=applyMove(state,move);PublicEvent event;event.actorMe=true;event.cards=move;event.myCount=static_cast<int>(next.myHand.size());event.opponentCount=5;event.deckCount=44;o.history.push_back(event);o.hand=next.myHand;o.previous=next.lastPlay;for(const auto& c:move)++o.played[fair::index(c.point)];belief.update(o);
 for(const auto& p:belief.particles()){assert(!p.state.myTurn);assert(fair::actionKey(p.state.myHand)==fair::actionKey(o.hand));}
}
