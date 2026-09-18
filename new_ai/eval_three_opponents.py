import os
import numpy as np
from sb3_contrib import MaskablePPO
from sevenking_gym import SevenKingGymEnv

EXE = os.path.abspath("../build/env_server.exe")
MODEL_PATH = "./models/serial/A18.zip"
N_GAMES = 100

OPPONENTS = [
    ("AI1", "AI1_Simple"),
    ("AI2", "AI2_Rule"),
    ("AI3", "AI3_Tracker"),
]


def evaluate(model, opponent_level, n_games):
    env = SevenKingGymEnv(EXE, opponent_level=opponent_level)
    wins = losses = draws = 0
    for i in range(n_games):
        obs, _ = env.reset()
        done = False
        while not done:
            mask = env.action_masks()
            action, _ = model.predict(obs, action_masks=mask, deterministic=True)
            obs, reward, done, truncated, info = env.step(int(action))
        if reward > 0:
            wins += 1
        elif reward < 0:
            losses += 1
        else:
            draws += 1
    env.close()
    return wins, losses, draws, wins / n_games


def main():
    print(f"加载模型: {MODEL_PATH}")
    model = MaskablePPO.load(MODEL_PATH)

    print(f"\n{'对手':<8}{'胜':<6}{'负':<6}{'平':<6}{'胜率':<10}")
    print("-" * 40)
    results = []
    for name, level in OPPONENTS:
        w, l, d, wr = evaluate(model, level, N_GAMES)
        print(f"{name:<8}{w:<6}{l:<6}{d:<6}{wr*100:.1f}%")
        results.append((name, wr))

    print()
    print("=== 结论 ===")
    for name, wr in results:
        if wr >= 0.55:
            tag = "真强"
        elif wr >= 0.45:
            tag = "接近平衡"
        else:
            tag = "虚高（AI1 虐菜）"
        print(f"对 {name}: {wr*100:.1f}% —— {tag}")


if __name__ == "__main__":
    main()