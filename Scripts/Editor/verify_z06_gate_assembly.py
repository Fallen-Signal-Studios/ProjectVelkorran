"""Fresh full-architecture reload with saved gate housing and dedicated lanterns."""
from pathlib import Path
import runpy,time
import unreal
root=Path(unreal.Paths.project_dir());DEFER_KIT_STONE_NORMAL_AUTORUN=True
exec(compile((root/'Scripts/Editor/verify_kit_stone_normal.py').read_text(),'verify_kit_stone_normal','exec'),globals())
verify_stone_material_stage=verify
del DEFER_KIT_STONE_NORMAL_AUTORUN
def verify():
    verify_stone_material_stage()
    assert len(actors)==2842 and z06_gate_assembly_count==5
    geometry=runpy.run_path(str(root/'Scripts/Editor/check_z06_gate_assembly.py'))['check_z06_gate_assembly'](world,actors)
    assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    (out/'z06-gate-assembly-verification.json').write_text(json.dumps(dict(status='passed',actor_count=len(actors),geometry=geometry),indent=2))
unreal.EditorPythonScripting.set_keep_python_script_alive(True);started=time.monotonic()
def tick(delta):
    if time.monotonic()-started<15:return
    unreal.unregister_slate_post_tick_callback(handle)
    try:verify()
    finally:unreal.EditorPythonScripting.set_keep_python_script_alive(False)
handle=unreal.register_slate_post_tick_callback(tick)
