"""Fresh instanced cover fit and full preceding architecture regression."""
from pathlib import Path
import runpy,time
import unreal
root=Path(unreal.Paths.project_dir());DEFER_Z06_REQUEST_PROPS_AUTORUN=True
exec(compile((root/'Scripts/Editor/verify_z06_request_props.py').read_text(),'verify_z06_request_props','exec'),globals())
verify_request_props_stage=verify
del DEFER_Z06_REQUEST_PROPS_AUTORUN
def verify():
    verify_request_props_stage()
    geometry=runpy.run_path(str(root/'Scripts/Editor/check_z06_cover_coffers.py'))['check_z06_cover_coffers'](world,actors)
    assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    (out/'z06-cover-coffers-verification.json').write_text(json.dumps(dict(status='passed',actor_count=len(actors),geometry=geometry),indent=2))
unreal.EditorPythonScripting.set_keep_python_script_alive(True);started=time.monotonic()
def tick(delta):
    if time.monotonic()-started<15:return
    unreal.unregister_slate_post_tick_callback(handle)
    try:verify()
    finally:unreal.EditorPythonScripting.set_keep_python_script_alive(False)
handle=unreal.register_slate_post_tick_callback(tick)
