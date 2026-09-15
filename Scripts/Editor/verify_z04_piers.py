"""Fresh relay piers and all preceding architecture checks."""
from pathlib import Path
import json, runpy, time, unreal
root = Path(unreal.Paths.project_dir())
DEFER_Z04_RECEIVERS_AUTORUN = True
exec(compile((root / 'Scripts/Editor/verify_z04_receivers.py').read_text(), 'verify_z04_receivers', 'exec'), globals())
verify_z04_receivers_stage = verify
del DEFER_Z04_RECEIVERS_AUTORUN

def verify():
    verify_z04_receivers_stage()
    result = runpy.run_path(str(root / 'Scripts/Editor/check_z04_piers.py'))['check_z04_piers'](actors)
    assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    (out / 'z04-piers-verification.json').write_text(json.dumps(dict(status='passed', actor_count=len(actors), settings=result), indent=2))

if not globals().get('DEFER_Z04_PIERS_AUTORUN', False):
    unreal.EditorPythonScripting.set_keep_python_script_alive(True)
    started = time.monotonic()
    def tick(delta):
        if time.monotonic()-started < 15:
            return
        unreal.unregister_slate_post_tick_callback(handle)
        try:
            verify()
        finally:
            unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    handle = unreal.register_slate_post_tick_callback(tick)
