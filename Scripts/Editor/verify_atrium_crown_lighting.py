"""Settled architecture regression including visible crown uplights."""
from pathlib import Path
import runpy,time
import unreal
root=Path(unreal.Paths.project_dir());DEFER_CROWN_AUTORUN=True
exec(compile((root/'Scripts/Editor/verify_atrium_crown.py').read_text(),'verify_atrium_crown','exec'),globals())
verify_crown_geometry=verify
del DEFER_CROWN_AUTORUN
def verify():
    verify_crown_geometry()
    assert len(actors)==2509+z06_paving_count+z06_ceiling_count+z06_side_count+z06_light_count+z06_end_count+z06_slab_count and atrium_crown_light_count==16
    geometry=runpy.run_path(str(root/'Scripts/Editor/check_atrium_crown_lighting.py'))['check_crown_lighting'](world,actors)
    assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    (out/'atrium-crown-lighting-verification.json').write_text(json.dumps(dict(status='passed',actor_count=len(actors),geometry=geometry),indent=2))
if not globals().get('DEFER_CROWN_LIGHTING_AUTORUN',False):
    unreal.EditorPythonScripting.set_keep_python_script_alive(True);started=time.monotonic()
    def tick(delta):
        if time.monotonic()-started<15:return
        unreal.unregister_slate_post_tick_callback(handle)
        try:verify()
        finally:unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    handle=unreal.register_slate_post_tick_callback(tick)
