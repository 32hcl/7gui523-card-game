#pragma once
#include "observation.h"
#include "searcher/search_state.h"
#include <random>
#include <string>
namespace fair {
int index(const std::string&);
Card card(int);
std::vector<Card> canonical(const std::vector<Card>&);
std::vector<Card> materialize(const std::vector<Card>&,const std::vector<Card>&);
std::vector<std::vector<Card>> moves(const SearchState&);
std::string actionKey(const std::vector<Card>&);
std::string publicKey(const SearchState&);
SearchState sampleWorld(const Observation&,std::mt19937&);
std::vector<Card> policy(const SearchState&,int,std::mt19937&);
double utility(const SearchState&);
}
