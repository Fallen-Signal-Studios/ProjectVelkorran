"""Fresh saved gate visual and prior architecture, with no actor-count change."""
from pathlib import Path
import runpy,time
import unreal
root=Path(unreal.Paths.project_dir());DEFER_Z06_REFUGE_FINISH_AUTORUN=True
exec(compile((root/'Scripts/Editor/verify_z06_refuge_finish.py').read_text(),'verify_z06_refuge_finish','exec'),globals())
verify_z06_refuge_structure_stage=verify
del DEFER_Z06_REFUGE_FINISH_AUTORUN
def verify():
    verify_z06_refuge_structure_stage()
    geometry=runpy.run_path(str(root/'Scripts/Editor/check_z06_rescue_gate.py'))['check_z06_rescue_gate'](world,actors)
    assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    (out/'z06-rescue-gate-verification.json').write_text(json.dumps(dict(status='passed',actor_count=len(actors),geometry=geometry),indent=2))
if not globals().get('DEFER_Z06_RESCUE_GATE_AUTORUN',False):
    unreal.EditorPythonScripting.set_keep_python_script_alive(True);started=time.monotonic()
    def tick(delta):
        if time.monotonic()-started<15:return
        unreal.unregister_slate_post_tick_callback(handle)
        try:verify()
        finally:unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    handle=unreal.register_slate_post_tick_callback(tick)
