import numpy as np
from sevenking_gym import SevenKingGymEnv

env = SevenKingGymEnv("../build/env_server.exe")
obs, _ = env.reset()
print("obs shape:", obs.shape)  # expected (73,)
print("obs[:10]:", obs[:10])
print("legal mask sum:", env.action_masks().sum())

for i in range(20):
    mask = env.action_masks()
    legal_indices = np.where(mask)[0]
    if len(legal_indices) == 0:
        print(f"step {i}: no legal actions, breaking")
        break
    action = int(np.random.choice(legal_indices))
    obs, reward, done, truncated, info = env.step(action)
    print(f"step {i}: action={action} reward={reward} done={done}")
    if done:
        print("终局 info:", info)
        break

env.close()