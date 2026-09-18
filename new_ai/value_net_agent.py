import numpy as np
import torch
import torch.nn as nn


class ValueNet(nn.Module):
    def __init__(self, state_dim):
        super().__init__()
        self.net = nn.Sequential(
            nn.Linear(state_dim, 256),
            nn.ReLU(),
            nn.BatchNorm1d(256),
            nn.Linear(256, 128),
            nn.ReLU(),
            nn.BatchNorm1d(128),
            nn.Linear(128, 64),
            nn.ReLU(),
            nn.Linear(64, 32),
            nn.ReLU(),
            nn.Linear(32, 1),
        )

    def forward(self, x):
        return self.net(x).squeeze(-1)


class ValueNetAgent:
    def __init__(self, model_path, state_dim=103):
        self.model = ValueNet(state_dim)
        ckpt = torch.load(model_path, map_location="cpu", weights_only=False)
        self.model.load_state_dict(ckpt["model"])
        self.model.eval()
        self.mean = torch.tensor(ckpt["mean"], dtype=torch.float32)
        self.std = torch.tensor(ckpt["std"], dtype=torch.float32)

    def choose_action(self, env):
        actions, next_states = env.peek()
        if not actions:
            return None
        with torch.no_grad():
            tensor = torch.tensor(next_states, dtype=torch.float32)
            tensor = (tensor - self.mean) / self.std
            values = self.model(tensor).numpy()
        best_idx = int(np.argmax(values))
        return actions[best_idx]