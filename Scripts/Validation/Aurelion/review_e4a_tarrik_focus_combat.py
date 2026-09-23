"""Observe earned E4A Tarrik companion combat with focus or autonomous intent.

The checkpoint bootstrap owns public load and normal retry input. This wrapper
adds passive animation evidence; SOV_TARRIK_AUTONOMOUS=1 omits FocusTarget.
"""
import os
from pathlib import Path
import runpy
import sys
import time

import unreal


HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import observe_companion_animation
import observe_companion_weapon_hit_path


os.environ.setdefault(
    'SOV_EARNED_COMPANION_SAVE_DIRECTORY',
    str(Path(unreal.Paths.project_dir()) /
        'Saved/Validation/Aurelion/CampaignQualityRoute-20260919-180911-b8ff8cfb/UserData/Saved/SaveGames'),
)
os.environ['SOV_CONTACT_BOUNDARY'] = 'M12_E4_QuarantineCrucibleA'
os.environ['SOV_CONTACT_RETRY_ENTRY'] = '1'
if os.environ.get('SOV_TARRIK_AUTONOMOUS') == '1':
    os.environ['SOV_CONTACT_PASSIVE_ONLY'] = '1'
else:
    os.environ['SOV_CONTACT_COMMAND_ONLY'] = '1'
if os.environ.get('SOV_TARRIK_SUSTAINED') == '1':
    os.environ['SOV_CONTACT_CONTINUE_AFTER_FIRST'] = '1'

observe_companion_animation.start('e4a-tarrik-animation.json')
observe_companion_weapon_hit_path.start('e4a-tarrik-hit-path.json')
bootstrap = runpy.run_path(str(HERE / 'start_earned_companion_contact_probe.py'))
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
finished = None


def monitor(delta):
    global finished
    status = bootstrap['report']['status']
    if status in ('bootstrapping', 'loading'):
        return
    if status == 'probe_started':
        import observe_companion_after_player_shot as contact
        probe = contact._RUN
        if probe and probe.handle is not None and time.monotonic() - probe.started < 75:
            return
        if probe and probe.handle is not None:
            probe.stop('75-second earned companion focus observation complete')
    if finished is None:
        finished = time.monotonic()
        unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
    elif time.monotonic() - finished > 2:
        unreal.unregister_slate_post_tick_callback(handle)
        unreal.EditorPythonScripting.set_keep_python_script_alive(False)


handle = unreal.register_slate_post_tick_callback(monitor)
