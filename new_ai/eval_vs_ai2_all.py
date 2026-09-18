import os, sys, json
sys.path.insert(0, '.')
from sb3_contrib import MaskablePPO
from sevenking_gym import SevenKingGymEnv

EXE = os.path.abspath('../build/env_server.exe')
MODEL_DIR = './models/vs_ai2'
N_GAMES = 20
OUT = './eval_vs_ai2_results.json'

results = []
for i in range(1, 21):
    name = f'A{i:02d}'
    path = f'{MODEL_DIR}/{name}.zip'
    print(f'[{name}] 评估中...', flush=True)
    model = MaskablePPO.load(path)
    env = SevenKingGymEnv(EXE, opponent_level='AI2_Rule')
    w = l = d = 0
    for _ in range(N_GAMES):
        obs, _ = env.reset()
        done = False
        while not done:
            mask = env.action_masks()
            action, _ = model.predict(obs, action_masks=mask, deterministic=True)
            obs, reward, done, truncated, info = env.step(int(action))
        if reward > 0: w += 1
        elif reward < 0: l += 1
        else: d += 1
    env.close()
    wr = w / N_GAMES * 100
    print(f'  {name}: {w}W {l}L {d}D  ({wr:.0f}%)', flush=True)
    results.append({'name': name, 'wins': w, 'losses': l, 'draws': d, 'win_rate': wr})

with open(OUT, 'w') as f:
    json.dump(results, f, indent=2)

print(f'\n=== 汇总 ===')
print(f'{"组":<5}{"胜":<5}{"负":<5}{"平":<5}{"胜率":<8}')
for r in results:
    print(f'{r["name"]:<5}{r["wins"]:<5}{r["losses"]:<5}{r["draws"]:<5}{r["win_rate"]:.0f}%')
best = max(results, key=lambda r: r['win_rate'])
print(f'\n最佳: {best["name"]} ({best["win_rate"]:.0f}%)')
print(f'JSON: {OUT}')