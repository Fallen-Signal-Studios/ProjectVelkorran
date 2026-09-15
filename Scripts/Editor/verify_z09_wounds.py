"""Fresh saved wound overlays and preceding architecture verification."""
from pathlib import Path
import json,runpy,time,unreal
root=Path(unreal.Paths.project_dir())
parent=(root/'Scripts/Editor/verify_z09_plinths.py').read_text()
marker='\nunreal.EditorPythonScripting.set_keep_python_script_alive(True);started=time.monotonic()'
assert parent.count(marker)==1
exec(compile(parent.split(marker)[0],'verify_z09_plinths','exec'),globals())
verify_z09_plinth_stage=verify
def verify():
    verify_z09_plinth_stage()
    result=runpy.run_path(str(root/'Scripts/Editor/check_z09_wounds.py'))['check_z09_wounds'](actors)
    assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    (out/'z09-wounds-verification.json').write_text(json.dumps(result,indent=2))
unreal.EditorPythonScripting.set_keep_python_script_alive(True);started=time.monotonic()
def tick(delta):
    if time.monotonic()-started<15:return
    unreal.unregister_slate_post_tick_callback(handle)
    try:verify()
    finally:unreal.EditorPythonScripting.set_keep_python_script_alive(False)
handle=unreal.register_slate_post_tick_callback(tick)
