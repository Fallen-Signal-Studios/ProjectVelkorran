"""Run the normal E1 handoff, then check Selene's rendered shoulder output.

The camera probe starts only after the normal input driver releases its inputs
and reports a completed handoff. It never advances the mission journal.
"""
import json
import os
import runpy
import time
from pathlib import Path
import unreal

root=Path(__file__).resolve().parent
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
assert os.environ.get('SOV_AURELION_ENTRY_CONTINUE_E1')=='1'
assert os.environ.get('SOV_AURELION_E1_CONTINUE_ROUTE')!='1'
started=time.monotonic()

def tick(delta):
    report_path=out/'E1Continuation'/'e1-input-continuation.json'
    report=json.loads(report_path.read_text()) if report_path.exists() else {}
    if report.get('status')=='passed':
        unreal.unregister_slate_post_tick_callback(handle)
        runpy.run_path(str(root/'check_shoulder_camera_output.py'),init_globals={
            'ATTACH_TO_EXISTING_PIE':True,'EXPECTED_PAWN_CLASS':'BP_SovSelene_C'})
    elif report.get('status')=='failed' or time.monotonic()-started>720:
        unreal.unregister_slate_post_tick_callback(handle)
        (out/'shoulder-runtime.json').write_text(json.dumps(dict(
            status='not_run',reason='Normal E1 handoff failed or exceeded the observation bound',
            entry_status=report.get('status')),indent=2))
        unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
        unreal.EditorPythonScripting.set_keep_python_script_alive(False)

handle=unreal.register_slate_post_tick_callback(tick)
runpy.run_path(str(root/'observe_camera_route.py'))
runpy.run_path(str(root/'start_companion_mesh_route.py'))
