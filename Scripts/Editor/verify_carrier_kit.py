"""Fresh custom carrier and preceding architecture checks."""
from pathlib import Path
import json,runpy,time,unreal
root=Path(unreal.Paths.project_dir())
DEFER_Z04_PIERS_AUTORUN=True
exec(compile((root/'Scripts/Editor/verify_z04_piers.py').read_text(),'verify_z04_piers','exec'),globals())
verify_z04_piers_stage=verify
del DEFER_Z04_PIERS_AUTORUN
def verify():
    verify_z04_piers_stage()
    settings=runpy.run_path(str(root/'Scripts/Editor/check_carrier_kit.py'))['check_carrier_kit'](actors)
    assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    (out/'carrier-verification.json').write_text(json.dumps(dict(status='passed',actor_count=len(actors),settings=settings),indent=2))
if not globals().get('DEFER_CARRIER_KIT_AUTORUN',False):
    unreal.EditorPythonScripting.set_keep_python_script_alive(True);started=time.monotonic()
    def tick(delta):
        if time.monotonic()-started<15:return
        unreal.unregister_slate_post_tick_callback(handle)
        try:verify()
        finally:unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    handle=unreal.register_slate_post_tick_callback(tick)
