import obspython as obs
import time
import os

output_path = ""

def script_description():
    return "Writes a millisecond-precision timestamp to a text file every frame."

def script_properties():
    props = obs.obs_properties_create()
    obs.obs_properties_add_path(
        props,
        "path",
        "Output file",
        obs.OBS_PATH_FILE,
        "Text files (*.txt);;All files (*.*)",
        ""
    )
    return props

def script_update(settings):
    global output_path
    output_path = obs.obs_data_get_string(settings, "path")

def script_tick(delta):
    global output_path
    if not output_path:
        return

    t = time.time()
    ms = int(t * 1000)
    # human-readable + ms
    text = time.strftime("%H:%M:%S", time.localtime(t)) + f".{ms % 1000:03d}"
    # or epoch ms only:
    # text = str(ms)

    try:
        with open(output_path, "w", encoding="utf-8") as f:
            f.write(text)
    except OSError:
        pass
