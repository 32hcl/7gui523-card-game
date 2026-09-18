import os
import sys
import json
from sevenking_gym import SevenKingGymEnv

EXE = os.path.abspath("../build/env_server.exe")
N_GAMES = 20
LEVEL = "AI3_Tracker"

def pass_only_policy(env):
    obs, _ = env.reset()
    done = False
    while not done:
        mask = env.action_masks()
        # 找 pass 动作的 index
        if len(mask) == 0:
            break
        # 找到 pass 对应的 action
        # pass 动作 = 不选任何可选 action = action_space_size - 1
        # 遍历找
        pass_idx = None
        for i, m in enumerate(mask):
            if m == 1:
                pass_idx = i
        if pass_idx is None:
            # 找不到就选第一个（不可能是 pass）
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
    print(f"空策略 vs {LEVEL}：每次只选 pass")
    wins = losses = draws = 0
    for i in range(N_GAMES):
        env = SevenKingGymEnv(EXE, opponent_level=LEVEL)
        r = pass_only_policy(env)
        if r > 0:
            wins += 1
        elif r < 0:
            losses += 1
        else:
            draws += 1
        print(f"  [{i+1}/{N_GAMES}] reward={r}  w={wins} l={losses} d={draws}")
    print(f"\n空策略 {N_GAMES} 局 vs {LEVEL}: {wins}胜 {losses}负 {draws}平，胜率 {wins/N_GAMES*100:.1f}%")

if __name__ == "__main__":
    main()