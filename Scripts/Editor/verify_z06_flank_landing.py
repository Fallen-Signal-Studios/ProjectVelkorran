"""Fresh flank-landing fit and all preceding architecture regressions."""
from pathlib import Path
import runpy,time
import unreal
root=Path(unreal.Paths.project_dir());DEFER_Z06_CLIMB_AUTORUN=True
exec(compile((root/'Scripts/Editor/verify_z06_climb.py').read_text(),'verify_z06_climb','exec'),globals())
verify_climb_panel_stage=verify
del DEFER_Z06_CLIMB_AUTORUN
def verify():
    verify_climb_panel_stage()
    geometry=runpy.run_path(str(root/'Scripts/Editor/check_z06_flank_landing.py'))['check_z06_flank_landing'](world,actors)
    runpy.run_path(str(root/'Scripts/Editor/audit_z06_light_balance.py'))
    assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    (out/'z06-flank-landing-verification.json').write_text(json.dumps(dict(status='passed',actor_count=len(actors),geometry=geometry),indent=2))
unreal.EditorPythonScripting.set_keep_python_script_alive(True);started=time.monotonic()
def tick(delta):
    if time.monotonic()-started<15:return
    unreal.unregister_slate_post_tick_callback(handle)
    try:verify()
    finally:unreal.EditorPythonScripting.set_keep_python_script_alive(False)
handle=unreal.register_slate_post_tick_callback(tick)
