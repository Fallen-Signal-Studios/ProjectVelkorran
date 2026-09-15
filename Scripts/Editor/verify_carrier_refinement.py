"""Fresh carrier refinement, local fill and preceding architecture checks."""
from pathlib import Path
import json,runpy,time,unreal
root=Path(unreal.Paths.project_dir())
DEFER_CARRIER_KIT_AUTORUN=True
exec(compile((root/'Scripts/Editor/verify_carrier_kit.py').read_text(),'verify_carrier_kit','exec'),globals())
verify_carrier_kit_stage=verify
del DEFER_CARRIER_KIT_AUTORUN
def verify():
    verify_carrier_kit_stage()
    fit=json.loads((root/'Art/Source/Aurelion/CarrierKit/lighting-fit.json').read_text())
    result=runpy.run_path(str(root/'Scripts/Editor/fit_carrier_fill.py'))['check_carrier_fill'](actors,fit)
    assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    (out/'carrier-refinement-verification.json').write_text(json.dumps(dict(status='passed',actor_count=len(actors),lighting=result),indent=2))
if not globals().get('DEFER_CARRIER_REFINEMENT_AUTORUN',False):
    unreal.EditorPythonScripting.set_keep_python_script_alive(True);started=time.monotonic()
    def tick(delta):
        if time.monotonic()-started<15:return
        unreal.unregister_slate_post_tick_callback(handle)
        try:verify()
        finally:unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    handle=unreal.register_slate_post_tick_callback(tick)
