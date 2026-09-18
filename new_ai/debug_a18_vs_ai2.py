import os
import numpy as np
from sb3_contrib import MaskablePPO
from sevenking_gym import SevenKingGymEnv
from action_space import index_to_action_map
from abstract_to_concrete import abstract_to_concrete

EXE = os.path.abspath("../build/env_server.exe")
MODEL = "./models/serial/A18.zip"
N_GAMES = 10
OUT = "./debug_a18_vs_ai2.txt"


def format_state(obs_dict):
    lines = []
    lines.append(f"  我的手牌: {obs_dict.get('my_hand', [])}")
    lines.append(f"  对手手牌数: {obs_dict.get('opp_hand_size')}")
    lines.append(f"  桌面牌: {obs_dict.get('table_cards', [])}")
    lines.append(f"  桌面分: {obs_dict.get('table_score')}")
    lines.append(f"  我方总分: {obs_dict.get('my_score')} 对手总分: {obs_dict.get('opp_score')}")
    lines.append(f"  牌堆剩余: {obs_dict.get('deck_remaining')}")
    last = obs_dict.get('last_play')
    if last:
        lines.append(f"  上一手: {last}")
    else:
        lines.append(f"  上一手: 无（新回合）")
    return "\n".join(lines)


def log(msg, fp):
    fp.write(msg + "\n")
    fp.flush()


def play_one_game(model, env, game_idx, fp):
    log(f"\n{'='*60}", fp)
    log(f"局 {game_idx+1}", fp)
    log(f"{'='*60}", fp)

    obs, _ = env.reset()
    obs_dict = env._obs_dict
    step = 0

    while True:
        mask = env.action_masks()
        legal = [i for i, m in enumerate(mask) if m]

        log(f"\n--- step {step} ---", fp)
        log(format_state(obs_dict), fp)
        log(f"  合法动作索引: {legal[:10]}{'...' if len(legal) > 10 else ''} (共{len(legal)}个)", fp)

        action, _ = model.predict(obs, action_masks=mask, deterministic=True)
        action = int(action)
        key = index_to_action_map.get(action, "???")

        log(f"  [A18] 选了索引 {action} → {key}", fp)

        obs, reward, done, truncated, info = env.step(action)
        obs_dict = env._obs_dict
        step += 1

        if done:
            log(f"\n[终局] reward={reward} info={info}", fp)
            log(f"最终：我方 {obs_dict.get('my_score')} 对手 {obs_dict.get('opp_score')}", fp)
            return reward


def main():
    model = MaskablePPO.load(MODEL)
    env = SevenKingGymEnv(EXE, opponent_level="AI2_Rule")

    wins = losses = draws = 0
    with open(OUT, "w", encoding="utf-8") as fp:
        for i in range(N_GAMES):
            r = play_one_game(model, env, i, fp)
            if r > 0:
                wins += 1
            elif r < 0:
                losses += 1
            else:
                draws += 1

        log(f"\n{'='*60}", fp)
        log(f"总结：{wins} 胜 {losses} 负 {draws} 平", fp)

    env.close()
    print(f"结果：{wins} 胜 {losses} 负 {draws} 平")
    print(f"日志：{OUT}")


if __name__ == "__main__":
    main()