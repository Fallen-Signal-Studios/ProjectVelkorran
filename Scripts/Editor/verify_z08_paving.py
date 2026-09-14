"""Fresh quarantine paving and preceding architecture verification."""
from pathlib import Path
import runpy,time
import unreal
root=Path(unreal.Paths.project_dir());DEFER_Z07_LIGHTING_AUTORUN=True
exec(compile((root/'Scripts/Editor/verify_z07_lighting.py').read_text(),'verify_z07_lighting','exec'),globals())
verify_z07_lighting_stage=verify
del DEFER_Z07_LIGHTING_AUTORUN
def verify():
    verify_z07_lighting_stage()
    geometry=runpy.run_path(str(root/'Scripts/Editor/check_z08_paving.py'))['check_z08_paving'](world,actors)
    assert len(actors)==3139 and z08_paving_count==204 and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    (out/'z08-paving-verification.json').write_text(json.dumps(dict(status='passed',actor_count=len(actors),geometry=geometry),indent=2))
if not globals().get('DEFER_Z08_PAVING_AUTORUN',False):
    unreal.EditorPythonScripting.set_keep_python_script_alive(True);started=time.monotonic()
    def tick(delta):
        if time.monotonic()-started<15:return
        unreal.unregister_slate_post_tick_callback(handle)
        try:verify()
        finally:unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    handle=unreal.register_slate_post_tick_callback(tick)
