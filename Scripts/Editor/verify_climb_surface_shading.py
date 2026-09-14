"""Fresh saved climb-shading correction and all preceding architecture checks."""
from pathlib import Path
import json,runpy,time,unreal
root=Path(unreal.Paths.project_dir());DEFER_WALL_CONTACT_AUTORUN=True
exec(compile((root/'Scripts/Editor/verify_wall_contact_bindings.py').read_text(),'verify_wall_contact_bindings','exec'),globals())
verify_wall_contact_stage=verify
del DEFER_WALL_CONTACT_AUTORUN

def verify():
    verify_wall_contact_stage()
    result=runpy.run_path(str(root/'Scripts/Editor/check_climb_surface_shading.py'))['check_climb_surface_shading'](actors)
    assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    (out/'climb-shading-verification.json').write_text(json.dumps(dict(status='passed',actor_count=len(actors),settings=result),indent=2))
if not globals().get('DEFER_CLIMB_SHADING_AUTORUN',False):
    unreal.EditorPythonScripting.set_keep_python_script_alive(True);started=time.monotonic()
    def tick(delta):
        if time.monotonic()-started<15:return
        unreal.unregister_slate_post_tick_callback(handle)
        try:verify()
        finally:unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    handle=unreal.register_slate_post_tick_callback(tick)
