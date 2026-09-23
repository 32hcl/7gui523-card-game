#include "solver.h"
#include "ai/fair_common.h"
#include "ai/searcher/minimax.h"
#include <chrono>
#include <algorithm>
#include <stdexcept>
#include <unordered_map>
namespace {
using Clock=std::chrono::steady_clock;
struct Timeout{};
struct Entry{int value=0,flag=0;};
struct Solver {
 Clock::time_point deadline;std::uint64_t nodes=0;std::unordered_map<std::string,Entry> table;
 int visit(const SearchState& s,int alpha,int beta){
  ++nodes;if(Clock::now()>=deadline)throw Timeout{};
  if(s.terminal)return s.winner*10000;
  const auto key=fair::publicKey(s)+":"+fair::actionKey(s.oppHand);
  const int a0=alpha,b0=beta;auto cached=table.find(key);
  if(cached!=table.end()){if(cached->second.flag==0)return cached->second.value;if(cached->second.flag==1)alpha=std::max(alpha,cached->second.value);else beta=std::min(beta,cached->second.value);if(alpha>=beta)return cached->second.value;}
  auto legal=fair::moves(s);if(legal.empty())throw std::logic_error("Unsettled empty leader");
  int best=s.myTurn?-20000:20000;
  for(const auto& move:legal){int value=visit(applyMove(s,move),alpha,beta);if(s.myTurn){best=std::max(best,value);alpha=std::max(alpha,best);}else{best=std::min(best,value);beta=std::min(beta,best);}if(alpha>=beta)break;}
  table[key]={best,best<=a0?2:best>=b0?1:0};return best;
 }
};
}
EndgameResult solveEndgame(const Observation& o,int budgetMs){
 if(o.deckCount!=0)throw std::invalid_argument("Endgame requires empty deck");
 std::mt19937 rng(123);auto state=fair::sampleWorld(o,rng);auto legal=fair::moves(state);
 EndgameResult result;if(legal.empty())return result;result.action=fair::policy(state,1,rng);
 Solver solver;solver.deadline=Clock::now()+std::chrono::milliseconds(std::max(1,std::min(900,budgetMs)));
 int best=-20000;try{for(const auto& move:legal){int value=solver.visit(applyMove(state,move),-20000,20000);if(value>best){best=value;result.action=move;result.value=value;}}result.solved=true;}catch(const Timeout&){}
 result.nodes=solver.nodes;return result;
}
