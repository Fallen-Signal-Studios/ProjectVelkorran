"""Fresh fitted Crucible piers and all preceding architecture checks."""
from pathlib import Path
import json,runpy,time,unreal
root=Path(unreal.Paths.project_dir());DEFER_CLIMB_SHADING_AUTORUN=True
exec(compile((root/'Scripts/Editor/verify_climb_surface_shading.py').read_text(),'verify_climb_surface_shading','exec'),globals())
verify_climb_shading_stage=verify
del DEFER_CLIMB_SHADING_AUTORUN

def verify():
    verify_climb_shading_stage()
    result=runpy.run_path(str(root/'Scripts/Editor/check_z08_columns.py'))['check_z08_columns'](actors)
    audit=runpy.run_path(str(root/'Scripts/Editor/audit_z08_presentation.py'))['audit_z08_presentation'](actors)
    (out/'z08-presentation-audit.json').write_text(json.dumps(audit,indent=2))
    assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    (out/'z08-columns-verification.json').write_text(json.dumps(dict(status='passed',actor_count=len(actors),settings=result),indent=2))
if not globals().get('DEFER_Z08_COLUMNS_AUTORUN',False):
    unreal.EditorPythonScripting.set_keep_python_script_alive(True);started=time.monotonic()
    def tick(delta):
        if time.monotonic()-started<15:return
        unreal.unregister_slate_post_tick_callback(handle)
        try:verify()
        finally:unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    handle=unreal.register_slate_post_tick_callback(tick)
