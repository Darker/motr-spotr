import asyncio
from asyncio.subprocess import Process
import datetime
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import time

from neural.dataset.binary_reading import FFTDataFile
from neural.neural_constants import DATA_RECORD, PROJECT_ROOT, SIGNAL_CLASSES
import simpleobsws

from neural.typing.sample_types import FFTLabels
from recording.obs.recording_constants import RECORDER_PATH, RECORDING_BINARY, RECORDING_JSON, RECORDING_MP4, WEBSOCKET_PW



ws = simpleobsws.WebSocketClient(
    url = 'ws://localhost:4455', password = WEBSOCKET_PW)

async def stop_process(p: Process):
    if sys.platform == "win32":
        # Graceful-ish on Windows
        p.terminate()
    else:
        # True Ctrl+C on POSIX
        p.send_signal(signal.SIGINT)

    try:
        await p.wait()
    except subprocess.TimeoutExpired:
        p.kill()

async def is_recording(ws: simpleobsws.WebSocketClient):
    request = simpleobsws.Request('GetRecordStatus') # Build a Request object

    ret = await ws.call(request) # Perform the request
    if not ret.ok(): # Check if the request succeeded
        raise Exception("Failed to query recording")
    print(ret.responseData)
    return ret.responseData["outputActive"] == True

async def wait_recording_state(ws: simpleobsws.WebSocketClient, state: bool, interval = 0.6):
    while (await is_recording(ws)) != state:
        await asyncio.sleep(interval)

async def make_request():
    loop = asyncio.get_running_loop()

    # This future will be completed when Ctrl+C arrives
    stop = loop.create_future()

    def handle_sigint(signum, frame):
        if not stop.done():
            stop.set_result(None)
    signal.signal(signal.SIGINT, handle_sigint)
    
    await ws.connect() # Make the connection to obs-websocket
    await ws.wait_until_identified() # Wait for the identification handshake to complete

    is_recording_now = await is_recording(ws)

    if is_recording_now:
        request = simpleobsws.Request('StopRecord') # Build a Request object

        ret = await ws.call(request)
        if not ret.ok(): 
            raise Exception("Failed to stop recording")
        
        # needs some time before stop and start
        await wait_recording_state(ws, False)

    request = simpleobsws.Request('StartRecord')

    ret = await ws.call(request)
    if not ret.ok():
        raise Exception("Failed to start recording")
    
    await wait_recording_state(ws, True)
    recording_start = datetime.datetime.now().astimezone().isoformat()
    
    # Start the FFT recorder
    command_full = str(RECORDER_PATH)
    args_all = ["--no-gui", f"--fftFile={RECORDING_BINARY}"]
    args_str = " ".join(args_all)
    print(f"Starting: \"{command_full}\" {args_str}")
    recorder_proc = await asyncio.create_subprocess_exec(
        command_full, *args_all,
        stdout=sys.stdout,
        stderr=sys.stderr,
    )

    async def wait_child():
        rc = await recorder_proc.wait()
        return ("child_exit", rc)
    
    process_end_promise = asyncio.create_task(wait_child())

    done, pending = await asyncio.wait(
        {stop, process_end_promise},
        return_when=asyncio.FIRST_COMPLETED
    )

    if stop in done:
        print("Ctrl+C received, cleaning up…")

        if sys.platform == "win32":
            recorder_proc.terminate()
        else:
            proc.send_signal(signal.SIGINT)

        try:
            await asyncio.wait_for(process_end_promise, timeout=5)
        except asyncio.TimeoutError:
            recorder_proc.kill()

    normal_end = True
    # Case 2: Child exited on its own
    if process_end_promise in done:
        reason, rc = process_end_promise.result()
        if rc == 0:
            print("Child finished normally")
        else:
            print(f"Child crashed or returned nonzero: {rc}")
            normal_end = False

    request = simpleobsws.Request('StopRecord') # Build a Request object

    ret = await ws.call(request) # Perform the request
    if not ret.ok(): # Check if the request succeeded
        raise Exception("Failed to stop recording")
    recording_end = datetime.datetime.now().astimezone().isoformat()
    print("Recording saved")
    print(ret.responseData)

    await wait_recording_state(ws, False)
    
    await ws.disconnect() # Disconnect from the websocket server cleanly
    data_valid = True
    test_file = None
    # validate recording
    try:
        test_file = FFTDataFile(RECORDING_BINARY)
        print(f"Num samples: {test_file.num_samples}")
    except ValueError as e:
        data_valid = False
        print(e)
    if test_file:
        test_file.fd.close()

    if data_valid:
        os.replace(ret.responseData["outputPath"], RECORDING_MP4)

        sample_desc: FFTLabels = {
            "classes": {},
            "recording": {
                "filename": RECORDING_BINARY.name,
                "end_time": recording_end,
                "start_time": recording_start
            }
        }

        for cls_name in SIGNAL_CLASSES:
            sample_desc["classes"][cls_name] = [
                {
                    "start": recording_start,
                    "end": recording_start
                }
            ]

        with open(RECORDING_JSON, "w") as json_file:
            json.dump(sample_desc, json_file, indent=4)
    else:
        os.unlink(ret.responseData["outputPath"])

asyncio.run(make_request())