#pragma once
#include "ai/fair_common.h"
struct BeliefParticle { SearchState state; int model=1; double weight=1; };
class ParticleFilter {
public:
 explicit ParticleFilter(FairConfig config={}):config_(config),rng_(config.seed){}
 void update(const Observation&);
 const std::vector<BeliefParticle>& particles()const{return particles_;}
 double ess()const{return ess_;}
 int resets()const{return resets_;}
private:
 void initialize(const Observation&);
 void normalize();
 FairConfig config_;std::mt19937 rng_;std::vector<BeliefParticle> particles_;
 size_t processed_=0;double ess_=0;int resets_=0;
};
