"""Fresh refuge fit and preceding architecture checks."""
from pathlib import Path
import json,runpy,time,unreal
root=Path(unreal.Paths.project_dir());DEFER_Z08_REFUGE_AUTORUN=True
exec(compile((root/'Scripts/Editor/verify_z08_refuge.py').read_text(),'verify_refuge','exec'),globals())
verify_z08_refuge_stage=verify
del DEFER_Z08_REFUGE_AUTORUN
def verify():
    verify_z08_refuge_stage()
    result=runpy.run_path(str(root/'Scripts/Editor/check_refuge_presentation.py'))['check_refuge_presentation'](actors)
    assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    (out/'refuge-presentation-verification.json').write_text(json.dumps(dict(status='passed',actor_count=len(actors),settings=result),indent=2))
if not globals().get('DEFER_REFUGE_PRESENTATION_AUTORUN',False):
    unreal.EditorPythonScripting.set_keep_python_script_alive(True);started=time.monotonic()
    def tick(delta):
        if time.monotonic()-started<15:return
        unreal.unregister_slate_post_tick_callback(handle)
        try:verify()
        finally:unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    handle=unreal.register_slate_post_tick_callback(tick)
