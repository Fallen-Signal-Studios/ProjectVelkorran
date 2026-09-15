"""Fresh roof/gate presentation and the full preceding architecture chain."""
from pathlib import Path
import json,runpy,time,unreal
root=Path(unreal.Paths.project_dir());DEFER_CACHE_ENCLOSURE_AUTORUN=True
exec(compile((root/'Scripts/Editor/verify_cache_enclosure.py').read_text(),'verify_cache_enclosure','exec'),globals())
verify_cache_enclosure_stage=verify
del DEFER_CACHE_ENCLOSURE_AUTORUN
def verify():
    verify_cache_enclosure_stage()
    result=runpy.run_path(str(root/'Scripts/Editor/check_support_closures.py'))['check_support_closures'](actors)
    assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    (out/'support-closure-verification.json').write_text(json.dumps(dict(status='passed',actor_count=len(actors),settings=result),indent=2))
if not globals().get('DEFER_SUPPORT_CLOSURES_AUTORUN',False):
    unreal.EditorPythonScripting.set_keep_python_script_alive(True);started=time.monotonic()
    def tick(delta):
        if time.monotonic()-started<15:return
        unreal.unregister_slate_post_tick_callback(handle)
        try:verify()
        finally:unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    handle=unreal.register_slate_post_tick_callback(tick)
