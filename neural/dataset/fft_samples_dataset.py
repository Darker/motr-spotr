import dataclasses
import hashlib
import json
from pathlib import Path
import time
from typing import Literal, Optional, Tuple, List, Dict, Any, TypedDict, cast, Union


import torch
import torch.nn.functional as F
import requests
from functools import lru_cache

from jakubs_neural_util.datasets.cached_dataset import CachedDataset

from neural.dataset.binary_reading import FFTDataFile
from neural.neural_constants import SIGNAL_CLASSES
from neural.typing.sample_types import FFTLabels, FFTTensorData
from neural.util.sample_indexing import FFTLabelIndex

@dataclasses.dataclass
class IndexedReader:
    reader: FFTDataFile
    index: int
    # How many full windows can we generate without padding at the end end beginning
    num_windows: int
    start_index: int

    loaded_labels: Optional[FFTLabelIndex]



def window_center(i: int, window_size: int) -> int:
    if window_size % 2 == 1:
        c = (window_size - 1) // 2
    else:
        c = window_size // 2  # right-center
    return i + c

def num_windows(total_samples: int, window_size: int) -> int:
    return total_samples - window_size + 1

@dataclasses.dataclass(frozen=True, kw_only=True)
class SampleCoords:
    reader_path: Path
    reader_sample_index: int

class FFTSampleDataset(CachedDataset[SampleCoords, None, FFTTensorData]):
    def __init__(self,
                 dir: Path,
                 window_size: int,
                 *,
                 recursive_glob: bool = False,
                 cache_folder: str = "",
                 subrange: Optional[Tuple[float, float]] = None,
                 subrange_is_percent: bool = False,
                 shuffle_seed: int = -1,
                 use_gpu: bool = False
                 ):
        """
        Args:
            dir (str): Path to the folder containing *.json files.
            window_size (int): how many FFT samples in each training record. 
        """
        super().__init__(
                        cache_dir=cache_folder,
                        subrange=subrange,
                        subrange_is_percent=subrange_is_percent,
                        shuffle_seed=shuffle_seed)

        self.FORMAT_VERSION = 2

        self.use_gpu = use_gpu
        self.torch_device = torch.device("cuda" if torch.cuda.is_available() and self.use_gpu else "cpu")

        self.folder = dir
        self.recursive_glob = recursive_glob
        self.open_files: dict[Path, IndexedReader] = {}
        self.window_size = window_size

        self.total_len = -1
        self.fft_size = -1
        self.files: list[Path] = []
        self._searched = False


    def count_items(self):
        if not self._searched:
            globres = (
                self.folder.glob("*.json")
                if not self.recursive_glob
                else self.folder.rglob("*.json")
            )

            # Keep only JSON files that have a matching .bin file
            self.files = sorted(
                f for f in globres
                if f.with_suffix(".bin").exists()
            )


    def get_real_len(self) -> int:
        if self.total_len >= 0:
            return self.total_len
        if not self._searched:
            self.count_items()

        total_len = 0

        # Open all data files
        for i, path in enumerate(self.files):
            reader = FFTDataFile(path.with_suffix(".bin"))
            reader_wrapper = IndexedReader(reader, i, 0, total_len, None)
            sample_count = len(reader)
            reader_wrapper.num_windows = sample_count - self.window_size + 1

            self.open_files[path] = reader_wrapper
            total_len += reader_wrapper.num_windows

            if self.fft_size == -1:
                self.fft_size = reader.fft_size
            else:
                assert self.fft_size == reader.fft_size
        self.total_len = total_len
        return total_len

    def init_items(self):
        pass

    def get_item_source_impl(self, idx: int) -> SampleCoords:
        # calculate which reader
        for reader_file, reader_info in self.open_files.items():
            start = reader_info.start_index
            end = start + reader_info.num_windows

            if start <= idx < end:
                index_in_reader = idx - start
                return SampleCoords(reader_path=reader_file, reader_sample_index=index_in_reader)

        raise IndexError

    def get_item_info(self, item: SampleCoords) -> Tuple[None, List[Path] | None]:
        return (None, [item.reader_path, item.reader_path.with_suffix(".bin")])

    def load_item(self, item: SampleCoords) -> FFTTensorData:
        # get samples
        fft_tensor = torch.zeros((self.window_size, self.fft_size), dtype=torch.float32)
        # classes_tensor = torch.zeros((self.window_size, len(SIGNAL_CLASSES)), dtype=torch.float32)
        class_counts = [0.0] * len(SIGNAL_CLASSES)
        reader_info = self.open_files[item.reader_path]
        label_info = self.get_labels_map(reader_info)

        for idx in range(0, self.window_size):
            fft_data = reader_info.reader[idx + item.reader_sample_index]
            fft_tensor[idx] = torch.from_numpy(fft_data.fft).to(torch.float32)
            # returns list of strings
            active_classes = label_info[fft_data.timestamp]
            for cls_idx, name in enumerate(SIGNAL_CLASSES):
                if name in active_classes:
                    # classes_tensor[idx, cls_idx] = 1.0
                    class_counts[cls_idx] += 1.0

        classes_total_tensor = torch.zeros((len(SIGNAL_CLASSES)), dtype=torch.float32)
        cutoff = self.window_size*0.3

        for cls_idx, count in enumerate(class_counts):
            frac = count / self.window_size
            classes_total_tensor[cls_idx] = 1.0 if frac >= cutoff else 0.0

        return {
            "samples": fft_tensor,
            "signal_classes": classes_total_tensor
        }
    
    def get_labels_map(self, reader_info: IndexedReader) -> FFTLabelIndex:
        if reader_info.loaded_labels is None:
            with open(reader_info.reader.filename.with_suffix(".json")) as f_labels:
                json_data: FFTLabels = json.load(f_labels)
            reader_info.loaded_labels = FFTLabelIndex(json_data)
        return reader_info.loaded_labels

    def __getstate__(self):
        state = self.__dict__.copy()
        state["open_files"] = {}
        state["total_len"] = -1
        return state

