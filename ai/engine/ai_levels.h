#pragma once

#include "ai_engine.h"

struct AILevelConfig {
    int level;
    const char* name;
    EvaluatorType evaluatorType;
    SearcherType searcherType;
    SamplerType samplerType;
    TrackerType trackerType;
    PolicyType policyType;
    int searchDepth;
    int sampleCount;
    int topK;
    double randomness;
    AIParams ai4Params;
};

inline AIEngineConfig buildAIEngineConfig(int level) {
    AILevelConfig cfg;
    cfg.level = level;
    cfg.ai4Params = AIParams();

    switch (level) {
        case 1:
            cfg.name = "电脑1";
            cfg.evaluatorType = EvaluatorType::Simple;
            cfg.searcherType = SearcherType::None;
            cfg.samplerType = SamplerType::Uniform;
            cfg.trackerType = TrackerType::None;
            cfg.policyType = PolicyType::TopK;
            cfg.searchDepth = 2;
            cfg.sampleCount = 5;
            cfg.topK = 5;
            cfg.randomness = 0.8;
            break;

        case 2:
            cfg.name = "电脑2";
            cfg.evaluatorType = EvaluatorType::Simple;
            cfg.searcherType = SearcherType::None;
            cfg.samplerType = SamplerType::Uniform;
            cfg.trackerType = TrackerType::None;
            cfg.policyType = PolicyType::TopK;
            cfg.searchDepth = 3;
            cfg.sampleCount = 10;
            cfg.topK = 3;
            cfg.randomness = 0.2;
            break;

        case 3:
            cfg.name = "电脑3";
            cfg.evaluatorType = EvaluatorType::Smart;
            cfg.searcherType = SearcherType::None;
            cfg.samplerType = SamplerType::Uniform;
            cfg.trackerType = TrackerType::None;
            cfg.policyType = PolicyType::Greedy;
            cfg.searchDepth = 4;
            cfg.sampleCount = 10;
            cfg.topK = 3;
            cfg.randomness = 0.2;
            break;

        case 4:
            cfg.name = "电脑4";
            cfg.evaluatorType = EvaluatorType::Smart;
            cfg.searcherType = SearcherType::None;
            cfg.samplerType = SamplerType::Uniform;
            cfg.trackerType = TrackerType::Basic;
            cfg.policyType = PolicyType::Greedy;
            cfg.searchDepth = 4;
            cfg.sampleCount = 15;
            cfg.topK = 3;
            cfg.randomness = 0.2;
            break;

        case 5:
            cfg.name = "电脑5";
            cfg.evaluatorType = EvaluatorType::Smart;
            cfg.searcherType = SearcherType::Minimax;
            cfg.samplerType = SamplerType::Uniform;
            cfg.trackerType = TrackerType::Basic;
            cfg.policyType = PolicyType::Greedy;
            cfg.searchDepth = 4;
            cfg.sampleCount = 15;
            cfg.topK = 3;
            cfg.randomness = 0.2;
            break;

        case 6:
            cfg.name = "电脑6";
            cfg.evaluatorType = EvaluatorType::Advanced;
            cfg.searcherType = SearcherType::Minimax;
            cfg.samplerType = SamplerType::Uniform;
            cfg.trackerType = TrackerType::Basic;
            cfg.policyType = PolicyType::Greedy;
            cfg.searchDepth = 5;
            cfg.sampleCount = 20;
            cfg.topK = 3;
            cfg.randomness = 0.2;
            break;

        case 7:
            cfg.name = "电脑7";
            cfg.evaluatorType = EvaluatorType::Advanced;
            cfg.searcherType = SearcherType::Minimax;
            cfg.samplerType = SamplerType::Uniform;
            cfg.trackerType = TrackerType::Basic;
            cfg.policyType = PolicyType::Greedy;
            cfg.searchDepth = 6;
            cfg.sampleCount = 20;
            cfg.topK = 3;
            cfg.randomness = 0.2;
            break;

        case 8:
            cfg.name = "电脑8";
            cfg.evaluatorType = EvaluatorType::Advanced;
            cfg.searcherType = SearcherType::Minimax;
            cfg.samplerType = SamplerType::Uniform;
            cfg.trackerType = TrackerType::Basic;
            cfg.policyType = PolicyType::TopK;
            cfg.searchDepth = 6;
            cfg.sampleCount = 25;
            cfg.topK = 3;
            cfg.randomness = 0.1;
            break;

        case 9:
            cfg.name = "电脑9";
            cfg.evaluatorType = EvaluatorType::Advanced;
            cfg.searcherType = SearcherType::Minimax;
            cfg.samplerType = SamplerType::Particle;
            cfg.trackerType = TrackerType::Basic;
            cfg.policyType = PolicyType::Greedy;
            cfg.searchDepth = 6;
            cfg.sampleCount = 25;
            cfg.topK = 3;
            cfg.randomness = 0.1;
            break;

        case 10:
            cfg.name = "电脑10";
            cfg.evaluatorType = EvaluatorType::Advanced;
            cfg.searcherType = SearcherType::Minimax;
            cfg.samplerType = SamplerType::Particle;
            cfg.trackerType = TrackerType::Basic;
            cfg.policyType = PolicyType::Greedy;
            cfg.searchDepth = 7;
            cfg.sampleCount = 30;
            cfg.topK = 3;
            cfg.randomness = 0.05;
            break;

        case 11:
            cfg.name = "电脑11";
            cfg.evaluatorType = EvaluatorType::Advanced;
            cfg.searcherType = SearcherType::Minimax;
            cfg.samplerType = SamplerType::Particle;
            cfg.trackerType = TrackerType::Basic;
            cfg.policyType = PolicyType::Greedy;
            cfg.searchDepth = 8;
            cfg.sampleCount = 30;
            cfg.topK = 3;
            cfg.randomness = 0.05;
            break;

        case 12:
            cfg.name = "电脑12";
            cfg.evaluatorType = EvaluatorType::Advanced;
            cfg.searcherType = SearcherType::Minimax;
            cfg.samplerType = SamplerType::Particle;
            cfg.trackerType = TrackerType::Basic;
            cfg.policyType = PolicyType::Greedy;
            cfg.searchDepth = 8;
            cfg.sampleCount = 40;
            cfg.topK = 3;
            cfg.randomness = 0.0;
            break;

        case 13:
            cfg.name = "电脑13";
            cfg.evaluatorType = EvaluatorType::Advanced;
            cfg.searcherType = SearcherType::Minimax;
            cfg.samplerType = SamplerType::Particle;
            cfg.trackerType = TrackerType::Basic;
            cfg.policyType = PolicyType::Greedy;
            cfg.searchDepth = 10;
            cfg.sampleCount = 40;
            cfg.topK = 3;
            cfg.randomness = 0.0;
            break;

        case 14:
            cfg.name = "电脑14";
            cfg.evaluatorType = EvaluatorType::Advanced;
            cfg.searcherType = SearcherType::Minimax;
            cfg.samplerType = SamplerType::Particle;
            cfg.trackerType = TrackerType::Basic;
            cfg.policyType = PolicyType::Greedy;
            cfg.searchDepth = 12;
            cfg.sampleCount = 50;
            cfg.topK = 3;
            cfg.randomness = 0.0;
            break;

        case 15:
            cfg.name = "电脑15";
            cfg.evaluatorType = EvaluatorType::Advanced;
            cfg.searcherType = SearcherType::Minimax;
            cfg.samplerType = SamplerType::Particle;
            cfg.trackerType = TrackerType::Basic;
            cfg.policyType = PolicyType::Greedy;
            cfg.searchDepth = 14;
            cfg.sampleCount = 60;
            cfg.topK = 3;
            cfg.randomness = 0.0;
            break;

        default:
            cfg.name = "未知";
            cfg.evaluatorType = EvaluatorType::Simple;
            cfg.searcherType = SearcherType::None;
            cfg.samplerType = SamplerType::Uniform;
            cfg.trackerType = TrackerType::None;
            cfg.policyType = PolicyType::Greedy;
            cfg.searchDepth = 4;
            cfg.sampleCount = 10;
            cfg.topK = 3;
            cfg.randomness = 0.2;
            break;
    }

    AIEngineConfig engineCfg;
    engineCfg.evaluatorType = cfg.evaluatorType;
    engineCfg.searcherType = cfg.searcherType;
    engineCfg.samplerType = cfg.samplerType;
    engineCfg.trackerType = cfg.trackerType;
    engineCfg.policyType = cfg.policyType;
    engineCfg.searchDepth = cfg.searchDepth;
    engineCfg.sampleCount = cfg.sampleCount;
    engineCfg.topK = cfg.topK;
    engineCfg.randomness = cfg.randomness;
    engineCfg.ai4Params = cfg.ai4Params;
    return engineCfg;
}

inline const char* getAILevelName(int level) {
    switch (level) {
        case 1: return "电脑1";
        case 2: return "电脑2";
        case 3: return "电脑3";
        case 4: return "电脑4";
        case 5: return "电脑5";
        case 6: return "电脑6";
        case 7: return "电脑7";
        case 8: return "电脑8";
        case 9: return "电脑9";
        case 10: return "电脑10";
        case 11: return "电脑11";
        case 12: return "电脑12";
        case 13: return "电脑13";
        case 14: return "电脑14";
        case 15: return "电脑15";
        default: return "未知";
    }
}