"""Fresh court and full preceding architecture chain."""
from pathlib import Path
import runpy,time
import unreal
root=Path(unreal.Paths.project_dir());DEFER_Z08_PAVING_AUTORUN=True
exec(compile((root/'Scripts/Editor/verify_z08_paving.py').read_text(),'verify_z08_paving','exec'),globals())
verify_z08_paving_stage=verify
del DEFER_Z08_PAVING_AUTORUN
def verify():
    verify_z08_paving_stage()
    geometry=runpy.run_path(str(root/'Scripts/Editor/check_z08_court.py'))['check_z08_court'](world,actors)
    runpy.run_path(str(root/'Scripts/Editor/audit_court_material_inputs.py'))
    assert len(actors)==3140 and z08_court_count==1 and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    (out/'z08-court-verification.json').write_text(json.dumps(dict(status='passed',actor_count=len(actors),geometry=geometry),indent=2))
if not globals().get('DEFER_Z08_COURT_AUTORUN',False):
    unreal.EditorPythonScripting.set_keep_python_script_alive(True);started=time.monotonic()
    def tick(delta):
        if time.monotonic()-started<15:return
        unreal.unregister_slate_post_tick_callback(handle)
        try:verify()
        finally:unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    handle=unreal.register_slate_post_tick_callback(tick)
