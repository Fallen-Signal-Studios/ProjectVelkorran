"""Fresh sideboard fit and preceding architecture checks."""
from pathlib import Path
import json,runpy,time,unreal
root=Path(unreal.Paths.project_dir());DEFER_Z08_BEDS_AUTORUN=True
exec(compile((root/'Scripts/Editor/verify_z08_beds.py').read_text(),'verify_beds','exec'),globals())
verify_z08_beds_stage=verify
del DEFER_Z08_BEDS_AUTORUN
def verify():
    verify_z08_beds_stage()
    result=runpy.run_path(str(root/'Scripts/Editor/check_z08_cabinet.py'))['check_z08_cabinet'](actors)
    result['access']=runpy.run_path(str(root/'Scripts/Editor/check_z08_cabinet.py'))['check_cabinet_access'](actors)
    assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    (out/'z08-cabinet-verification.json').write_text(json.dumps(dict(status='passed',actor_count=len(actors),settings=result),indent=2))
if not globals().get('DEFER_Z08_CABINET_AUTORUN',False):
    unreal.EditorPythonScripting.set_keep_python_script_alive(True);started=time.monotonic()
    def tick(delta):
        if time.monotonic()-started<15:return
        unreal.unregister_slate_post_tick_callback(handle)
        try:verify()
        finally:unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    handle=unreal.register_slate_post_tick_callback(tick)
