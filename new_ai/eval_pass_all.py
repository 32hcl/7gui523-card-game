import os
import sys
import json
from sevenking_gym import SevenKingGymEnv

EXE = os.path.abspath("../build/env_server.exe")
N_GAMES = 20

def pass_only_policy(env):
    obs, _ = env.reset()
    done = False
    while not done:
        mask = env.action_masks()
        if len(mask) == 0:
            break
        pass_idx = None
        for i, m in enumerate(mask):
            if m == 1:
                pass_idx = i
        if pass_idx is None:
            for i, m in enumerate(mask):
                if m == 1:
                    pass_idx = i
                    break
        if pass_idx is None:
            break
        obs, reward, done, truncated, info = env.step(pass_idx)
    env.close()
    return reward

def main():
    for level in ["AI1_Simple", "AI2_Rule", "AI3_Tracker"]:
        wins = losses = draws = 0
        for i in range(N_GAMES):
            env = SevenKingGymEnv(EXE, opponent_level=level)
            r = pass_only_policy(env)
            if r > 0: wins += 1
            elif r < 0: losses += 1
            else: draws += 1
        print(f"空策略 vs {level}: {wins}胜 {losses}负 {draws}平，胜率 {wins/N_GAMES*100:.1f}%")

if __name__ == "__main__":
    main()