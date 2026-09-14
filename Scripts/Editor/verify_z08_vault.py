"""Fresh saved-vault regression and preceding architecture checks."""
from pathlib import Path
import runpy,time
import unreal
root=Path(unreal.Paths.project_dir());DEFER_BASALT_NORMAL_AUTORUN=True
exec(compile((root/'Scripts/Editor/verify_basalt_fine_normal.py').read_text(),'verify_basalt_fine_normal','exec'),globals())
verify_basalt_normal_stage=verify
del DEFER_BASALT_NORMAL_AUTORUN
def verify():
    verify_basalt_normal_stage()
    geometry=runpy.run_path(str(root/'Scripts/Editor/check_z08_vault.py'))['check_z08_vault'](world,actors)
    assert unreal.load_asset('/Game/Aurelion/Environment/Blender/M_RecessPanel_WarmInlay').get_editor_property('used_with_nanite')
    lighting=runpy.run_path(str(root/'Scripts/Editor/audit_z08_lighting.py'))['audit_z08_lighting'](actors)
    (out/'z08-lighting-inventory.json').write_text(json.dumps(lighting,indent=2))
    assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    (out/'z08-vault-verification.json').write_text(json.dumps(dict(status='passed',actor_count=len(actors),geometry=geometry),indent=2))
if not globals().get('DEFER_Z08_VAULT_AUTORUN',False):
    unreal.EditorPythonScripting.set_keep_python_script_alive(True);started=time.monotonic()
    def tick(delta):
        if time.monotonic()-started<15:return
        unreal.unregister_slate_post_tick_callback(handle)
        try:verify()
        finally:unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    handle=unreal.register_slate_post_tick_callback(tick)
