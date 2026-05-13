import torch
import torch.nn as nn
import torch.nn.functional as F

class LearnableWindow(nn.Module):
    def __init__(self, H, W):
        super().__init__()
        self.cx = nn.Parameter(torch.tensor(W/2))
        self.cy = nn.Parameter(torch.tensor(H/2))
        self.sx = nn.Parameter(torch.tensor(W/4))
        self.sy = nn.Parameter(torch.tensor(H/4))

    def forward(self, x):
        # x: [B, 1, H, W]
        B, C, H, W = x.shape
        yy, xx = torch.meshgrid(
            torch.arange(H, device=x.device),
            torch.arange(W, device=x.device),
            indexing="ij"
        )
        mask = torch.exp(-((xx - self.cx)**2 / (self.sx**2) +
                           (yy - self.cy)**2 / (self.sy**2)))
        return x * mask
