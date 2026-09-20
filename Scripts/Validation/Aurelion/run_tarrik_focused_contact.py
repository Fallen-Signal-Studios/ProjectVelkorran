"""Retry an earned E4A entry, then focus Tarrik through his public command API.

The contact observer supplies no player attack, leaving the living target
available for Tarrik. It observes actual damage, never grants it.
This controlled command probe is not passive follow-combat qualification.
"""
import os
from pathlib import Path
import runpy
import sys
import time
import unreal

here = Path(__file__).resolve().parent
sys.path.insert(0, str(here))
os.environ['SOV_EARNED_COMPANION_SAVE_DIRECTORY'] = str(Path(unreal.Paths.project_dir()) /
    'Saved/Validation/Aurelion/CampaignQualityRoute-20260919-180911-b8ff8cfb/UserData/Saved/SaveGames')
os.environ['SOV_CONTACT_BOUNDARY'] = 'M12_E4_QuarantineCrucibleA'
os.environ['SOV_CONTACT_FOLLOW_E4A'] = '0'
os.environ['SOV_CONTACT_RETRY_ENTRY'] = '1'
os.environ['SOV_CONTACT_COMMAND_ONLY'] = '1'
bootstrap = runpy.run_path(str(here / 'start_earned_companion_contact_probe.py'))
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
ended = None

def monitor(delta):
    global ended
    status = bootstrap['report']['status']
    if status in ('bootstrapping', 'loading'):
        return
    if status == 'probe_started':
        import observe_companion_after_player_shot as contact
        if contact._RUN.handle is not None:
            return
    if ended is None:
        ended = time.monotonic()
        unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
    elif time.monotonic()-ended > 2:
        unreal.unregister_slate_post_tick_callback(handle)
        unreal.EditorPythonScripting.set_keep_python_script_alive(False)

handle = unreal.register_slate_post_tick_callback(monitor)
