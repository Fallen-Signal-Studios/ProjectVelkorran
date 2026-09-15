"""Fresh wound-gallery roof and preceding architecture verification."""
from pathlib import Path
import json,runpy,time,unreal
root=Path(unreal.Paths.project_dir());DEFER_Z09_PIERS_AUTORUN=True
exec(compile((root/'Scripts/Editor/verify_z09_piers.py').read_text(),'verify_z09_piers','exec'),globals())
del DEFER_Z09_PIERS_AUTORUN
verify_z09_piers_stage=verify
def verify():
    verify_z09_piers_stage()
    result=runpy.run_path(str(root/'Scripts/Editor/check_z09_ceiling.py'))['check_z09_ceiling'](actors)
    assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    (out/'z09-ceiling-verification.json').write_text(json.dumps(result,indent=2))
unreal.EditorPythonScripting.set_keep_python_script_alive(True);started=time.monotonic()
def tick(delta):
    if time.monotonic()-started<15:return
    unreal.unregister_slate_post_tick_callback(handle)
    try:verify()
    finally:unreal.EditorPythonScripting.set_keep_python_script_alive(False)
handle=unreal.register_slate_post_tick_callback(tick)
