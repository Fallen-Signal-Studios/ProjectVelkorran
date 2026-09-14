"""Settled full architecture regression including the open atrium crown."""
from pathlib import Path
import runpy,time
import unreal
root=Path(unreal.Paths.project_dir());DEFER_BRIDGE_FLOOR_AUTORUN=True
crown_defer=bool(globals().get('DEFER_CROWN_AUTORUN',False))
exec(compile((root/'Scripts/Editor/verify_atrium_bridge_floors.py').read_text(),'verify_atrium_bridge_floors','exec'),globals())
verify_floors=verify
del DEFER_BRIDGE_FLOOR_AUTORUN
def verify():
    verify_floors()
    assert len(actors)==2493+atrium_crown_light_count+z06_paving_count+z06_ceiling_count+z06_side_count+z06_light_count and atrium_crown_count==9
    geometry=runpy.run_path(str(root/'Scripts/Editor/check_atrium_crown.py'))['check_crown'](world,actors)
    assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    (out/'atrium-crown-verification.json').write_text(json.dumps(dict(status='passed',actor_count=len(actors),geometry=geometry),indent=2))
unreal.EditorPythonScripting.set_keep_python_script_alive(True);started=time.monotonic()
def tick(delta):
    if time.monotonic()-started<15:return
    unreal.unregister_slate_post_tick_callback(handle)
    try:verify()
    finally:unreal.EditorPythonScripting.set_keep_python_script_alive(False)
if not crown_defer:handle=unreal.register_slate_post_tick_callback(tick)
