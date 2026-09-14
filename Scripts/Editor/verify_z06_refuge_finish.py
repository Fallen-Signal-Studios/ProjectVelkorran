"""Fresh reload of refuge support, guards and the complete architecture chain."""
from pathlib import Path
import runpy,time
import unreal
root=Path(unreal.Paths.project_dir());DEFER_Z06_REFUGE_AUTORUN=True
exec(compile((root/'Scripts/Editor/verify_z06_refuge.py').read_text(),'verify_z06_refuge','exec'),globals())
verify_z06_refuge_surface_stage=verify
del DEFER_Z06_REFUGE_AUTORUN
def verify():
    verify_z06_refuge_surface_stage()
    assert len(actors)==2837+z06_gate_assembly_count+z06_flank_landing_count+z07_paving_count+z07_wall_count+z07_light_count+z08_paving_count and z06_refuge_finish_count==3
    geometry=runpy.run_path(str(root/'Scripts/Editor/check_z06_refuge_finish.py'))['check_z06_refuge_finish'](world,actors)
    assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    (out/'z06-refuge-finish-verification.json').write_text(json.dumps(dict(status='passed',actor_count=len(actors),geometry=geometry),indent=2))
if not globals().get('DEFER_Z06_REFUGE_FINISH_AUTORUN',False):
    unreal.EditorPythonScripting.set_keep_python_script_alive(True);started=time.monotonic()
    def tick(delta):
        if time.monotonic()-started<15:return
        unreal.unregister_slate_post_tick_callback(handle)
        try:verify()
        finally:unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    handle=unreal.register_slate_post_tick_callback(tick)
