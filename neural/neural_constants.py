from pathlib import Path
from typing import TypedDict, Union, Optional, Literal, get_args
from collections.abc import Sequence


NEURAL_ROOT = Path(__file__).parent
PROJECT_ROOT = Path(__file__).parent.parent

DATA_ROOT = NEURAL_ROOT / "data"
DATA_RECORD =  DATA_ROOT / "recordings"
DATA_RECORD_TRN = DATA_RECORD / "private/train"
DATA_RECORD_VAL = DATA_RECORD / "private/val"

CURRENT_WEIGHTS_DIR = DATA_ROOT / "weights"
STABLE_WEIGHTS_DIR = DATA_ROOT / "weights_stable"

T_SIGNAL_CLASS = Literal["aku-bosh"]

SIGNAL_CLASSES: tuple[T_SIGNAL_CLASS, ...] = tuple(sorted(get_args(T_SIGNAL_CLASS)))

SignalClassList = Sequence[T_SIGNAL_CLASS]

NUM_FFT_MODEL = 8