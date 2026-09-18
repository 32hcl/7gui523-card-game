import os
import sys
import numpy as np
from sb3_contrib import MaskablePPO

from sevenking_gym import SevenKingGymEnv

EXE_PATH = os.path.abspath("../build/env_server.exe")
MODEL_PATH = "./models/sevenking_ppo_final"


def evaluate(model, num_games=60):
    env = SevenKingGymEnv(EXE_PATH)
    wins = losses = draws = 0
    for i in range(num_games):
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
    print(f"评估 {num_games} 局：{wins} 胜 {losses} 负 {draws} 平")
    return wins / num_games


if __name__ == "__main__":
    model = MaskablePPO.load(MODEL_PATH)
    evaluate(model)