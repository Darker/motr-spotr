from pathlib import Path
import shutil

from neural.neural_constants import DATA_RECORD_TRN, DATA_RECORD_VAL
from recording.obs.recording_constants import RECORDING_BINARY, RECORDING_JSON, RECORDING_MP4



def move_recording(name: str, is_train: bool):
    """Move current.bin/json/mp4 to train/val with new base name."""
    dest_dir = DATA_RECORD_TRN if is_train else DATA_RECORD_VAL
    dest_dir.mkdir(parents=True, exist_ok=True)

    mapping = {
        RECORDING_BINARY: dest_dir / f"{name}.bin",
        RECORDING_JSON:   dest_dir / f"{name}.json",
        RECORDING_MP4:    dest_dir / f"{name}.mp4",
    }

    for src, dst in mapping.items():
        if not src.exists():
            raise FileNotFoundError(f"Missing expected file: {src}")
        shutil.copy(src, dst)
        print(f"Moved {src.name} → {dst}")

    print("Done.")


if __name__ == "__main__":
    name = input("Enter recording name: ").strip()
    mode = input("Train or val [t/v]: ").strip().lower()

    if mode not in ("t", "v"):
        raise ValueError("Must choose 't' or 'v'")

    move_recording(name, is_train=(mode == "t"))
