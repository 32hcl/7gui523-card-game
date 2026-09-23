#pragma once

#include <vector>
#include <memory>
#include <random>
#include "core/card/card.h"
#include "core/player.h"
#include "core/card/cardtype.h"
#include "core/card/deck.h"
#include "evaluator.h"
#include "searcher.h"
#include "sampler.h"
#include "tracker.h"
#include "policy.h"
#include "state_evaluator.h"

enum class EvaluatorType { Simple, Smart, Advanced };
enum class SearcherType { None, Minimax, IterativeDeepening };
enum class SamplerType { Perfect, Uniform, Particle };
enum class TrackerType { None, Basic };
enum class PolicyType { Greedy, TopK };
enum class StateEvaluatorType { Simple };

struct AIEngineConfig {
    EvaluatorType evaluatorType = EvaluatorType::Simple;
    SearcherType searcherType = SearcherType::None;
    SamplerType samplerType = SamplerType::Uniform;
    TrackerType trackerType = TrackerType::None;
    PolicyType policyType = PolicyType::Greedy;
    StateEvaluatorType stateEvaluatorType = StateEvaluatorType::Simple;

    int searchDepth = 6;
    int sampleCount = 30;
    int topK = 3;
    double randomness = 0.2;

    AIParams ai4Params;

    static AIEngineConfig defaultConfig();
};

class AIEngine {
public:
    explicit AIEngine(const AIEngineConfig& config);

    std::vector<Card> choosePlay(const Player& player,
                                  const Player& opponent,
                                  const CardTypeResult& previous,
                                  const Deck& deck,
                                  int tableScore);

    void recordPlayed(const std::vector<Card>& cards);

    void setOpponentHand(const std::vector<Card>& hand);

private:
    AIEngineConfig config_;
    std::unique_ptr<Evaluator> evaluator_;
    std::unique_ptr<Searcher> searcher_;
    std::unique_ptr<Sampler> sampler_;
    std::unique_ptr<Tracker> tracker_;
    std::unique_ptr<Policy> policy_;
    std::unique_ptr<StateEvaluator> stateEvaluator_;
    std::mt19937 rng_;
    std::vector<Card> opponentHand_;
    int turnsElapsed_ = 0;
};