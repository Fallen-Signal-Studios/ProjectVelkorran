"""Fresh reload of the complete architecture chain including breach-rescue coffers."""
from pathlib import Path
import runpy,time
import unreal
root=Path(unreal.Paths.project_dir());DEFER_Z06_PAVING_AUTORUN=True
exec(compile((root/'Scripts/Editor/verify_z06_paving.py').read_text(),'verify_z06_paving','exec'),globals())
verify_z06_floor_stage=verify
del DEFER_Z06_PAVING_AUTORUN

def verify():
    verify_z06_floor_stage()
    assert len(actors)==2761 and z06_ceiling_count==126
    geometry=runpy.run_path(str(root/'Scripts/Editor/check_z06_ceiling.py'))['check_z06_ceiling'](world,actors)
    assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    (out/'z06-ceiling-verification.json').write_text(json.dumps(dict(status='passed',actor_count=len(actors),geometry=geometry),indent=2))

unreal.EditorPythonScripting.set_keep_python_script_alive(True);started=time.monotonic()
def tick(delta):
    if time.monotonic()-started<15:return
    unreal.unregister_slate_post_tick_callback(handle)
    try:verify()
    finally:unreal.EditorPythonScripting.set_keep_python_script_alive(False)
handle=unreal.register_slate_post_tick_callback(tick)
