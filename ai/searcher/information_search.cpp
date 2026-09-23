#include "information_search.h"
#include "minimax.h"
#include <chrono>
#include <cmath>
#include <climits>
#include <unordered_map>
namespace {struct Edge{int visits=0;double total=0;};struct Node{int visits=0;std::unordered_map<std::string,Edge> edges;};}
std::vector<Card> informationSearch(const Observation& o,const std::vector<BeliefParticle>& particles,const FairConfig& c,std::mt19937& rng,bool completeWorld){
 using Clock=std::chrono::steady_clock;const auto deadline=Clock::now()+std::chrono::milliseconds(c.budgetMs);
 auto root=fair::sampleWorld(o,rng);auto legal=fair::moves(root);if(legal.size()<2)return legal.empty()?std::vector<Card>{}:legal.front();
 for(const auto& a:legal)if(parseCardType(a).type==CardType::Special523)return a;
 auto fallback=fair::policy(root,1,rng);if(particles.empty())return fallback;
 std::vector<double> weights;for(const auto& p:particles)weights.push_back(p.weight);std::discrete_distribution<size_t> pick(weights.begin(),weights.end());
 std::unordered_map<std::string,Node> tree;
 for(int sim=0;sim<c.simulations&&Clock::now()<deadline;++sim){const auto& particle=particles[pick(rng)];auto s=particle.state;s.myTurn=true;
  std::string history="root";std::vector<std::pair<std::string,std::string>> path;bool rollout=false;
  for(int step=0;step<c.horizon&&!s.terminal&&Clock::now()<deadline;++step){
   auto actions=fair::moves(s);if(actions.empty())break;std::vector<Card> action;
   if(s.myTurn&&!rollout){
    // Keys contain own observations and action history, never sampled hidden cards.
    auto nodeKey=history+fair::publicKey(s);auto& node=tree[nodeKey];double best=-1e100;
    for(const auto& a:actions){auto key=fair::actionKey(a);auto& edge=node.edges[key];double u=edge.visits?edge.total/edge.visits+c.exploration*std::sqrt(std::log(node.visits+1.0)/edge.visits):1e50+std::uniform_real_distribution<double>(0,1)(rng)*1e45;
     if(u>best){best=u;action=a;}}
    const auto key=fair::actionKey(action);rollout=node.edges[key].visits==0;path.emplace_back(nodeKey,key);
   }else action=fair::policy(s,s.myTurn?c.rolloutModel:particle.model,rng);
   history+=(s.myTurn?"M":"O")+fair::actionKey(action);s=applyMove(s,action);history+=fair::publicKey(s);
   // Controlled determinization ablation: after the root use omniscient minimax.
   if(completeWorld&&step==0&&!s.terminal){int value=minimax(s,std::min(4,c.horizon),-1000000,1000000,s.myTurn,SearchParams{});double reward=std::tanh(value/500.0);for(const auto& edge:path){auto& n=tree[edge.first];++n.visits;auto& e=n.edges[edge.second];++e.visits;e.total+=reward;}path.clear();break;}
  }
  double reward=fair::utility(s);for(const auto& edge:path){auto& n=tree[edge.first];++n.visits;auto& e=n.edges[edge.second];++e.visits;e.total+=reward;}
 }
 auto it=tree.find("root"+fair::publicKey(root));if(it==tree.end())return fallback;double best=-1e100;auto answer=fallback;
 for(const auto& a:legal){auto e=it->second.edges.find(fair::actionKey(a));if(e!=it->second.edges.end()&&e->second.visits){double value=e->second.total/e->second.visits;if(value>best){best=value;answer=a;}}}return answer;
}
