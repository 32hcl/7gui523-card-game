#pragma once
#include "observation.h"
#include <random>
#include "belief/particle_filter.h"
class FairEngine {
public:
 explicit FairEngine(FairConfig config = {});
 std::vector<Card> choosePlay(const Observation&,int level=1);
 int beliefResets()const{return belief_.resets();}
 double beliefESS()const{return belief_.ess();}
private:
 FairConfig config_;
 std::mt19937 rng_;
 ParticleFilter belief_;
};
