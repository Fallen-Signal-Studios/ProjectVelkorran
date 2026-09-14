"""Fresh request-visual and complete saved architecture regression."""
from pathlib import Path
import runpy,time
import unreal
root=Path(unreal.Paths.project_dir());DEFER_Z06_GATE_ASSEMBLY_AUTORUN=True
exec(compile((root/'Scripts/Editor/verify_z06_gate_assembly.py').read_text(),'verify_z06_gate_assembly','exec'),globals())
verify_gate_assembly_stage=verify
del DEFER_Z06_GATE_ASSEMBLY_AUTORUN
def verify():
    verify_gate_assembly_stage()
    geometry=runpy.run_path(str(root/'Scripts/Editor/check_z06_request_props.py'))['check_z06_request_props'](world,actors)
    assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    (out/'z06-request-props-verification.json').write_text(json.dumps(dict(status='passed',actor_count=len(actors),geometry=geometry),indent=2))
if not globals().get("DEFER_Z06_REQUEST_PROPS_AUTORUN",False):
    unreal.EditorPythonScripting.set_keep_python_script_alive(True);started=time.monotonic()
    def tick(delta):
        if time.monotonic()-started<15:return
        unreal.unregister_slate_post_tick_callback(handle)
        try:verify()
        finally:unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    handle=unreal.register_slate_post_tick_callback(tick)
