"""Fresh corrected enclosure fit and all preceding architecture gates."""
from pathlib import Path
import json,runpy,time,unreal
root=Path(unreal.Paths.project_dir());DEFER_MEDICAL_CACHE_AUTORUN=True
exec(compile((root/'Scripts/Editor/verify_medical_cache_art.py').read_text(),'verify_medical_cache','exec'),globals())
verify_medical_cache_stage=verify
del DEFER_MEDICAL_CACHE_AUTORUN
def verify():
    verify_medical_cache_stage()
    result=runpy.run_path(str(root/'Scripts/Editor/check_cache_enclosure.py'))['check_cache_enclosure'](actors)
    assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    (out/'cache-enclosure-verification.json').write_text(json.dumps(dict(status='passed',actor_count=len(actors),settings=result),indent=2))
if not globals().get('DEFER_CACHE_ENCLOSURE_AUTORUN',False):
    unreal.EditorPythonScripting.set_keep_python_script_alive(True);started=time.monotonic()
    def tick(delta):
        if time.monotonic()-started<15:return
        unreal.unregister_slate_post_tick_callback(handle)
        try:verify()
        finally:unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    handle=unreal.register_slate_post_tick_callback(tick)
