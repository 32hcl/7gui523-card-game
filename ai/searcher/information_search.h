#pragma once
#include "ai/belief/particle_filter.h"
std::vector<Card> informationSearch(const Observation&,const std::vector<BeliefParticle>&,const FairConfig&,std::mt19937&,bool completeWorld=false);
