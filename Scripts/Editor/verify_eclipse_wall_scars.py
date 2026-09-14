"""Fresh-load overlays and complete preceding architecture verification chain."""
from pathlib import Path
import json,runpy,time,unreal
import os
_audit=[]
for _a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
    if _a.get_actor_label() in ('Z08__EclipseConduit_05','Z08__EclipseScar_05','Z08__EclipseConduit_06','Z08__EclipseScar_06'):
        _c=_a.get_component_by_class(unreal.StaticMeshComponent)
        _audit.append(dict(actor=_a.get_actor_label(),component=_c.get_path_name(),transform=_a.get_actor_transform().export_text(),mesh=_c.static_mesh.get_path_name(),actor_collision=_a.get_actor_enable_collision(),collision=str(_c.get_collision_enabled()),profile=str(_c.get_collision_profile_name()),materials=[_c.get_material(i).get_path_name() for i in range(_c.get_num_materials())]))
(Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])/'current-scar-state.json').write_text(json.dumps(_audit,indent=2))
root=Path(unreal.Paths.project_dir());DEFER_Z08_LABELS_AUTORUN=True
exec(compile((root/'Scripts/Editor/verify_z08_technical_labels.py').read_text(),'verify_z08_labels','exec'),globals())
verify_z08_labels_stage=verify
del DEFER_Z08_LABELS_AUTORUN
def verify():
    verify_z08_labels_stage()
    result=runpy.run_path(str(root/'Scripts/Editor/check_eclipse_wall_scars.py'))['check_eclipse_wall_scars'](actors)
    assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    (out/'eclipse-wall-scars-verification.json').write_text(json.dumps(dict(status='passed',actor_count=len(actors),settings=result),indent=2))
if not globals().get('DEFER_ECLIPSE_SCARS_AUTORUN',False):
    unreal.EditorPythonScripting.set_keep_python_script_alive(True);started=time.monotonic()
    def tick(delta):
        if time.monotonic()-started<15:return
        unreal.unregister_slate_post_tick_callback(handle)
        try:verify()
        finally:unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    handle=unreal.register_slate_post_tick_callback(tick)
