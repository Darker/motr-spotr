from neural.neural_constants import DATA_RECORD, PROJECT_ROOT


RECORDER_PATH = PROJECT_ROOT / "build/MSVC 2022 Release - amd64 Ninja/adc_reader/adc_reader.exe"

WEBSOCKET_PORT = 4455
WEBSOCKET_PW = "oOCEYbnPC5ZoR0lY"

RECORDING_BINARY = DATA_RECORD / "current.bin"
RECORDING_JSON = RECORDING_BINARY.with_suffix(".json")
RECORDING_MP4 = RECORDING_BINARY.with_suffix(".mp4")