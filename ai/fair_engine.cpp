#include "fair_engine.h"
#include "fair_common.h"
#include "endgame/solver.h"
#include "searcher/information_search.h"
#include <chrono>
#include <stdexcept>
FairEngine::FairEngine(FairConfig config):config_(config),rng_(config.seed),belief_(config){
 if(config.particles<1||config.particles>4096||config.simulations<1||config.horizon<1||config.horizon>250||config.budgetMs<1||config.budgetMs>900||config.tolerance<=0||config.tolerance>1||config.exploration<0||config.rolloutModel<0||config.rolloutModel>4)throw std::invalid_argument("Invalid fair search config");
 double sum=0;for(double p:config.modelPrior){if(p<0)throw std::invalid_argument("Invalid model prior");sum+=p;}if(sum<=0)throw std::invalid_argument("Empty model prior");
}
std::vector<Card> FairEngine::choosePlay(const Observation& o,int level){

 if(o.deckCount==0)return fair::materialize(solveEndgame(o,900).action,o.hand);
 if(level>=2){auto start=std::chrono::steady_clock::now();belief_.update(o);auto remaining=config_;auto spent=static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start).count());remaining.budgetMs=std::max(1,config_.budgetMs-spent);return fair::materialize(informationSearch(o,belief_.particles(),remaining,rng_,level==3),o.hand);}
 return fair::materialize(fair::policy(fair::sampleWorld(o,rng_),1,rng_),o.hand);
}
