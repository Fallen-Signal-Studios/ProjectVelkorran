"""Fresh cargo verification and preceding architecture chain."""
from pathlib import Path
import json,runpy,time,unreal
root=Path(unreal.Paths.project_dir());DEFER_Z08_DECK_LIGHTING_AUTORUN=True
exec(compile((root/'Scripts/Editor/verify_z08_deck_lighting.py').read_text(),'verify_deck_lighting','exec'),globals())
verify_z08_deck_lighting_stage=verify
del DEFER_Z08_DECK_LIGHTING_AUTORUN
def verify():
    verify_z08_deck_lighting_stage()
    result=runpy.run_path(str(root/'Scripts/Editor/check_z08_cargo.py'))['check_z08_cargo'](actors)
    assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    (out/'z08-cargo-verification.json').write_text(json.dumps(dict(status='passed',actor_count=len(actors),settings=result),indent=2))
if not globals().get('DEFER_Z08_CARGO_AUTORUN',False):
    unreal.EditorPythonScripting.set_keep_python_script_alive(True);started=time.monotonic()
    def tick(delta):
        if time.monotonic()-started<15:return
        unreal.unregister_slate_post_tick_callback(handle)
        try:verify()
        finally:unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    handle=unreal.register_slate_post_tick_callback(tick)
