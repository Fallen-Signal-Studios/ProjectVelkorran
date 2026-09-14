"""Fresh gallery uplight and preceding architecture verification."""
from pathlib import Path
import runpy,time
import unreal
root=Path(unreal.Paths.project_dir());DEFER_Z07_CONSOLES_AUTORUN=True
exec(compile((root/'Scripts/Editor/verify_z07_consoles.py').read_text(),'verify_z07_consoles','exec'),globals())
verify_z07_console_stage=verify
del DEFER_Z07_CONSOLES_AUTORUN
def verify():
    verify_z07_console_stage()
    geometry=runpy.run_path(str(root/'Scripts/Editor/check_z07_lighting.py'))['check_z07_lighting'](world,actors)
    runpy.run_path(str(root/'Scripts/Editor/audit_z08_architecture.py'),init_globals=dict(INVENTORY_ONLY=True))
    assert len(actors)==2935+z08_paving_count+z08_court_count and z07_light_count==12 and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    (out/'z07-lighting-verification.json').write_text(json.dumps(dict(status='passed',actor_count=len(actors),geometry=geometry),indent=2))
if not globals().get('DEFER_Z07_LIGHTING_AUTORUN',False):
    unreal.EditorPythonScripting.set_keep_python_script_alive(True);started=time.monotonic()
    def tick(delta):
        if time.monotonic()-started<15:return
        unreal.unregister_slate_post_tick_callback(handle)
        try:verify()
        finally:unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    handle=unreal.register_slate_post_tick_callback(tick)
