"""Fresh saved capture assembly and full preceding architecture chain."""
from pathlib import Path
import runpy,time
import unreal
root=Path(unreal.Paths.project_dir());DEFER_Z07_WALL_AUTORUN=True
exec(compile((root/'Scripts/Editor/verify_z07_walls.py').read_text(),'verify_z07_walls','exec'),globals())
verify_z07_walls_stage=verify
del DEFER_Z07_WALL_AUTORUN
def verify():
    verify_z07_walls_stage()
    geometry=runpy.run_path(str(root/'Scripts/Editor/check_z07_capture.py'))['check_z07_capture'](world,actors)
    assert len(actors)==2923+z07_light_count+z08_paving_count and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    (out/'z07-capture-verification.json').write_text(json.dumps(dict(status='passed',actor_count=len(actors),geometry=geometry),indent=2))
if not globals().get('DEFER_Z07_CAPTURE_AUTORUN',False):
    unreal.EditorPythonScripting.set_keep_python_script_alive(True);started=time.monotonic()
    def tick(delta):
        if time.monotonic()-started<15:return
        unreal.unregister_slate_post_tick_callback(handle)
        try:verify()
        finally:unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    handle=unreal.register_slate_post_tick_callback(tick)
