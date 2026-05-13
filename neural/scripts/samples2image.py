

from neural.dataset.binary_reading import FFTDataFile, fft_samples_to_image_cv, read_fft_file
import cv2
import time
import json

from neural.neural_constants import DATA_RECORD_TRN, DATA_RECORD_VAL, DATA_ROOT

from datetime import datetime
import numpy as np

from neural.typing.sample_types import FFTLabels
from neural.util.sample_indexing import FFTLabelIndex


def load_intervals(json_data):
    intervals: list[tuple[float,float]] = []
    for cls, items in json_data["classes"].items():
        for it in items:
            start = datetime.fromisoformat(it["start"]).timestamp() * 1000
            end   = datetime.fromisoformat(it["end"]).timestamp() * 1000
            intervals.append((start, end))
    return intervals

def is_in_intervals(ts_ms, intervals):
    for start, end in intervals:
        if start <= ts_ms <= end:
            return True
    return False

def apply_interval_markers(colored_img, samples, intervals):
    h, w, _ = colored_img.shape
    marker_width = 16

    for row_idx, (ts_ms, fft_row) in enumerate(samples):
        color = (255, 255, 255) if is_in_intervals(ts_ms, intervals) else (0, 0, 0)
        colored_img[row_idx, w - marker_width : w] = color

if __name__ == "__main__":

    SAMPLE_PATH = DATA_RECORD_VAL / "val_test.bin"
    JSON_PATH = SAMPLE_PATH.with_suffix(".json")

    fft_size, samples = read_fft_file(SAMPLE_PATH)
    print(f"Num samples: {len(samples)}")
    print(f"FFT sample size: {fft_size}")
    print(time.gmtime(samples[0][0]/1000))
    print(time.gmtime(samples[len(samples)-1][0]/1000))

    with open(JSON_PATH, "r") as f:
        sample_info: FFTLabels = json.load(f)
    intervals = load_intervals(sample_info)
    print(intervals[0][0])
    print(time.gmtime(intervals[0][0]/1000))
    
    image = fft_samples_to_image_cv(samples)
    colored = cv2.applyColorMap(image, cv2.COLORMAP_INFERNO)

    apply_interval_markers(colored, samples, intervals)

    cv2.imwrite("fft_color_val.png", colored)

    fft_file = FFTDataFile(SAMPLE_PATH)
    print(f"Num samples: {len(fft_file)}")
    with open(JSON_PATH, "r") as f:
        sample_info: FFTLabels = json.load(f)

    index = FFTLabelIndex(sample_info)

    rows = [x.fft.copy() for x in fft_file]
    mat = np.vstack(rows)

    mat = (mat * 255).clip(0, 255).astype(np.uint8)
    colored = cv2.applyColorMap(mat, cv2.COLORMAP_INFERNO)

    h, w, _ = colored.shape
    marker_width = 16

    for row_idx, data in enumerate(fft_file):
        is_active = "aku-bosh" in index[data.timestamp]
        color = (255, 255, 255) if is_active else (0, 0, 0)
        colored[row_idx, w - marker_width : w] = color
    
    cv2.imwrite("fft_color_val_v2.png", colored)
