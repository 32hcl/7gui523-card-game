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
        // ── AI 人格配置（测试用，不参与关卡） ──
        case 100: {
            cfg.name = "搜索";
            cfg.evaluatorType = EvaluatorType::Smart;
            cfg.searcherType = SearcherType::Minimax;
            cfg.samplerType = SamplerType::Uniform;
            cfg.trackerType = TrackerType::Basic;
            cfg.policyType = PolicyType::TopK;
            cfg.searchDepth = 8;
            cfg.sampleCount = 60;
            cfg.topK = 3;
            cfg.randomness = 0.15;
            break;
        }
        case 101: {
            cfg.name = "贪心";
            cfg.evaluatorType = EvaluatorType::Advanced;
            cfg.searcherType = SearcherType::None;
            cfg.samplerType = SamplerType::Uniform;
            cfg.trackerType = TrackerType::Basic;
            cfg.policyType = PolicyType::TopK;
            cfg.searchDepth = 0;
            cfg.sampleCount = 35;
            cfg.topK = 2;
            cfg.randomness = 0.10;
            cfg.ai4Params.cardCountWeight = 40;
            cfg.ai4Params.finishBonus = 2500;
            cfg.ai4Params.tableScoreWeight = 3;
            cfg.ai4Params.stealThreshold = 6;
            cfg.ai4Params.stealMultiplier = 5;
            cfg.ai4Params.bombKeepPenalty = 30;
            cfg.ai4Params.rocketKeepPenalty = 200;
            cfg.ai4Params.splitPairPenalty = 8;
            cfg.ai4Params.earlyBigPenalty = 30;
            break;
        }
        case 102: {
            cfg.name = "保守";
            cfg.evaluatorType = EvaluatorType::Advanced;
            cfg.searcherType = SearcherType::None;
            cfg.samplerType = SamplerType::Uniform;
            cfg.trackerType = TrackerType::Basic;
            cfg.policyType = PolicyType::TopK;
            cfg.searchDepth = 0;
            cfg.sampleCount = 25;
            cfg.topK = 5;
            cfg.randomness = 0.25;
            cfg.ai4Params.cardCountWeight = 15;
            cfg.ai4Params.finishBonus = 1200;
            cfg.ai4Params.tableScoreWeight = 1;
            cfg.ai4Params.stealThreshold = 15;
            cfg.ai4Params.stealMultiplier = 3;
            cfg.ai4Params.bombKeepPenalty = 200;
            cfg.ai4Params.rocketKeepPenalty = 800;
            cfg.ai4Params.splitPairPenalty = 45;
            cfg.ai4Params.earlyBigPenalty = 80;
            cfg.ai4Params.specialKeepBonus = 150;
            cfg.ai4Params.endgameBombBonus = 400;
            cfg.ai4Params.endgameRocketBonus = 350;
            break;
        }
        default:
            cfg.name = "默认";
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
        case 7: return "黑老大";
        case 8: return "千里眼";
        case 9: return "赌场老板";
        default: return "未知";
    }
}