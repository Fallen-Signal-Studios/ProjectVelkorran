"""Fresh saved capture-gallery paving and the full preceding architecture chain."""
from pathlib import Path
import runpy,time
import unreal
root=Path(unreal.Paths.project_dir());DEFER_Z06_KEY_BALANCE_AUTORUN=True
exec(compile((root/'Scripts/Editor/verify_z06_key_balance.py').read_text(),'verify_z06_key_balance','exec'),globals())
verify_z06_key_balance_stage=verify
del DEFER_Z06_KEY_BALANCE_AUTORUN
def verify():
    verify_z06_key_balance_stage()
    geometry=runpy.run_path(str(root/'Scripts/Editor/check_z07_paving.py'))['check_z07_paving'](world,actors)
    assert len(actors)==2897+z07_wall_count and z07_paving_count==54
    assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    (out/'z07-paving-verification.json').write_text(json.dumps(dict(status='passed',actor_count=len(actors),geometry=geometry),indent=2))
    runpy.run_path(str(root/'Scripts/Editor/audit_z07_architecture.py'),init_globals={'INVENTORY_ONLY':True})
    runpy.run_path(str(root/'Scripts/Editor/audit_z07_light_balance.py'))
if not globals().get('DEFER_Z07_PAVING_AUTORUN',False):
    unreal.EditorPythonScripting.set_keep_python_script_alive(True);started=time.monotonic()
    def tick(delta):
        if time.monotonic()-started<15:return
        unreal.unregister_slate_post_tick_callback(handle)
        try:verify()
        finally:unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    handle=unreal.register_slate_post_tick_callback(tick)
