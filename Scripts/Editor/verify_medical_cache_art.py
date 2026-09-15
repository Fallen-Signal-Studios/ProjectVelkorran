"""Fresh native cache presentation check and complete preceding architecture chain."""
from pathlib import Path
import json,runpy,time,unreal
root=Path(unreal.Paths.project_dir());DEFER_REFUGE_SHELL_AUTORUN=True
exec(compile((root/'Scripts/Editor/verify_refuge_shell.py').read_text(),'verify_refuge_shell','exec'),globals())
verify_refuge_shell_stage=verify
del DEFER_REFUGE_SHELL_AUTORUN
def verify():
    verify_refuge_shell_stage()
    result=runpy.run_path(str(root/'Scripts/Editor/check_medical_cache_art.py'))['check_medical_cache_art'](actors)
    assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    (out/'medical-cache-art-verification.json').write_text(json.dumps(dict(status='passed',actor_count=len(actors),settings=result),indent=2))
if not globals().get('DEFER_MEDICAL_CACHE_AUTORUN',False):
    unreal.EditorPythonScripting.set_keep_python_script_alive(True);started=time.monotonic()
    def tick(delta):
        if time.monotonic()-started<15:return
        unreal.unregister_slate_post_tick_callback(handle)
        try:verify()
        finally:unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    handle=unreal.register_slate_post_tick_callback(tick)
