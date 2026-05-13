from pathlib import Path
from typing import TypedDict
import numpy as np
from numpy.typing import NDArray
from numpy.dtypes import Float64DType
from dataclasses import dataclass

import torch

from neural.neural_constants import T_SIGNAL_CLASS

@dataclass(frozen=True, kw_only=True)
class FFTSample:
    fft: NDArray[np.float64]
    timestamp: int

class TimeRange(TypedDict):
    start: str
    end: str

class RecordingInfo(TypedDict):
    filename: str

class FFTLabels(TypedDict):
    classes: dict[T_SIGNAL_CLASS, list[TimeRange]]
    recording: RecordingInfo

class FFTTensorData(TypedDict):
    samples: torch.Tensor # [N_WINDOW, N_FFT_WINDOWS]
    signal_classes: torch.Tensor # [N_CLASSES]