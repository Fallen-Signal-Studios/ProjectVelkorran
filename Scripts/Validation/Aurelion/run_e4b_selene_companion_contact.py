"""Replay earned E4B with an ordinary Tarrik shot and observe Selene's response."""
import os
from pathlib import Path
import runpy
import sys
import time
import unreal

here = Path(__file__).resolve().parent
sys.path.insert(0, str(here))
if os.environ.get('SOV_SELENE_SUSTAINED') == '1':
    import observe_companion_animation
    import observe_companion_weapon_hit_path
    os.environ['SOV_CONTACT_CONTINUE_AFTER_FIRST'] = '1'
    observe_companion_animation.start('e4b-selene-animation.json')
    observe_companion_weapon_hit_path.start('e4b-selene-hit-path.json')
os.environ.setdefault('SOV_EARNED_COMPANION_SAVE_DIRECTORY', str(Path(unreal.Paths.project_dir()) /
    'Saved/Validation/Aurelion/E4BAuthoredBodyAimRetest-20260922-201008-99c2607f/UserData/Saved/SaveGames'))
os.environ['SOV_CONTACT_BOUNDARY'] = 'M12_E4_QuarantineCrucibleB'
os.environ['SOV_CONTACT_FOLLOW_E4A'] = '0'
os.environ['SOV_CONTACT_RETRY_ENTRY'] = '1'
os.environ['SOV_CONTACT_COMMAND_ONLY'] = '0'
bootstrap = runpy.run_path(str(here / 'start_earned_companion_contact_probe.py'))
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
ended = None


def monitor(_delta):
    global ended
    status = bootstrap['report']['status']
    if status in ('bootstrapping', 'loading'):
        return
    if status == 'probe_started':
        import observe_companion_after_player_shot as contact
        if contact._RUN.handle is not None and (os.environ.get('SOV_SELENE_SUSTAINED') != '1'
                or time.monotonic() - contact._RUN.started < 75):
            return
        if contact._RUN.handle is not None:
            contact._RUN.stop('75-second earned Selene companion observation complete')
    if ended is None:
        ended = time.monotonic()
        unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
    elif time.monotonic() - ended > 2:
        unreal.unregister_slate_post_tick_callback(handle)
        unreal.EditorPythonScripting.set_keep_python_script_alive(False)


handle = unreal.register_slate_post_tick_callback(monitor)
