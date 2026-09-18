import os, sys, json
import numpy as np
from sb3_contrib import MaskablePPO
from sb3_contrib.common.maskable.policies import MaskableActorCriticPolicy
from stable_baselines3.common.vec_env import DummyVecEnv
from sevenking_gym import SevenKingGymEnv

EXE = os.path.abspath("../build/env_server.exe")
BASE_MODEL = None  # 从零开始训练
OUT_DIR = "./models/vs_ai2"
LOG_FILE = "./serial_log_vs_ai2.json"
os.makedirs(OUT_DIR, exist_ok=True)

MAX_GROUPS = 20

PARAM_SETS = [
    ("A01", 3e-4, 0.01, 0.2, 0.99, 0.95, 10),
    ("A02", 1e-4, 0.01, 0.2, 0.99, 0.95, 10),
    ("A03", 5e-4, 0.01, 0.2, 0.99, 0.95, 10),
    ("A04", 3e-4, 0.001, 0.2, 0.99, 0.95, 10),
    ("A05", 3e-4, 0.05, 0.2, 0.99, 0.95, 10),
    ("A06", 3e-4, 0.01, 0.1, 0.99, 0.95, 10),
    ("A07", 3e-4, 0.01, 0.3, 0.99, 0.95, 10),
    ("A08", 3e-4, 0.01, 0.2, 0.95, 0.95, 10),
    ("A09", 3e-4, 0.01, 0.2, 0.99, 0.9, 10),
    ("A10", 3e-4, 0.01, 0.2, 0.99, 0.98, 10),
    ("A11", 3e-4, 0.01, 0.2, 0.99, 0.95, 5),
    ("A12", 3e-4, 0.01, 0.2, 0.99, 0.95, 20),
    ("A13", 1e-4, 0.001, 0.1, 0.99, 0.95, 10),
    ("A14", 5e-4, 0.05, 0.3, 0.99, 0.95, 10),
    ("A15", 2e-4, 0.02, 0.15, 0.99, 0.95, 10),
    ("A16", 3e-4, 0.01, 0.2, 0.98, 0.95, 10),
    ("A17", 3e-4, 0.02, 0.2, 0.99, 0.92, 10),
    ("A18", 1e-4, 0.05, 0.2, 0.99, 0.95, 15),
    ("A19", 4e-4, 0.005, 0.18, 0.99, 0.95, 10),
    ("A20", 3e-4, 0.01, 0.25, 0.99, 0.95, 12),
]

STEPS_PER_SET = 10000

def make_env():
    return SevenKingGymEnv(EXE, opponent_level="AI2_Rule")

def evaluate(model, n_games=60):
    env = SevenKingGymEnv(EXE, opponent_level="AI2_Rule")
    wins = losses = draws = 0
    for _ in range(n_games):
        obs, _ = env.reset()
        done = False
        while not done:
            mask = env.action_masks()
            action, _ = model.predict(obs, action_masks=mask, deterministic=True)
            obs, reward, done, truncated, info = env.step(int(action))
        if reward > 0: wins += 1
        elif reward < 0: losses += 1
        else: draws += 1
    env.close()
    return wins, losses, draws, wins / n_games

def main():
    env = DummyVecEnv([make_env])
    prev_path = BASE_MODEL
    results = []

    for i, (name, lr, ent, clip, gamma, lam, ne) in enumerate(PARAM_SETS):
        if i >= MAX_GROUPS:
            break

        print(f"\n=== [{i+1}/20] {name}: lr={lr}, ent={ent}, clip={clip}, "
              f"gamma={gamma}, lam={lam}, ne={ne} ===")

        if prev_path is None:
            print(f"[{name}] 从随机初始化开始")
            model = MaskablePPO(MaskableActorCriticPolicy, env, verbose=0)
        else:
            if not os.path.exists(prev_path):
                raise FileNotFoundError(f"模型文件不存在: {prev_path}")
            print(f"[{name}] 加载模型: {prev_path}")
            model = MaskablePPO.load(prev_path, env=env)
        model.learning_rate = lr
        model.ent_coef = ent
        model.clip_range = lambda _: clip
        model.gamma = gamma
        model.gae_lambda = lam
        model.n_epochs = ne

        model.learn(total_timesteps=STEPS_PER_SET)

        save_path = f"{OUT_DIR}/{name}.zip"
        model.save(save_path)
        print(f"[{name}] 保存到: {save_path}")

        w, l, d, wr = evaluate(model, n_games=60)
        print(f"评估 60 局：{w} 胜 {l} 负 {d} 平，胜率 {wr*100:.1f}%")

        results.append({
            "name": name, "lr": lr, "ent": ent, "clip": clip,
            "gamma": gamma, "lam": lam, "ne": ne,
            "wins": w, "losses": l, "draws": d, "win_rate": wr,
        })

        prev_path = save_path

    with open(LOG_FILE, "w") as f:
        json.dump(results, f, indent=2)

    print("\n=== 汇总 ===")
    print(f"{'组':<5}{'lr':<10}{'ent':<8}{'clip':<7}{'胜率':<8}")
    for r in results:
        print(f"{r['name']:<5}{r['lr']:<10}{r['ent']:<8}{r['clip']:<7}{r['win_rate']*100:.1f}%")

    best = max(results, key=lambda r: r["win_rate"])
    print(f"\n最佳：{best['name']}，胜率 {best['win_rate']*100:.1f}%")

if __name__ == "__main__":
    main()