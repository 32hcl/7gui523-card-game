#pragma once

#include "engine/ai_engine.h"

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

inline const char* getBossName(int level) {
    switch (level) {
        case 1: return "新手";
        case 2: return "换牌虫";
        case 3: return "急眼雀";
        case 4: return "疯狗";
        case 5: return "赖账鬼";
        case 6: return "不识数";
        default: return "未知";
    }
}