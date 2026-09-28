#include "ai/cfr/cfr_types.h"
#include "ai/cfr/cfr_trainer.h"
#include "ai/cfr/endgame_db.h"
#include "ai/cfr/strategy.h"
#include <iostream>
#include <fstream>
#include <random>
#include <string>
#include <chrono>
#include <cstdlib>

static void printUsage() {
    std::cout << "Usage: cfr_train [options]\n"
              << "Options:\n"
              << "  --endgame-iters N    Endgame DCFR iterations (default: 5000)\n"
              << "  --cfr-iters N        MCCFR iterations (default: 1000, needs --full-cfr)\n"
              << "  --checkpoint N       Checkpoint interval (default: 1000)\n"
              << "  --endgame-only       Only build endgame DB (default)\n"
              << "  --full-cfr           Enable Phase 2: External Sampling MCCFR\n"
              << "  --help               Show this help\n";
}

int main(int argc, char* argv[]) {
    int endgameIters = 5000;
    int cfrIters = 1000;
    int checkpointInterval = 1000;
    bool endgameOnly = true;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--endgame-iters" && i + 1 < argc) {
            endgameIters = std::atoi(argv[++i]);
        } else if (arg == "--cfr-iters" && i + 1 < argc) {
            cfrIters = std::atoi(argv[++i]);
        } else if (arg == "--checkpoint" && i + 1 < argc) {
            checkpointInterval = std::atoi(argv[++i]);
        } else if (arg == "--endgame-only") {
            endgameOnly = true;
        } else if (arg == "--full-cfr") {
            endgameOnly = false;
        } else if (arg == "--help" || arg == "-h") {
            printUsage();
            return 0;
        }
    }

    if (endgameIters < 1) endgameIters = 1;
    if (cfrIters < 1) cfrIters = 1;
    if (checkpointInterval < 1) checkpointInterval = 1;

    // Phase 1: Build endgame database
    std::cout << "=== Phase 1: Endgame DB ===\n";
    std::cout << "  Iterations: " << endgameIters << "\n";

    CFRParams egParams;
    egParams.iterations = endgameIters;

    EndgameDB endgameDB;

    auto t0 = std::chrono::steady_clock::now();
    endgameDB.build(egParams);
    auto t1 = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(t1 - t0).count();

    std::cout << "  Info sets: " << endgameDB.size() << "\n";
    std::cout << "  Time: " << elapsed << "s\n";

    endgameDB.save("data/cfr/endgame_db.bin");
    std::cout << "  Saved: data/cfr/endgame_db.bin\n\n";

    // Phase 2: MCCFR training
    if (!endgameOnly) {
        std::cout << "=== Phase 2: VR-MCCFR (Outcome Sampling + Baseline) ===\n";
        std::cout << "  Iterations: " << cfrIters << "\n";
        std::cout << "  Checkpoint interval: " << checkpointInterval << "\n";

        CFRParams params;
        params.iterations = cfrIters;
        params.checkpointInterval = checkpointInterval;

        CFRTrainer trainer(params, &endgameDB);
        std::mt19937 rng(42);

        auto ft0 = std::chrono::steady_clock::now();
        trainer.train(cfrIters, rng);
        auto ft1 = std::chrono::steady_clock::now();
        double ftElapsed = std::chrono::duration<double>(ft1 - ft0).count();

        auto& stats = trainer.stats();
        std::cout << "  Iterations: " << stats.iteration << "\n";
        std::cout << "  Info sets: " << (int)stats.infosetCoverage << "\n";
        std::cout << "  Avg entropy: " << stats.avgStrategyEntropy << "\n";
        std::cout << "  Max regret: "
                  << stats.avgPositiveRegret << "\n";
        std::cout << "  Time: " << ftElapsed << "s\n";

        trainer.saveCheckpoint("data/cfr/cfr_checkpoint.bin");
        std::cout << "  Saved: data/cfr/cfr_checkpoint.bin\n";

        trainer.exportAvgStrategy("data/cfr/avg_strategy.bin");
        std::cout << "  Saved: data/cfr/avg_strategy.bin\n";

        StrategyStore strategy;
        strategy.importFromNodes(trainer.nodes());
        strategy.save("data/cfr/strategy.bin");
        std::cout << "  Saved: data/cfr/strategy.bin\n";
    }

    std::cout << "\nDone.\n";
    return 0;
}