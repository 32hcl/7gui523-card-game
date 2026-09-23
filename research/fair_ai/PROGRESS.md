# Fair AI research log

## Stage 0
Six bug repairs committed individually (A1 d11bfd0, A2 a4c89d0, A3 976cb95, A4 843c6f2, A5 d97214e, A6 533b96b).
Shared game/position.cpp implements confirmed v2 transition. Search, benchmark, env_server use it. README corrected. Legacy GUI round orchestration remains unchanged by scope; not validated as v2.
AI4 depth6 vs AI3: 588 wins / 11 losses / 1 draw, 600 games, 98.00%; descriptive Wilson 95% CI approximately 96.5–98.9%. Seeds 11000/21000/31000, paired seats. JSON files retain every result.
MSVC /W3 /WX Release headless builds passed; Qt paths unavailable. Tests now 15 groups, including 100 tiny endgames compared against terminal-depth minimax.
Training terminal labels now +1/0/-1. Legacy peek explicitly privileged; excluded from fair engine. AI1 keeps its original random-device behavior and is not a seeded research baseline.

## Stage 1
Fair1 observation-only interface, rank reconstruction at deck=0, alpha-beta with bound-aware transposition table and 900ms deadline.
Seed 41000, vs AI2: 17 wins / 179 losses / 4 draws, 8.5%, p50 .0212ms, p95 .0477ms, p99 .4701ms.
Matched original AI3 control: exactly 17/179/4. No measured win-rate benefit in this set. Most outcomes are already decided by the score gap before endgame.

## Stage 2 (in progress)
Particle filter conditions on public actions and private draws, mixture of AI2/AI3/conservative/scoring/random policies, likelihood floor and ESS resampling. On particle collapse, restart from current public counts; history loss is a known approximation.
Information-set UCB tree uses own observation/action-history keys; opponent actions use modeled policies with only that actor's own hand and public counts. Complete-world minimax ablation deliberately allows simulated-state lookahead but never receives actual hidden state.
Preliminary default results: Fair2 vs AI2 74/200, vs AI3 147/200; final rerun follows key-history correction and configuration serialization.

## Stage 2 completed (default configuration)
Frozen final tree-key implementation, 200 games per opponent, seed51000:
Fair2 vs AI2 74/200 wins, 4 draws (37%); vs AI3 148/200 wins, 2 draws (74%).
Complete-world minimax ablation vs AI2 69/200 wins, 4 draws (34.5%); vs AI3 160/200 wins, 6 draws (80%). These methods have different continuation evaluators, so this does not isolate strategy fusion alone.
Fair2 vs AI2 had 606 particle collapses/restarts over 200 games. This preserves legal public counts but loses accumulated history and model evidence; belief quality is a material limitation.

## Stage 3 skipped
The default search is below 50% vs AI2. Prioritize rollout/structure tuning before adding a CPU-trained neural model. No neural training or neural ablation was performed; no claim that neural methods intrinsically have zero benefit.

## Stage 4 in progress
20 starts, 200 games each on shared screening seed61000. Elite retention, three coordinate steps at each of three starting optima. Top5 use independent validation seed71000 vs AI2 and AI3. Select before holdout seeds81000/91000/101000. Each executable call times out after7100 seconds. Results cache checks binary SHA256 plus full parameter specification. Search ends at its declared finite budget, not a proof of global convergence.
