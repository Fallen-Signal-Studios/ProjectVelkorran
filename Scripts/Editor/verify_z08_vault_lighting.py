"""Fresh inward-vault-lighting and preceding architecture checks."""
from pathlib import Path
import runpy,time
import unreal
root=Path(unreal.Paths.project_dir());DEFER_Z08_VAULT_AUTORUN=True
exec(compile((root/'Scripts/Editor/verify_z08_vault.py').read_text(),'verify_z08_vault','exec'),globals())
verify_z08_vault_stage=verify
del DEFER_Z08_VAULT_AUTORUN
def verify():
    verify_z08_vault_stage()
    geometry=runpy.run_path(str(root/'Scripts/Editor/check_z08_vault_lighting.py'))['check_z08_vault_lighting'](actors)
    assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    (out/'z08-vault-lighting-verification.json').write_text(json.dumps(dict(status='passed',actor_count=len(actors),geometry=geometry),indent=2))
if not globals().get('DEFER_Z08_VAULT_LIGHTING_AUTORUN',False):
    unreal.EditorPythonScripting.set_keep_python_script_alive(True);started=time.monotonic()
    def tick(delta):
        if time.monotonic()-started<15:return
        unreal.unregister_slate_post_tick_callback(handle)
        try:verify()
        finally:unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    handle=unreal.register_slate_post_tick_callback(tick)
