"""Value Network 训练脚本：73维状态 → 标量价值（预测最终胜负）"""
import os
import pickle
import numpy as np
import torch
import torch.nn as nn
import torch.optim as optim
from torch.utils.data import DataLoader, TensorDataset

DATA_FILE = "./selfplay_data_10k.pkl"
OUT_MODEL = "./value_net.pt"
CKPT_FILE = "./value_net_ckpt.pt"
DEVICE = "cuda" if torch.cuda.is_available() else "cpu"
EPOCHS = 50
BATCH_SIZE = 128
LR = 0.001
VAL_RATIO = 0.2


class ValueNet(nn.Module):
    def __init__(self, input_dim=103):
        super().__init__()
        self.net = nn.Sequential(
            nn.Linear(input_dim, 256),
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
        return self.net(x)


def load_data(path):
    with open(path, "rb") as f:
        data = pickle.load(f)

    X = np.array([d["state"] for d in data], dtype=np.float32)
    y = np.array([d["cumulative_reward"] for d in data], dtype=np.float32).reshape(-1, 1)

    mean = X.mean(axis=0)
    std = X.std(axis=0) + 1e-8
    X = (X - mean) / std

    n = len(X)
    idx = np.random.permutation(n)
    n_val = int(n * VAL_RATIO)
    val_idx, train_idx = idx[:n_val], idx[n_val:]

    return (
        torch.tensor(X[train_idx]), torch.tensor(y[train_idx]),
        torch.tensor(X[val_idx]),  torch.tensor(y[val_idx]),
        mean, std,
    )


def main():
    X_train, y_train, X_val, y_val, mean, std = load_data(DATA_FILE)
    print(f"数据: 训练 {len(X_train)} / 验证 {len(X_val)}")
    print(f"设备: {DEVICE}")
    print(f"训练集 y: +1={ (y_train>0).sum().item() }  -1={ (y_train<0).sum().item() }  0={ (y_train==0).sum().item() }")

    train_ds = TensorDataset(X_train, y_train)
    val_ds = TensorDataset(X_val, y_val)
    train_loader = DataLoader(train_ds, batch_size=BATCH_SIZE, shuffle=True)
    val_loader = DataLoader(val_ds, batch_size=BATCH_SIZE)

    model = ValueNet().to(DEVICE)
    criterion = nn.HuberLoss(delta=1.0)
    optimizer = optim.Adam(model.parameters(), lr=LR, weight_decay=1e-5)

    start_epoch = 0
    best_val_loss = float("inf")
    best_epoch = 0

    if os.path.exists(CKPT_FILE):
        print(f"发现续训检查点 {CKPT_FILE}，加载中...")
        ckpt = torch.load(CKPT_FILE, map_location=DEVICE, weights_only=False)
        model.load_state_dict(ckpt["model"])
        optimizer.load_state_dict(ckpt["optimizer"])
        start_epoch = ckpt["epoch"]
        best_val_loss = ckpt["best_val_loss"]
        best_epoch = ckpt.get("best_epoch", start_epoch)
        print(f"从 epoch {start_epoch} 恢复，best_val_loss={best_val_loss:.4f}")

    for epoch in range(start_epoch, EPOCHS):
        model.train()
        train_loss = 0.0
        for bx, by in train_loader:
            bx, by = bx.to(DEVICE), by.to(DEVICE)
            optimizer.zero_grad()
            loss = criterion(model(bx), by)
            loss.backward()
            optimizer.step()
            train_loss += loss.item() * bx.size(0)
        train_loss /= len(train_ds)

        model.eval()
        val_loss = 0.0
        with torch.no_grad():
            for bx, by in val_loader:
                bx, by = bx.to(DEVICE), by.to(DEVICE)
                val_loss += criterion(model(bx), by).item() * bx.size(0)
        val_loss /= len(val_ds)

        improved = ""
        if val_loss < best_val_loss:
            best_val_loss = val_loss
            best_epoch = epoch + 1
            torch.save({"model": model.state_dict(), "mean": mean, "std": std}, OUT_MODEL)
            improved = " *"

        print(f"Epoch {epoch+1:2d}/{EPOCHS}  train_loss={train_loss:.4f}  val_loss={val_loss:.4f}{improved}")

        if (epoch + 1) % 5 == 0:
            torch.save({
                "model": model.state_dict(),
                "optimizer": optimizer.state_dict(),
                "epoch": epoch + 1,
                "best_val_loss": best_val_loss,
                "best_epoch": best_epoch,
            }, CKPT_FILE)
            print(f"  checkpoint 保存 epoch {epoch+1}")

    print(f"\n最佳 val_loss: {best_val_loss:.4f} (epoch {best_epoch})")
    print(f"模型保存: {OUT_MODEL}")

    if os.path.exists(CKPT_FILE):
        os.remove(CKPT_FILE)
        print(f"清理检查点 {CKPT_FILE}")


if __name__ == "__main__":
    main()