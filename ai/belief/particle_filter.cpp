#include "particle_filter.h"
#include "ai/searcher/minimax.h"
#include <algorithm>
#include <numeric>
#include <stdexcept>
void ParticleFilter::initialize(const Observation& o){
 particles_.clear();std::discrete_distribution<int> models(config_.modelPrior.begin(),config_.modelPrior.end());
 for(int i=0;i<config_.particles;++i)particles_.push_back({fair::sampleWorld(o,rng_),models(rng_),1.0/config_.particles});ess_=config_.particles;
}
void ParticleFilter::normalize(){
 double sum=0;for(const auto& p:particles_)sum+=p.weight;if(sum<=0){particles_.clear();return;}
 double squares=0;for(auto& p:particles_){p.weight/=sum;squares+=p.weight*p.weight;}ess_=1/squares;
 if(ess_<config_.particles*.5){std::vector<double>w;for(const auto& p:particles_)w.push_back(p.weight);std::discrete_distribution<size_t> draw(w.begin(),w.end());std::vector<BeliefParticle> next;
 for(int i=0;i<config_.particles;++i){auto p=particles_[draw(rng_)];p.weight=1.0/config_.particles;std::shuffle(p.state.deckCards.begin(),p.state.deckCards.end(),rng_);next.push_back(std::move(p));}particles_=std::move(next);}
}
void ParticleFilter::update(const Observation& o){
 if(particles_.empty()||o.history.size()<processed_){
  processed_=0;
  if(!o.initialHand.empty()&&!o.history.empty()){Observation initial;initial.hand=o.initialHand;initialize(initial);for(auto& p:particles_)p.state.myTurn=o.initialMyTurn;}
  else {initialize(o);processed_=o.history.size();return;}
 }
 for(;processed_<o.history.size();++processed_){const auto& event=o.history[processed_];
  for(auto& p:particles_){if(p.weight==0)continue;auto& s=p.state;
   try {
    if(s.myTurn!=event.actorMe)throw std::invalid_argument("turn");
    auto action=fair::materialize(event.cards,s.myTurn?s.myHand:s.oppHand);
    auto legal=fair::moves(s);const auto key=fair::actionKey(action);bool found=false;for(const auto& a:legal)found|=fair::actionKey(a)==key;if(!found)throw std::invalid_argument("action");
    if(!event.actorMe){double uniform=1.0/static_cast<double>(legal.size());double likelihood=uniform;
     if(p.model!=4)likelihood=config_.tolerance*uniform+(fair::actionKey(fair::policy(s,p.model,rng_))==key?1-config_.tolerance:0);
     p.weight*=likelihood;
    }
    if(!event.myDraws.empty()){
     // Condition the unseen deck on private draws. The other player's preceding
     // draws remain latent; sampling from the remainder is the conditional law.
     auto pool=s.deckCards;double chance=1;
     for(const auto& c:event.myDraws){int count=0;for(const auto& x:pool)count+=x.point==c.point;if(!count)throw std::invalid_argument("draw");chance*=static_cast<double>(count)/pool.size();auto it=std::find_if(pool.begin(),pool.end(),[&](const Card& x){return x.point==c.point;});pool.erase(it);}
     p.weight*=chance;std::shuffle(pool.begin(),pool.end(),rng_);std::vector<Card> order;
     // On a pass the non-actor won and draws first.
     if(event.actorMe){int n=std::min(5-static_cast<int>(s.oppHand.size()),static_cast<int>(s.deckCards.size()));for(int i=0;i<n;++i){if(pool.empty())throw std::invalid_argument("draw count");order.push_back(pool.back());pool.pop_back();}}
     order.insert(order.end(),event.myDraws.begin(),event.myDraws.end());for(auto it=order.rbegin();it!=order.rend();++it)pool.push_back(*it);s.deckCards=std::move(pool);
    }
    s=applyMove(s,action);
    if(static_cast<int>(s.myHand.size())!=event.myCount||static_cast<int>(s.oppHand.size())!=event.opponentCount||static_cast<int>(s.deckCards.size())!=event.deckCount)throw std::invalid_argument("counts");
   }catch(const std::invalid_argument&){p.weight=0;}catch(const std::logic_error&){p.weight=0;}
  }
  normalize();if(particles_.empty()){++resets_;initialize(o);processed_=o.history.size();return;}
 }
 // The current private hand is an observation, never a hidden-state input.
 for(auto& p:particles_)if(fair::actionKey(p.state.myHand)!=fair::actionKey(o.hand))p.weight=0;
 normalize();if(particles_.empty()){++resets_;initialize(o);}
}
