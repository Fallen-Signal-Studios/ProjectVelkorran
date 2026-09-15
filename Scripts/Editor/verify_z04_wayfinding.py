"""Fresh full relay wayfinding and preceding architecture qualification."""
from pathlib import Path
import json,runpy,time,unreal
root=Path(unreal.Paths.project_dir());DEFER_Z04_CARGO_AUTORUN=True
exec(compile((root/'Scripts/Editor/verify_z04_cargo.py').read_text(),'verify_z04_cargo','exec'),globals())
verify_z04_cargo_stage=verify
del DEFER_Z04_CARGO_AUTORUN
def verify():
    verify_z04_cargo_stage()
    result=runpy.run_path(str(root/'Scripts/Editor/check_z04_wayfinding.py'))['check_z04_wayfinding'](actors)
    assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    (out/'z04-wayfinding-verification.json').write_text(json.dumps(dict(status='passed',actor_count=len(actors),settings=result),indent=2))
if not globals().get('DEFER_Z04_WAYFINDING_AUTORUN',False):
    unreal.EditorPythonScripting.set_keep_python_script_alive(True);started=time.monotonic()
    def tick(delta):
        if time.monotonic()-started<15:return
        unreal.unregister_slate_post_tick_callback(handle)
        try:verify()
        finally:unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    handle=unreal.register_slate_post_tick_callback(tick)
