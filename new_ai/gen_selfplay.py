import os
import sys
import json
import pickle
import numpy as np
from sevenking_env import SevenKingEnv

EXE = os.path.abspath("../build/env_server.exe")
OUT_FILE = "./selfplay_data_10k.pkl"
CKPT_FILE = "./selfplay_data_10k_partial.pkl"
N_GAMES = 10000


def play_one_game(env):
    resp = env._send({
        "command": "auto_play",
        "opp_ai": "AI2",
        "agent_ai": "AI2",
    })
    return resp["trajectory"]


def main():
    all_data = []

    if os.path.exists(CKPT_FILE):
        print(f"发现续跑检查点 {CKPT_FILE}，加载中...")
        with open(CKPT_FILE, "rb") as f:
            all_data = pickle.load(f)
        print(f"已加载 {len(all_data)} 条样本，继续生成")

    # 估算已完成的局数：每局约 29 个样本
    already_done = len(all_data) // 29 if len(all_data) > 0 else 0
    if already_done > 0:
        print(f"估计已完成 {already_done} 局，跳过")

    env = SevenKingEnv(EXE)

    for i in range(already_done, N_GAMES):
        traj = play_one_game(env)
        all_data.extend(traj)

        if (i + 1) % 100 == 0:
            print(f"{i+1}/{N_GAMES} 局，累计样本 {len(all_data)}")

        if (i + 1) % 1000 == 0:
            with open(CKPT_FILE, "wb") as f:
                pickle.dump(all_data, f)
            print(f"  检查点保存: {CKPT_FILE} ({len(all_data)} 条)")

    env.close()

    with open(OUT_FILE, "wb") as f:
        pickle.dump(all_data, f)
    print(f"\n完成。样本总数 {len(all_data)}，保存到 {OUT_FILE}")

    if os.path.exists(CKPT_FILE):
        os.remove(CKPT_FILE)
        print(f"清理检查点 {CKPT_FILE}")


if __name__ == "__main__":
    main()