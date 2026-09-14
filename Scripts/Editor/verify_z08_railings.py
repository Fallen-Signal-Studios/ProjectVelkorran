"""Fresh-load guardrails and preceding architecture checks."""
from pathlib import Path
import json,runpy,time,unreal
root=Path(unreal.Paths.project_dir());DEFER_ECLIPSE_SCARS_AUTORUN=True
exec(compile((root/'Scripts/Editor/verify_eclipse_wall_scars.py').read_text(),'verify_scars','exec'),globals())
verify_eclipse_scars_stage=verify
del DEFER_ECLIPSE_SCARS_AUTORUN
def verify():
    verify_eclipse_scars_stage()
    result=runpy.run_path(str(root/'Scripts/Editor/check_z08_railings.py'))['check_z08_railings'](actors)
    assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    (out/'z08-railing-verification.json').write_text(json.dumps(dict(status='passed',actor_count=len(actors),settings=result),indent=2))
if not globals().get('DEFER_Z08_RAILINGS_AUTORUN',False):
    unreal.EditorPythonScripting.set_keep_python_script_alive(True);started=time.monotonic()
    def tick(delta):
        if time.monotonic()-started<15:return
        unreal.unregister_slate_post_tick_callback(handle)
        try:verify()
        finally:unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    handle=unreal.register_slate_post_tick_callback(tick)
