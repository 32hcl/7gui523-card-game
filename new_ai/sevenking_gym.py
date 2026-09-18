import numpy as np
import gymnasium as gym
from gymnasium import spaces

from sevenking_env import SevenKingEnv
from state_encoder import encode_state, STATE_DIM
from action_space import ACTION_SPACE_SIZE, action_to_index_map, index_to_action_map
from abstract_to_concrete import abstract_to_concrete, concrete_to_abstract


class SevenKingGymEnv(gym.Env):
    metadata = {"render_modes": []}

    def __init__(self, exe_path, opponent_level="AI1_Simple"):
        super().__init__()
        self.env = SevenKingEnv(exe_path, opponent_level)
        self.observation_space = spaces.Box(
            low=-1.0, high=1.0, shape=(STATE_DIM,), dtype=np.float32
        )
        self.action_space = spaces.Discrete(ACTION_SPACE_SIZE)
        self.action_history = []
        self._current_legal_abstract = set()
        self._obs_dict = None

    def _build_obs(self):
        return np.array(encode_state(self._obs_dict, self.action_history), dtype=np.float32)

    def _update_legal(self):
        concrete_actions = self.env.legal_actions()
        self._current_legal_abstract = set()
        for cards in concrete_actions:
            if len(cards) == 0:
                self._current_legal_abstract.add("pass")
            else:
                key = concrete_to_abstract(cards)
                if key is not None:
                    self._current_legal_abstract.add(key)

    def action_masks(self):
        mask = np.zeros(ACTION_SPACE_SIZE, dtype=bool)
        for key in self._current_legal_abstract:
            if key in action_to_index_map:
                mask[action_to_index_map[key]] = True
        return mask

    def reset(self, seed=None, options=None):
        super().reset(seed=seed)
        resp = self.env.reset()
        self._obs_dict = resp["observation"]
        self.action_history = []
        self._update_legal()
        return self._build_obs(), {}

    def step(self, action_index):
        abstract_key = index_to_action_map[action_index]
        try:
            concrete = abstract_to_concrete(abstract_key, self._obs_dict["my_hand"])
        except (ValueError, KeyError):
            return self._build_obs(), -1.0, False, False, {"invalid": True}

        resp = self.env.step(concrete)
        self._obs_dict = resp["observation"]

        # track (my_play, opp_last_play) for action_history
        opp_cards = []
        lp = self._obs_dict.get("last_play")
        if lp is not None and isinstance(lp, dict):
            cards = lp.get("cards")
            if cards is not None:
                opp_cards = list(cards)
        self.action_history.append((concrete, opp_cards))

        reward = float(resp["reward"])
        done = bool(resp["done"])
        info = {}
        if done:
            info["winner"] = resp.get("winner")

        if not done:
            self._update_legal()

        return self._build_obs(), reward, done, False, info

    def close(self):
        self.env.close()