import os
import sys
import numpy as np
from sb3_contrib import MaskablePPO
from sb3_contrib.common.maskable.policies import MaskableActorCriticPolicy
from stable_baselines3.common.callbacks import CheckpointCallback
from stable_baselines3.common.vec_env import DummyVecEnv

from sevenking_gym import SevenKingGymEnv


EXE_PATH = os.path.abspath("../build/env_server.exe")
LOG_DIR = "./logs"
MODEL_DIR = "./models"
os.makedirs(LOG_DIR, exist_ok=True)
os.makedirs(MODEL_DIR, exist_ok=True)


def linear_schedule(initial_value):
    def func(progress):
        return progress * initial_value
    return func


def make_env():
    return SevenKingGymEnv(EXE_PATH)


def main():
    # 训练环境
    env = DummyVecEnv([make_env])

    # MaskablePPO 模型
    model = MaskablePPO(
        MaskableActorCriticPolicy,
        env,
        learning_rate=linear_schedule(3e-4),
        n_steps=4096,
        batch_size=64,
        n_epochs=10,
        gamma=0.99,
        gae_lambda=0.95,
        clip_range=0.2,
        ent_coef=0.05,
        target_kl=0.02,
        verbose=1,
    )

    # checkpoint 回调：每 5000 步保存一次
    checkpoint = CheckpointCallback(
        save_freq=10000,
        save_path=MODEL_DIR,
        name_prefix="sevenking_ppo",
    )

    print("开始训练...")
    model.learn(total_timesteps=20000, callback=checkpoint)
    model.save(os.path.join(MODEL_DIR, "sevenking_ppo_final"))
    print("训练完成")


if __name__ == "__main__":
    main()