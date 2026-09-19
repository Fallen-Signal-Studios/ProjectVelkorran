"""Run the controlled HUD fixture to completion under ExecutePythonScript.

Use run-editor-script.ps1 without KeepEntryOpen. This owns the temporary PIE
session, ends it after the fixture reports, and releases the Python keep-alive.
"""
import sys
import time
from pathlib import Path
import unreal

assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
sys.path.insert(0, str(Path(__file__).resolve().parent))
import probe_hud_gameplay_fixture as fixture

unreal.EditorPythonScripting.set_keep_python_script_alive(True)
ended = None


def monitor(delta):
    global ended
    if fixture.report['status'] == 'running':
        return
    if ended is None:
        ended = time.monotonic()
        unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
    elif time.monotonic() - ended > 2:
        unreal.unregister_slate_post_tick_callback(monitor_handle)
        unreal.EditorPythonScripting.set_keep_python_script_alive(False)


monitor_handle = unreal.register_slate_post_tick_callback(monitor)
