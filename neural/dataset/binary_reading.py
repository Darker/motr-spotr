
from dataclasses import dataclass
import os
from pathlib import Path
import struct
from threading import Lock

import numpy as np
from numpy.typing import NDArray
from numpy.dtypes import Float64DType

import torch

from neural.typing.sample_types import FFTSample

def read_fft_file(path):
    samples = []

    with open(path, "rb") as f:
        # ---- 1) Read FFT size (uint64 big-endian) ----
        header = f.read(8)
        if len(header) < 8:
            raise ValueError("File too short")
        (fft_size,) = struct.unpack(">Q", header)

        # ---- 2) Precompute struct formats ----
        ts_fmt = ">Q"                     # uint64 big-endian
        fft_fmt = ">" + "d" * fft_size    # fft_size doubles, big-endian
        ts_size = 8
        fft_size_bytes = fft_size * 8

        # ---- 3) Read all samples ----
        while True:
            ts_bytes = f.read(ts_size)
            if not ts_bytes:
                break  # EOF

            if len(ts_bytes) < ts_size:
                raise ValueError("Truncated timestamp")

            (timestamp,) = struct.unpack(ts_fmt, ts_bytes)

            data_bytes = f.read(fft_size_bytes)
            if len(data_bytes) < fft_size_bytes:
                raise ValueError("Truncated FFT data")

            fft_values = struct.unpack(fft_fmt, data_bytes)

            samples.append((timestamp, fft_values))

    return fft_size, samples

class FFTDataFile:
    def __init__(self, path: Path):
        self.filename = path
        self.fd = open(path, "rb")

        # Read FFT size
        header = self.fd.read(8)
        if len(header) < 8:
            raise ValueError("File too short")
        (fft_size,) = struct.unpack(">Q", header)
        self.fft_size = fft_size

        self.data_start = 8
        self.sample_size = 8 + 8 * fft_size

        self.fd.seek(0, os.SEEK_END)
        file_size = self.fd.tell()
        data_size = file_size - 8

        if data_size % self.sample_size != 0:
            raise ValueError("File size does not match sample size")

        self.num_samples = data_size // self.sample_size

        # Predefine dtype for fast reads
        self.fft_dtype = np.dtype(">f8")   # big-endian float64

        self.file_lock = Lock()

    def __len__(self):
        return self.num_samples

    def __getitem__(self, key: int):
        if key >= self.num_samples:
            raise IndexError

        offset = self.data_start + key * self.sample_size
        with self.file_lock:
            self.fd.seek(offset)

            # Read timestamp
            ts_bytes = self.fd.read(8)
            timestamp: int
            (timestamp,) = struct.unpack(">Q", ts_bytes)

            # Read FFT block
            fft_bytes = self.fd.read(8 * self.fft_size)

        # Convert to numpy array of big-endian float64
        fft = np.frombuffer(fft_bytes, dtype=self.fft_dtype, count=self.fft_size)
        return FFTSample(fft=fft.astype(np.float64, copy=False), timestamp=timestamp)
    
    def get_timestamp_at(self, index: int):
        offset = self.data_start + index * self.sample_size
        with self.file_lock:
            self.fd.seek(offset)

            # Read timestamp
            ts_bytes = self.fd.read(8)
            timestamp: int
            (timestamp,) = struct.unpack(">Q", ts_bytes)
            return timestamp
    
    def __iter__(self):
        for i in range(0, self.num_samples):
            yield self[i]

    def make_tensor(self, offset: int, window_size: int):
        if offset + window_size > self.num_samples:
            raise IndexError
        
        all = torch.zeros((window_size, self.fft_size))
        for i in range(offset, offset + window_size):
            fft_data = self[i]
            all[i-offset] = torch.from_numpy(fft_data.fft).to(torch.float32)
        return all


def fft_samples_to_image_cv(samples):
    rows = [np.array(fft, dtype=np.float64) for (_, fft) in samples]
    mat = np.vstack(rows)

    # Normalize to 0–255
    # mat = mat - mat.min()
    # if mat.max() > 0:
    #     mat = mat / mat.max()
    mat = (mat * 255).astype(np.uint8)

    return mat

