

import torch

from neural.dataset.binary_reading import FFTDataFile, fft_samples_to_image_cv, read_fft_file
import cv2
import time
import json

from neural.models.fft_classifier import FFTClassifier
from neural.neural_constants import CURRENT_WEIGHTS_DIR, DATA_RECORD, DATA_RECORD_TRN, DATA_RECORD_VAL, DATA_ROOT, SIGNAL_CLASSES

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

    SAMPLE_PATH = DATA_RECORD / "current.bin"
    JSON_PATH = SAMPLE_PATH.with_suffix(".json")


    with open(JSON_PATH, "r") as f:
        sample_info: FFTLabels = json.load(f)

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
    fft_window_size = 8

    model = FFTClassifier(fft_window_size, fft_file.fft_size, SIGNAL_CLASSES)
    model.load_weights(CURRENT_WEIGHTS_DIR)
    model.eval()


    input = torch.zeros((1, fft_window_size, fft_file.fft_size))

    bosh_idx = SIGNAL_CLASSES.index("aku-bosh")

    for row_idx, fft_data in enumerate(fft_file):
        is_active = "aku-bosh" in index[fft_data.timestamp]
        color = (255, 255, 255) if is_active else (0, 0, 0)
        colored[row_idx, w - marker_width : w] = color
        # pred without batch is [N_CLASSES]
        

        if row_idx < fft_window_size:
            # still filling the initial window
            input[0, row_idx] = torch.from_numpy(fft_data.fft).to(torch.float32)
        else:
            model_pred = model(input).squeeze(0).cpu().detach()
            model_pred_prob = torch.sigmoid(model_pred)
            pred_bosh = model_pred_prob[bosh_idx].item()
            #  print(model_pred_prob)
            color_model = (0, 64*pred_bosh, 254*pred_bosh)
            colored[row_idx - fft_window_size//2, w - marker_width*2 : w-marker_width] = color_model

            # shift window left by 1
            input[0, :-1] = input[0, 1:].clone()

            # append newest FFT at the end
            input[0, -1] = torch.from_numpy(fft_data.fft).to(torch.float32)

    cv2.imwrite("fft_color.png", colored)
