#include "fair_common.h"
#include "ai.h"
#include "searcher/minimax.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
namespace fair {
static const std::array<std::string,15> points{{"4","6","8","9","10","J","Q","K","A","3","2","5","小鬼","大鬼","7"}};
int index(const std::string& p){auto it=std::find(points.begin(),points.end(),p);if(it==points.end())throw std::invalid_argument("Unknown rank");return static_cast<int>(it-points.begin());}
Card card(int i){const auto& p=points.at(static_cast<size_t>(i));return {p,"",p=="5"?5:p=="10"?10:p=="K"?20:0};}
std::vector<Card> canonical(const std::vector<Card>& cards){std::vector<Card> out;for(const auto& c:cards)out.push_back(card(index(c.point)));return out;}
std::vector<Card> materialize(const std::vector<Card>& action,const std::vector<Card>& hand){auto pool=hand;std::vector<Card> out;for(const auto& c:action){auto it=std::find_if(pool.begin(),pool.end(),[&](const Card& h){return h.point==c.point;});if(it==pool.end())throw std::logic_error("Unheld rank");out.push_back(*it);pool.erase(it);}return out;}
std::string actionKey(const std::vector<Card>& a){std::array<int,15> n{};for(const auto& c:a)++n[index(c.point)];std::string key;for(int x:n)key+=static_cast<char>('0'+x);return key;}
std::vector<std::vector<Card>> moves(const SearchState& s){std::vector<std::vector<Card>> out;std::set<std::string> seen;for(const auto& a:genLegalMoves(s))if(seen.insert(actionKey(a)).second)out.push_back(a);return out;}
std::string publicKey(const SearchState& s){return actionKey(s.myHand)+":"+std::to_string(s.oppHand.size())+":"+std::to_string(s.deckCards.size())+":"+std::to_string(s.myScore)+":"+std::to_string(s.oppScore)+":"+std::to_string(s.tableScore)+":"+std::to_string(s.tableBonus)+":"+std::to_string(static_cast<int>(s.lastPlay.type))+s.lastPlay.keyPoint+":"+std::to_string(s.myTurn)+std::to_string(s.finalPhase)+std::to_string(s.firstEmpty);}
SearchState sampleWorld(const Observation& o,std::mt19937& rng){
 SearchState s;s.myHand=canonical(o.hand);s.myScore=o.myScore;s.oppScore=o.opponentScore;s.tableScore=o.tableScore;s.tableBonus=o.tableBonus;
 s.lastPlay=o.previous;s.lastPlay.cards=canonical(o.previous.cards);s.finalPhase=o.finalPhase;s.firstEmpty=o.firstEmpty;
 std::array<int,15> available{};for(int i=0;i<15;++i)available[i]=(i==12||i==13?1:4)-o.played[i];
 for(const auto& c:s.myHand)--available[index(c.point)];std::vector<Card> pool;
 for(int i=0;i<15;++i){if(available[i]<0)throw std::invalid_argument("Invalid public counts");for(int n=0;n<available[i];++n)pool.push_back(card(i));}
 if(static_cast<int>(pool.size())!=o.opponentCount+o.deckCount)throw std::invalid_argument("Public cards not conserved");
 std::shuffle(pool.begin(),pool.end(),rng);s.oppHand.assign(pool.begin(),pool.begin()+o.opponentCount);s.deckCards.assign(pool.begin()+o.opponentCount,pool.end());return s;
}
std::vector<Card> policy(const SearchState& s,int model,std::mt19937& rng){
 auto legal=moves(s);if(legal.empty())return {};for(const auto& a:legal)if(parseCardType(a).type==CardType::Special523)return a;
 if(model==4)return legal[std::uniform_int_distribution<size_t>(0,legal.size()-1)(rng)];
 const auto& h=s.myTurn?s.myHand:s.oppHand;const auto& other=s.myTurn?s.oppHand:s.myHand;
 if(model==0||model==1){Player me=createPlayer("model"),opp=createPlayer("unknown");me.hand=h;me.totalScore=s.myTurn?s.myScore:s.oppScore;opp.hand.resize(other.size());opp.totalScore=s.myTurn?s.oppScore:s.myScore;Deck d;d.cards.resize(s.deckCards.size());
 CardTracker tracker;std::array<int,15> remaining{};for(const auto* cards:{&s.myHand,&s.oppHand,&s.deckCards})for(const auto& c:*cards)++remaining[index(c.point)];std::vector<Card> played;for(int i=0;i<15;++i)for(int n=remaining[i];n<(i==12||i==13?1:4);++n)played.push_back(card(i));tracker.recordPlayed(played);
 auto a=model==0?aiChoosePlayAI2(me,opp,s.lastPlay,d,s.tableScore+s.tableBonus):aiChoosePlayAI3(me,opp,s.lastPlay,d,s.tableScore+s.tableBonus,tracker);
 if(!a.empty()||s.lastPlay.type!=CardType::Invalid)return a;return legal.front();}
 double best=-1e100;std::vector<Card> action;
 for(const auto& a:legal){double rank=0,score=0;for(const auto& c:a){rank+=index(c.point)+1;score+=c.score;}double value=a.empty()?(model==2?-3.0:-20.0):static_cast<double>(a.size())*8-rank*(model==2?2:0.5)+score*(model==3?1.0:-0.2)+(s.tableScore+s.tableBonus)*(model==3?1.0:0.1);if(a.size()==h.size())value+=30;if(value>best){best=value;action=a;}}
 return action;
}
double utility(const SearchState& s){if(s.terminal)return static_cast<double>(s.winner);double ranks=0;for(const auto& c:s.myHand)ranks+=index(c.point)+1;for(const auto& c:s.oppHand)ranks-=index(c.point)+1;return std::tanh((s.myScore-s.oppScore)/80.0+ranks/120.0+(static_cast<double>(s.oppHand.size())-static_cast<double>(s.myHand.size()))*.035+(s.myTurn?-1:1)*(s.tableScore+s.tableBonus)/160.0);}
}
