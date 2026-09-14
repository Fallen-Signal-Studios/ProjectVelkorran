"""Settled fresh-load regression including the fitted radial bridge floors."""
from pathlib import Path
import runpy,time
import unreal
root=Path(unreal.Paths.project_dir());DEFER_CANOPY_AUTORUN=True
floor_defer=bool(globals().get('DEFER_BRIDGE_FLOOR_AUTORUN',False))
exec(compile((root/'Scripts/Editor/verify_atrium_canopies.py').read_text(),'verify_atrium_canopies','exec'),globals())
verify_canopies=verify
del DEFER_CANOPY_AUTORUN
def verify():
    verify_canopies()
    assert len(actors)==2484+atrium_crown_count+atrium_crown_light_count+z06_paving_count+z06_ceiling_count+z06_side_count+z06_light_count+z06_end_count+z06_slab_count+z06_refuge_count+z06_refuge_finish_count and atrium_bridge_floor_count==4
    geometry=runpy.run_path(str(root/'Scripts/Editor/check_atrium_bridge_floors.py'))['check_bridge_floors'](world,actors)
    assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    (out/'atrium-bridge-floor-verification.json').write_text(json.dumps(dict(status='passed',actor_count=len(actors),geometry=geometry),indent=2))
unreal.EditorPythonScripting.set_keep_python_script_alive(True);started=time.monotonic()
def tick(delta):
    if time.monotonic()-started<15:return
    unreal.unregister_slate_post_tick_callback(handle)
    try:verify()
    finally:unreal.EditorPythonScripting.set_keep_python_script_alive(False)
if not floor_defer:handle=unreal.register_slate_post_tick_callback(tick)
