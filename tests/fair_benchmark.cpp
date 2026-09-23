#include "ai/ai.h"
#include "ai/fair_engine.h"
#include "ai/fair_common.h"
#include "ai/searcher/minimax.h"
#include <algorithm>
#include <chrono>
#include <climits>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <numeric>
#include <random>
#include <stdexcept>
using Clock=std::chrono::steady_clock;
static SearchState perspective(const SearchState& s,bool me) {
    if(me) return s;
    auto v=s; std::swap(v.myHand,v.oppHand); std::swap(v.myScore,v.oppScore);
    v.myTurn=!s.myTurn; v.winner=-s.winner; v.firstEmpty=-s.firstEmpty; return v;
}
static std::vector<Card> oldMove(const SearchState& v,const std::string& level,const CardTracker& tracker) {
    Player me=createPlayer("me"),opp=createPlayer("opp");me.hand=v.myHand;me.totalScore=v.myScore;
    opp.hand=v.oppHand;opp.totalScore=v.oppScore;Deck deck;deck.cards=v.deckCards;
    if(level=="AI4") {
        auto moves=genLegalMoves(v); int best=INT_MIN+1;std::vector<Card> move;
        for(const auto& a:moves){auto n=applyMove(v,a);int x=minimax(n,5,INT_MIN+1,INT_MAX-1,false,SearchParams{});if(x>best){best=x;move=a;}}
        return move;
    }
    me.aiLevel=level=="AI2"?AILevel::AI2_Rule:level=="AI3"?AILevel::AI3_Tracker:AILevel::AI1_Simple;
    return aiChoosePlay(me,opp,v.lastPlay,deck,v.tableScore+v.tableBonus,tracker);
}
static double quantile(std::vector<double> x,double q){if(x.empty())return 0;std::sort(x.begin(),x.end());return x[static_cast<size_t>(q*static_cast<double>(x.size()-1))];}
int main(int argc,char** argv) {
 try {
    std::map<std::string,std::string> args;for(int i=1;i+1<argc;i+=2)args[argv[i]]=argv[i+1];
    auto get=[&](const std::string& k,const std::string& d){auto i=args.find(k);return i==args.end()?d:i->second;};
    int games=std::stoi(get("--games","200")),seed=std::stoi(get("--seed","20260919"));
    if(games<=0 || games%2)throw std::invalid_argument("Use positive even games for paired seats");
    auto candidate=get("--candidate","AI4"),opponent=get("--opponent","AI3");
    for(const auto& name:{candidate,opponent})if(name!="AI1"&&name!="AI2"&&name!="AI3"&&name!="AI4"&&name!="Fair1"&&name!="Fair2"&&name!="World")throw std::invalid_argument("Unknown AI name");
    if(opponent=="Fair1"||opponent=="Fair2"||opponent=="World")throw std::invalid_argument("Use fair levels as candidate");
    std::string path=get("--output","benchmark.json");
    FairConfig config;config.particles=std::stoi(get("--particles","48"));config.simulations=std::stoi(get("--simulations","48"));config.horizon=std::stoi(get("--horizon","12"));config.budgetMs=std::stoi(get("--budget-ms","60"));config.exploration=std::stod(get("--exploration","1.2"));config.tolerance=std::stod(get("--tolerance","0.08"));config.rolloutModel=std::stoi(get("--rollout-model","1"));
    for(int i=0;i<5;++i)config.modelPrior[i]=std::stod(get("--prior"+std::to_string(i),std::to_string(config.modelPrior[i])));
    std::vector<double> latency;int wins=0,losses=0,draws=0;double scoreDiff=0;int beliefResets=0;
    std::string details;auto begin=Clock::now();
    for(int g=0;g<games;++g){
        std::mt19937 rng(static_cast<unsigned>(seed+g/2));Deck deck=createStandardDeck();std::shuffle(deck.cards.begin(),deck.cards.end(),rng);
        Player first=createPlayer("first"),second=createPlayer("second");dealCards(first,deck,5);dealCards(second,deck,5);
        bool firstSeat=g%2==0;SearchState s;s.myHand=firstSeat?first.hand:second.hand;s.oppHand=firstSeat?second.hand:first.hand;
        s.deckCards=deck.cards;s.myTurn=firstSeat;CardTracker tracker;
        
        FairEngine engine(config);Observation obs;obs.initialHand=s.myHand;obs.initialMyTurn=firstSeat;
        int steps=0;
        while(!s.terminal){
            if(++steps>250)throw std::runtime_error("Nonterminating game");
            bool mine=s.myTurn;auto v=perspective(s,mine);auto start=Clock::now();
            obs.hand=s.myHand;obs.opponentCount=static_cast<int>(s.oppHand.size());obs.deckCount=static_cast<int>(s.deckCards.size());
            obs.myScore=s.myScore;obs.opponentScore=s.oppScore;obs.tableScore=s.tableScore;obs.tableBonus=s.tableBonus;
            obs.previous=s.lastPlay;obs.finalPhase=s.finalPhase;obs.firstEmpty=s.firstEmpty;
            auto action=mine&&(candidate=="Fair1"||candidate=="Fair2"||candidate=="World")?engine.choosePlay(obs,candidate=="Fair1"?1:candidate=="Fair2"?2:3):oldMove(v,mine?candidate:opponent,tracker);
            if(mine)latency.push_back(std::chrono::duration<double,std::milli>(Clock::now()-start).count());
            auto before=s.myHand;
            if(mine)for(const auto& c:action){auto it=std::find_if(before.begin(),before.end(),[&](const Card& x){return x.point==c.point&&x.suit==c.suit;});if(it!=before.end())before.erase(it);}
            s=applyMove(s,action);tracker.recordPlayed(action);
            PublicEvent event;event.actorMe=mine;event.cards=fair::canonical(action);event.myCount=static_cast<int>(s.myHand.size());event.opponentCount=static_cast<int>(s.oppHand.size());event.deckCount=static_cast<int>(s.deckCards.size());
            for(const auto& c:s.myHand){auto it=std::find_if(before.begin(),before.end(),[&](const Card& x){return x.point==c.point&&x.suit==c.suit;});if(it==before.end())event.myDraws.push_back(fair::card(fair::index(c.point)));else before.erase(it);}
            obs.history.push_back(event);for(const auto& c:action)++obs.played[fair::index(c.point)];
        }
        beliefResets+=engine.beliefResets();
        wins+=s.winner==1;losses+=s.winner==-1;draws+=s.winner==0;scoreDiff+=s.myScore-s.oppScore;
        if(g)details+=",";
        details+="{\"game\":"+std::to_string(g)+",\"deal_seed\":"+std::to_string(seed+g/2)+",\"first\":"+(firstSeat?"true":"false")+",\"winner\":"+std::to_string(s.winner)+",\"score_diff\":"+std::to_string(s.myScore-s.oppScore)+",\"steps\":"+std::to_string(steps)+"}";
        if((g+1)%20==0)std::cerr<<candidate<<" vs "<<opponent<<" "<<g+1<<"/"<<games<<" wins="<<wins<<"\n";
    }
    double p=static_cast<double>(wins)/games,z=1.95996398454,d=1+z*z/games;
    double center=(p+z*z/(2*games))/d,half=z*std::sqrt(p*(1-p)/games+z*z/(4.0*games*games))/d;
    std::ofstream out(path);if(!out)throw std::runtime_error("Cannot write output");out<<std::setprecision(10);
    out<<"{\"belief_resets\":"<<beliefResets<<",\"config\":{\"particles\":"<<config.particles<<",\"simulations\":"<<config.simulations<<",\"horizon\":"<<config.horizon<<",\"budget_ms\":"<<config.budgetMs<<",\"exploration\":"<<config.exploration<<",\"tolerance\":"<<config.tolerance<<",\"rollout_model\":"<<config.rolloutModel<<",\"prior\":[";for(int i=0;i<5;++i){if(i)out<<",";out<<config.modelPrior[i];}out<<"]},\"candidate\":\""<<candidate<<"\",\"opponent\":\""<<opponent<<"\",\"seed\":"<<seed<<",\"games\":"<<games<<",\"wins\":"<<wins<<",\"losses\":"<<losses<<",\"draws\":"<<draws<<",\"win_rate\":"<<p<<",\"score_rate\":"<<(wins+0.5*draws)/games<<",\"ci95_wilson_marginal\":["<<center-half<<","<<center+half<<"],\"average_score_diff\":"<<scoreDiff/games<<",\"p50_ms\":"<<quantile(latency,.5)<<",\"p95_ms\":"<<quantile(latency,.95)<<",\"p99_ms\":"<<quantile(latency,.99)<<",\"max_ms\":"<<quantile(latency,1)<<",\"elapsed_seconds\":"<<std::chrono::duration<double>(Clock::now()-begin).count()<<",\"details\":["<<details<<"]}\n";
    std::cout<<wins<<"/"<<games<<" output="<<path<<"\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
}