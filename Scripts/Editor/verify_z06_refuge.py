"""Fresh reload of the architecture chain and fitted refuge surfaces."""
from pathlib import Path
import runpy,time
import unreal
root=Path(unreal.Paths.project_dir());DEFER_Z06_SLAB_AUTORUN=True
exec(compile((root/'Scripts/Editor/verify_z06_slab.py').read_text(),'verify_z06_slab','exec'),globals())
verify_z06_slab_stage=verify
del DEFER_Z06_SLAB_AUTORUN
def verify():
    verify_z06_slab_stage()
    assert len(actors)==2834+z06_refuge_finish_count+z06_gate_assembly_count+z06_flank_landing_count+z07_paving_count and z06_refuge_count==2
    geometry=runpy.run_path(str(root/'Scripts/Editor/check_z06_refuge.py'))['check_z06_refuge'](world,actors)
    assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    (out/'z06-refuge-verification.json').write_text(json.dumps(dict(status='passed',actor_count=len(actors),geometry=geometry),indent=2))
if not globals().get('DEFER_Z06_REFUGE_AUTORUN',False):
    unreal.EditorPythonScripting.set_keep_python_script_alive(True);started=time.monotonic()
    def tick(delta):
        if time.monotonic()-started<15:return
        unreal.unregister_slate_post_tick_callback(handle)
        try:verify()
        finally:unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    handle=unreal.register_slate_post_tick_callback(tick)
