"""Settled fresh-load architecture regression including the split-vault canopies."""
from pathlib import Path
import runpy,time
import unreal
root=Path(unreal.Paths.project_dir());DEFER_ARCHITECTURE_AUTORUN=True
canopy_defer=bool(globals().get('DEFER_CANOPY_AUTORUN',False))
exec(compile((root/'Scripts/Editor/verify_atrium_parapets.py').read_text(),'verify_atrium_parapets','exec'),globals())
verify_previous=verify
del DEFER_ARCHITECTURE_AUTORUN
def verify():
    verify_previous()
    assert len(actors)==2480+atrium_bridge_floor_count+atrium_crown_count+atrium_crown_light_count+z06_paving_count+z06_ceiling_count+z06_side_count+z06_light_count+z06_end_count+z06_slab_count+z06_refuge_count+z06_refuge_finish_count and atrium_canopy_count==8
    geometry=runpy.run_path(str(root/'Scripts/Editor/check_atrium_canopies.py'))['check_canopies'](world,actors)
    assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    (out/'atrium-canopy-verification.json').write_text(json.dumps(dict(status='passed',actor_count=len(actors),geometry=geometry),indent=2))
unreal.EditorPythonScripting.set_keep_python_script_alive(True);started=time.monotonic()
def tick(delta):
    if time.monotonic()-started<15:return
    unreal.unregister_slate_post_tick_callback(handle)
    try:verify()
    finally:unreal.EditorPythonScripting.set_keep_python_script_alive(False)
if not canopy_defer:handle=unreal.register_slate_post_tick_callback(tick)
