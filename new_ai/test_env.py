import random
import sys
from sevenking_env import SevenKingEnv


def play_one_game(env):
    resp = env.reset()
    state = resp["observation"]
    total_steps = 0

    while True:
        actions = env.legal_actions()
        if not actions:
            print("没有任何合法动作，异常")
            return None

        action = random.choice(actions)

        resp = env.step(action)
        total_steps += 1

        if resp.get("done"):
            return {
                "reward": resp["reward"],
                "steps": total_steps,
                "winner": resp.get("winner"),
                "my_final": resp.get("my_final_score"),
                "opp_final": resp.get("opp_final_score"),
            }

        if total_steps > 500:
            print("超过 500 步，可能死循环")
            return None


def main():
    exe = sys.argv[1] if len(sys.argv) > 1 else "./build/env_server.exe"
    env = SevenKingEnv(exe)

    wins = 0
    losses = 0
    draws = 0

    for i in range(10):
        result = play_one_game(env)
        if result is None:
            print(f"第 {i+1} 局：异常结束")
            continue
        r = result["reward"]
        if r > 0:
            wins += 1
            tag = "赢"
        elif r < 0:
            losses += 1
            tag = "输"
        else:
            draws += 1
            tag = "平"
        print(f"第 {i+1} 局：{tag}  reward={r}  steps={result['steps']}  "
              f"my={result['my_final']}  opp={result['opp_final']}")

    print(f"\n总：{wins} 胜  {losses} 负  {draws} 平")
    env.close()


if __name__ == "__main__":
    main()