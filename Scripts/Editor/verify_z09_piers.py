"""Fresh saved pier variant and preceding architecture verification."""
from pathlib import Path
import json,runpy,time,unreal
root=Path(unreal.Paths.project_dir());DEFER_Z09_WALLS_AUTORUN=True
exec(compile((root/'Scripts/Editor/verify_z09_walls.py').read_text(),'verify_z09_walls','exec'),globals())
del DEFER_Z09_WALLS_AUTORUN
verify_z09_walls_stage=verify
def verify():
    verify_z09_walls_stage()
    result=runpy.run_path(str(root/'Scripts/Editor/check_z09_piers.py'))['check_z09_piers'](actors)
    assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    (out/'z09-piers-verification.json').write_text(json.dumps(result,indent=2))
if not globals().get('DEFER_Z09_PIERS_AUTORUN',False):
    unreal.EditorPythonScripting.set_keep_python_script_alive(True);started=time.monotonic()
    def tick(delta):
        if time.monotonic()-started<15:return
        unreal.unregister_slate_post_tick_callback(handle)
        try:verify()
        finally:unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    handle=unreal.register_slate_post_tick_callback(tick)
