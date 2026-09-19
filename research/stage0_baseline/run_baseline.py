"""Stage 0 baseline: AI4 vs AI3, 3 seeds × 200 games each."""
import subprocess
import json
import math
from pathlib import Path
from datetime import datetime

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
BUILD = ROOT / "build"
EXE = BUILD / "ai_battle.exe"

SEEDS = [20260918, 20260919, 20260920]
GAMES_PER_SEED = 200

def run_one_seed(seed: int) -> dict:
    """Run full tournament with given seed, return parsed results."""
    print(f"\n{'='*60}")
    print(f"Running seed {seed}, {GAMES_PER_SEED} games/combo...")
    print(f"{'='*60}")
    
    result = subprocess.run(
        [str(EXE), "--seed", str(seed), str(GAMES_PER_SEED)],
        capture_output=True, text=True,
        cwd=str(HERE)
    )
    
    # Parse stdout for the AI4 vs AI3 line
    # Format: "AI4     AI3     W       L       D       WR       Avg1     Avg2"
    ai4_vs_ai3 = {}
    for line in result.stdout.split('\n'):
        if line.strip().startswith('AI4') and 'AI3' in line:
            parts = line.split()
            if len(parts) >= 8:
                ai4_vs_ai3 = {
                    'seed': seed,
                    'first_wins': int(parts[2]),
                    'second_wins': int(parts[3]),
                    'draws': int(parts[4]),
                    'win_rate': float(parts[5]),
                    'avg_score_first': float(parts[6]),
                    'avg_score_second': float(parts[7]),
                }
            break
    
    # Also check battle_result.txt
    result_file = HERE / "battle_result.txt"
    if result_file.exists():
        content = result_file.read_text(encoding='utf-8')
        for line in content.split('\n'):
            if line.strip().startswith('AI4') and 'AI3' in line:
                parts = line.split()
                if len(parts) >= 8:
                    ai4_vs_ai3 = {
                        'seed': seed,
                        'first_wins': int(parts[2]),
                        'second_wins': int(parts[3]),
                        'draws': int(parts[4]),
                        'win_rate': float(parts[5]),
                        'avg_score_first': float(parts[6]),
                        'avg_score_second': float(parts[7]),
                    }
                break
    
    # Rename output file to avoid overwriting
    if result_file.exists():
        result_file.rename(HERE / f"battle_result_seed{seed}.txt")
    
    return ai4_vs_ai3


def wilson(wins: int, n: int) -> list:
    """Wilson 95% CI for binomial proportion."""
    z = 1.959963984540054
    p = wins / n
    denom = 1 + z * z / n
    center = (p + z * z / (2 * n)) / denom
    half = z * math.sqrt(p * (1 - p) / n + z * z / (4 * n * n)) / denom
    return [round(center - half, 4), round(center + half, 4)]


def main():
    HERE.mkdir(parents=True, exist_ok=True)
    
    results = []
    total_wins = 0
    total_losses = 0
    total_draws = 0
    
    for seed in SEEDS:
        r = run_one_seed(seed)
        if r:
            results.append(r)
            total_wins += r['first_wins']
            total_losses += r['second_wins']
            total_draws += r['draws']
            print(f"  Seed {seed}: AI4 wins={r['first_wins']}, AI3 wins={r['second_wins']}, "
                  f"draws={r['draws']}, WR={r['win_rate']:.4f}")
        else:
            print(f"  Seed {seed}: FAILED to parse results!")
    
    total_games = total_wins + total_losses + total_draws
    overall_wr = total_wins / total_games if total_games > 0 else 0
    ci = wilson(total_wins, total_games) if total_games > 0 else [0, 0]
    
    report = {
        'stage': 'stage0_baseline',
        'timestamp': datetime.now().isoformat(),
        'parameters': {
            'ai4': 'AI4_Expert (cheat minimax depth 6)',
            'ai3': 'AI3_Tracker',
            'seeds': SEEDS,
            'games_per_seed': GAMES_PER_SEED,
            'total_games': total_games,
        },
        'per_seed': results,
        'aggregate': {
            'ai4_wins': total_wins,
            'ai3_wins': total_losses,
            'draws': total_draws,
            'ai4_win_rate': round(overall_wr, 4),
            'ai4_win_rate_ci95_wilson': ci,
        },
    }
    
    output_path = HERE / "baseline_results.json"
    with open(output_path, 'w', encoding='utf-8') as f:
        json.dump(report, f, indent=2, ensure_ascii=False)
    
    print(f"\n{'='*60}")
    print(f"BASELINE COMPLETE")
    print(f"  AI4 wins: {total_wins}/{total_games} = {overall_wr:.2%}")
    print(f"  95% CI: [{ci[0]:.2%}, {ci[1]:.2%}]")
    print(f"  Results saved to: {output_path}")
    print(f"{'='*60}")


if __name__ == '__main__':
    main()