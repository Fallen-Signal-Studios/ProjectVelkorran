"""Fresh saved plinths and preceding architecture verification."""
from pathlib import Path
import json,runpy,time,unreal
root=Path(unreal.Paths.project_dir())
parent=(root/'Scripts/Editor/verify_z09_floor.py').read_text()
marker='unreal.EditorPythonScripting.set_keep_python_script_alive(True);started=time.monotonic()'
assert parent.count(marker)==1
exec(compile(parent.split(marker)[0],'verify_z09_floor','exec'),globals())
verify_z09_floor_stage=verify
def verify():
    verify_z09_floor_stage()
    result=runpy.run_path(str(root/'Scripts/Editor/check_z09_plinths.py'))['check_z09_plinths'](actors)
    assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    (out/'z09-plinth-verification.json').write_text(json.dumps(result,indent=2))
unreal.EditorPythonScripting.set_keep_python_script_alive(True);started=time.monotonic()
def tick(delta):
    if time.monotonic()-started<15:return
    unreal.unregister_slate_post_tick_callback(handle)
    try:verify()
    finally:unreal.EditorPythonScripting.set_keep_python_script_alive(False)
handle=unreal.register_slate_post_tick_callback(tick)
