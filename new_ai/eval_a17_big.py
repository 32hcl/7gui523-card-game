import os
import numpy as np
from sb3_contrib import MaskablePPO
from sevenking_gym import SevenKingGymEnv

EXE = os.path.abspath("../build/env_server.exe")
MODEL = "./models/vs_ai2/A17.zip"
N_GAMES = 200

def evaluate(model, n_games):
    env = SevenKingGymEnv(EXE, opponent_level="AI2_Rule")
    wins = losses = draws = 0
    for i in range(n_games):
        obs, _ = env.reset()
        done = False
        while not done:
            mask = env.action_masks()
            action, _ = model.predict(obs, action_masks=mask, deterministic=True)
            obs, reward, done, truncated, info = env.step(int(action))
        if reward > 0: wins += 1
        elif reward < 0: losses += 1
        else: draws += 1

        if (i + 1) % 50 == 0:
            print(f"  {i+1}/{n_games}: {wins}胜 {losses}负 {draws}平 胜率={wins/(i+1)*100:.1f}%")
    env.close()
    return wins, losses, draws

def main():
    model = MaskablePPO.load(MODEL)
    print(f"评估 A17 vs AI2，共 {N_GAMES} 局")
    w, l, d = evaluate(model, N_GAMES)
    wr = w / N_GAMES
    print()
    print(f"最终：{w}胜 {l}负 {d}平，胜率 {wr*100:.1f}%")

    if wr >= 0.65:
        print("A17 确实强，可以进入下一阶段")
    elif wr >= 0.55:
        print("A17 比 AI2 略强，但不是压倒性")
    else:
        print("70% 是噪声，A17 实际水平和 AI2 差不多")

if __name__ == "__main__":
    main()