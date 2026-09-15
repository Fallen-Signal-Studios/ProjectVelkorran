"""Fresh full Z03 ceiling replacement and all earlier architecture checks."""
from pathlib import Path
import json,runpy,time,unreal
root=Path(unreal.Paths.project_dir());DEFER_SUPPORT_CLOSURES_AUTORUN=True
exec(compile((root/'Scripts/Editor/verify_support_closures.py').read_text(),'verify_support_closures','exec'),globals())
verify_support_closures_stage=verify
del DEFER_SUPPORT_CLOSURES_AUTORUN
def verify():
    verify_support_closures_stage()
    result=runpy.run_path(str(root/'Scripts/Editor/check_z03_ceiling.py'))['check_z03_ceiling'](actors)
    assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    (out/'z03-ceiling-verification.json').write_text(json.dumps(dict(status='passed',actor_count=len(actors),settings=result),indent=2))
if not globals().get('DEFER_Z03_CEILING_AUTORUN',False):
    unreal.EditorPythonScripting.set_keep_python_script_alive(True);started=time.monotonic()
    def tick(delta):
        if time.monotonic()-started<15:return
        unreal.unregister_slate_post_tick_callback(handle)
        try:verify()
        finally:unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    handle=unreal.register_slate_post_tick_callback(tick)
