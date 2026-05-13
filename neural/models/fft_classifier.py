from collections.abc import Sequence

import torch
import torch.nn as nn
import torch.nn.functional as F


class FFTClassifier(nn.Module):
    def __init__(self, num_ffts: int, fft_size: int, class_names: Sequence[str]):
        super().__init__()

        assert num_ffts > 3, f"Input fft count must be larger than 3 to fit conv kernel"
        assert fft_size % 4 == 0, f"FFT size must be multiple of four"

        # This halves number of FFT buckets (FFT treated as grayscale image)
        # This also halves num fft
        self.input_preprocessing = nn.Sequential(
            nn.Conv2d(1, 1, kernel_size=3, padding=(0,1), stride=2),
            nn.ReLU(inplace=True),
        )

        # aplied on each FFT merged row (shared weights)
        self.linear_preprocesing = nn.Sequential(
            nn.Linear(fft_size//2, fft_size//4),
            nn.Dropout(0.1),
            nn.ReLU(inplace=True)
        )

        # merge all ffts
        self.num_rows_preprocessed = (num_ffts-1) // 2
        self.num_cols_preprocessed = fft_size // 4
        self.classes = sorted(class_names)

        self.trunk = nn.Sequential(
            nn.Linear(self.num_rows_preprocessed * self.num_cols_preprocessed, fft_size),
            nn.Dropout(0.1),
            nn.ReLU(inplace=True),
            nn.Linear(fft_size, 256),
            nn.ReLU(inplace=True),
            nn.Linear(256, len(self.classes)),
        )

    def forward(self, ffts: torch.Tensor):
        """
        ffts: [B, N_FFT, N_BUCKETS]
        """

        B = ffts.size(0)

        # Reshape to NCHW for Conv2d
        x = ffts.unsqueeze(1)              # [B, 1, num_ffts, fft_size]

        # 1) Convolution preprocessing
        x = self.input_preprocessing(x)    # [B, 1, H', W']
        # H' = (num_ffts - 1) // 2
        # W' = fft_size // 2

        # Remove channel dimension
        x = x.squeeze(1)                   # [B, H', W']

        # 2) Apply linear preprocessing to each row
        # Treat rows as batch items: merge B and H'
        B2, H2, W2 = x.shape               # H2 = num_rows_preprocessed, W2 = fft_size//2
        x = x.reshape(B * H2, W2)          # [B*H', W']

        x = self.linear_preprocesing(x)    # [B*H', W'' = fft_size//4]

        # Restore row structure
        x = x.reshape(B, H2, self.num_cols_preprocessed)   # [B, H', W'']

        # 3) Flatten all rows into one vector per sample
        x = x.reshape(B, -1)               # [B, H'*W'']

        # 4) Classification trunk
        x = self.trunk(x)                  # [B, num_classes]

        return x

if __name__ == "__main__":
    num_ffts = 8
    fft_size = 1024
    classes = ["a", "b", "c"]

    model = FFTClassifier(num_ffts, fft_size, classes)

    # mock batch of 4 samples
    mock = torch.randn(4, num_ffts, fft_size)

    out = model(mock)
    print("Final output:", out.shape)