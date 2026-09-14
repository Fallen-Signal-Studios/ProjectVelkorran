"""Fresh reload of the complete architecture chain and Z06 paving."""
from pathlib import Path
import runpy,time
import unreal
root=Path(unreal.Paths.project_dir());DEFER_CROWN_LIGHTING_AUTORUN=True
exec(compile((root/'Scripts/Editor/verify_atrium_crown_lighting.py').read_text(),'verify_atrium_crown_lighting','exec'),globals())
verify_crown_lighting_stage=verify
del DEFER_CROWN_LIGHTING_AUTORUN

def verify():
    verify_crown_lighting_stage()
    assert len(actors)==2635+z06_ceiling_count+z06_side_count+z06_light_count+z06_end_count+z06_slab_count+z06_refuge_count+z06_refuge_finish_count+z06_gate_assembly_count and z06_paving_count==126
    geometry=runpy.run_path(str(root/'Scripts/Editor/check_z06_paving.py'))['check_z06_paving'](world,actors)
    assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    (out/'z06-paving-verification.json').write_text(json.dumps(dict(status='passed',actor_count=len(actors),geometry=geometry),indent=2))

if not globals().get('DEFER_Z06_PAVING_AUTORUN',False):
    unreal.EditorPythonScripting.set_keep_python_script_alive(True);started=time.monotonic()
    def tick(delta):
        if time.monotonic()-started<15:return
        unreal.unregister_slate_post_tick_callback(handle)
        try:verify()
        finally:unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    handle=unreal.register_slate_post_tick_callback(tick)
