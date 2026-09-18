import os
import sys
from value_net_agent import ValueNetAgent
from sevenking_env import SevenKingEnv

EXE = os.path.abspath("../build/env_server.exe")
MODEL = "./value_net.pt"
N_GAMES = 50


def main():
    agent = ValueNetAgent(MODEL)
    env = SevenKingEnv(EXE, opponent_level="AI2_Rule")
    wins = losses = draws = 0

    for i in range(N_GAMES):
        env.reset()
        done = False
        steps = 0
        reward = 0
        while not done and steps < 200:
            action = agent.choose_action(env)
            if action is None:
                break
            resp = env.step(action)
            done = resp.get("done", False)
            if done:
                reward = resp.get("reward", 0)
            steps += 1
        if reward > 0:
            wins += 1
        elif reward < 0:
            losses += 1
        else:
            draws += 1
        print(f"{i+1}/{N_GAMES}: reward={reward}  累计 {wins}胜 {losses}负 {draws}平")

    env.close()
    wr = wins / N_GAMES
    print(f"\n最终胜率：{wr*100:.1f}% ({wins}胜 {losses}负 {draws}平)")


if __name__ == "__main__":
    main()