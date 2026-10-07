#include "ai_engine.h"
#include "play_selector.h"
#include "../plugins/particle_sampler.h"
#include "ai/ai.h"
#include <utility>

AIEngineConfig AIEngineConfig::defaultConfig() {
    AIEngineConfig cfg;
    return cfg;
}

AIEngine::AIEngine(const AIEngineConfig& config)
    : config_(config), rng_(714025), turnsElapsed_(0) {
    switch (config_.evaluatorType) {
        case EvaluatorType::Simple:
            evaluator_ = std::make_unique<SimpleEvaluator>();
            break;
        case EvaluatorType::Smart:
            evaluator_ = std::make_unique<SmartEvaluator>();
            break;
        case EvaluatorType::Advanced:
            evaluator_ = std::make_unique<AdvancedEvaluator>(config_.ai4Params);
            break;
    }

    switch (config_.searcherType) {
        case SearcherType::None:
            searcher_ = std::make_unique<NoSearcher>();
            break;
        case SearcherType::Minimax:
        case SearcherType::IterativeDeepening: {
            if (config_.stateEvaluatorType == StateEvaluatorType::Simple) {
                stateEvaluator_ = std::make_unique<SimpleStateEvaluator>();
            }
            searcher_ = std::make_unique<MinimaxSearcher>(config_.searchDepth, stateEvaluator_.get());
            break;
        }
    }

    switch (config_.samplerType) {
        case SamplerType::Perfect:
            sampler_ = std::make_unique<PerfectSampler>(opponentHand_);
            break;
        case SamplerType::Uniform:
            sampler_ = std::make_unique<UniformSampler>();
            break;
        case SamplerType::Particle:
            sampler_ = std::make_unique<ParticleSampler>();
            break;
    }

    switch (config_.trackerType) {
        case TrackerType::None:
            tracker_ = std::make_unique<NoTracker>();
            break;
        case TrackerType::Basic:
            tracker_ = std::make_unique<BasicTracker>();
            break;
    }

    switch (config_.policyType) {
        case PolicyType::Greedy:
            policy_ = std::make_unique<GreedyPolicy>();
            break;
        case PolicyType::TopK:
            policy_ = std::make_unique<TopKPolicy>(config_.topK, config_.randomness);
            break;
    }
}

std::vector<Card> AIEngine::choosePlay(const Player& player,
                                        const Player& opponent,
                                        const CardTypeResult& previous,
                                        const Deck& deck,
                                        int tableScore) {
    const CardTracker* trackerPtr = tracker_->getImpl();

    auto allPlays = enumerateLegalPlays(player);
    if (allPlays.empty()) return {};

    auto finish = tryFinishPlay(player, allPlays, previous);
    if (!finish.empty()) return finish;

    auto intercept = tryEndgameIntercept(player, allPlays, previous, (int)opponent.hand.size());
    if (!intercept.empty()) return intercept;

    std::vector<Card> result;

    if (config_.searcherType == SearcherType::None) {
        // 采样一次并复用：保证所有候选在相同的随机对手世界下比较，消除噪声偏差
        std::vector<OpponentHandSample> sharedSamples;
        if (config_.samplerType != SamplerType::Perfect && trackerPtr) {
            sharedSamples = sampler_->sample(*trackerPtr, player.hand,
                                             (int)opponent.hand.size(),
                                             config_.sampleCount, turnsElapsed_);
        }
        std::vector<std::pair<std::vector<Card>, int>> scoredPlays;
        for (const auto& play : allPlays) {
            auto parsed = parseAnyCardType(play);
            if (!previous.cards.empty() && !canBeat(parsed, previous)) continue;

            int score;
            if (!sharedSamples.empty()) {
                int totalScore = 0;
                for (const auto& sample : sharedSamples) {
                    Player sampledOpponent;
                    sampledOpponent.hand = sample.hand;
                    sampledOpponent.totalScore = opponent.totalScore;
                    totalScore += evaluator_->evaluate(play, player, sampledOpponent, deck, previous, tableScore, trackerPtr, DeckSide::Boss);
                }
                score = totalScore / (int)sharedSamples.size();
            } else {
                score = evaluator_->evaluate(play, player, opponent, deck, previous, tableScore, trackerPtr, DeckSide::Boss);
            }
            scoredPlays.emplace_back(play, score);
        }
        result = policy_->select(scoredPlays, rng_);
    } else {
        result = searcher_->search(player, opponent, previous, deck, tableScore, trackerPtr, evaluator_.get(), DeckSide::Boss);
    }

    if (!result.empty()) {
        auto parsed = parseAnyCardType(result);
        if (!previous.cards.empty() && !canBeat(parsed, previous)) {
            return {};
        }
    }

    turnsElapsed_++;
    return result;
}

void AIEngine::recordPlayed(const std::vector<Card>& cards) {
    tracker_->recordPlayed(cards);
    turnsElapsed_++;
}

void AIEngine::recordPlayed(const std::vector<Card>& cards, DeckSide side) {
    tracker_->recordPlayed(cards, side);
    turnsElapsed_++;
}

void AIEngine::setOpponentHand(const std::vector<Card>& hand) {
    opponentHand_ = hand;
    if (config_.samplerType == SamplerType::Perfect) {
        sampler_ = std::make_unique<PerfectSampler>(opponentHand_);
    }
}