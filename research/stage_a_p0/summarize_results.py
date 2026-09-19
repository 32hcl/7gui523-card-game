"""Summarize the unchanged loss_analysis runner's paired before/after CSVs."""
import csv
import hashlib
import json
import math
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
SEEDS = [20260918, 20260919, 20260920]


def wilson(wins, n):
    z = 1.959963984540054
    p = wins / n
    denominator = 1 + z * z / n
    center = (p + z * z / (2 * n)) / denominator
    half = z * math.sqrt(p * (1 - p) / n + z * z / (4 * n * n)) / denominator
    return [center - half, center + half]


def summary(rows):
    wins = sum(r['result'] == 'WIN' for r in rows)
    losses = sum(r['result'] == 'LOSE' for r in rows)
    return dict(games=len(rows), wins=wins, losses=losses,
                draws=len(rows) - wins - losses, win_rate=wins / len(rows),
                win_rate_ci95_wilson=wilson(wins, len(rows)),
                average_score_difference=sum(int(r['ai4_score']) - int(r['opp_score'])
                                             for r in rows) / len(rows),
                first_player_games=sum(int(r['ai4_first']) for r in rows))


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


report = {
    'scope': ['A1', 'A2', 'A3'],
    'parameters': {'candidate': 'AI4_Expert', 'opponent': 'AI3_Tracker',
                   'engine': 'cheat_minimax', 'search_depth': 6,
                   'scoreDiffWeight': 10, 'tableScoreWeight': 8,
                   'handRankWeight': 2, 'handScoreWeight': 3, 'handSizeWeight': 20,
                   'terminal_utility_after': {'win': 10000, 'loss': -10000, 'draw': 0}},
    'protocol': {'seeds': SEEDS, 'games_per_seed_per_version': 200,
                 'game_seed': 'seed + game_id * 31337',
                 'seating': 'existing seeded random order; identical before/after',
                 'runner': 'tests/loss_analysis.cpp (unchanged)',
                 'metric': 'strict wins / games; draws are not counted as wins'},
    'limitations': [
        'Existing runner rules retained, including automatic special-hand wins and no pressure-bonus simulation.',
        'This measures P0 repair impact under the legacy runner, not a corrected full-rules AI strength baseline.',
        'A1 validated by sampled-world regression tests; cheat-versus-AI3 games do not measure sampled strength.',
        'P1 A4/A5/A6 intentionally not changed.',
        'No GUI compilation: Qt is not installed.'
    ],
    'build': {'compiler': 'MSVC 19.51.36257.0', 'configuration': 'Release /O2 /Ob2',
              'warnings': '/W3 /WX', 'gui': False,
              'targets': ['unit_tests', 'loss_analysis'],
              'warning_count': 0, 'unit_test_groups_passed': 13, 'unit_test_groups_total': 13},
    'versions': {},
    'source_sha256': {
        'before_minimax': sha(HERE / 'minimax.before.cpp.snapshot'),
        'before_sample_search': sha(HERE / 'sample_search.before.cpp.snapshot'),
        'after_minimax': sha(ROOT / 'ai/searcher/minimax.cpp'),
        'after_sample_search': sha(ROOT / 'ai/searcher/sample_search.cpp'),
        'runner': sha(ROOT / 'tests/loss_analysis.cpp'),
    },
}
all_rows = {}
for version in ['before', 'after']:
    batches = []
    combined = []
    for seed in SEEDS:
        with (HERE / f'{version}-{seed}.csv').open(encoding='utf-8-sig', newline='') as file:
            rows = list(csv.DictReader(file))
        if len(rows) != 200 or any(r['result'] not in ('WIN', 'LOSE', 'TIE') for r in rows):
            raise RuntimeError(f'Incomplete or invalid batch: {version} {seed}')
        batches.append({'seed': seed, **summary(rows), 'games_detail': rows})
        combined.extend(rows)
    all_rows[version] = combined
    report['versions'][version] = {'batches': batches, 'combined': summary(combined)}

paired_deltas = []
for before, after in zip(all_rows['before'], all_rows['after']):
    assert (before['seed'], before['ai4_first']) == (after['seed'], after['ai4_first'])
    paired_deltas.append(int(after['result'] == 'WIN') - int(before['result'] == 'WIN'))
n = len(paired_deltas)
delta = sum(paired_deltas) / n
variance = sum((x - delta) ** 2 for x in paired_deltas) / (n - 1)
half = 1.959963984540054 * math.sqrt(variance / n)
report['paired_comparison'] = {
    'win_rate_change': delta,
    'paired_normal_ci95_approximate': [delta - half, delta + half],
    'nonwin_to_win': paired_deltas.count(1), 'win_to_nonwin': paired_deltas.count(-1),
    'unchanged_win_indicator': paired_deltas.count(0),
}
for version in ['before', 'after']:
    for seed in SEEDS:
        batch = next(b for b in report['versions'][version]['batches'] if b['seed'] == seed)
        (HERE / f'{version}-{seed}.json').write_text(
            json.dumps({'parameters': report['parameters'], 'version': version, **batch},
                       ensure_ascii=False, indent=2), encoding='utf-8')
(HERE / 'results.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
print(json.dumps({v: report['versions'][v]['combined'] for v in ['before', 'after']}, indent=2))
print(json.dumps(report['paired_comparison'], indent=2))
