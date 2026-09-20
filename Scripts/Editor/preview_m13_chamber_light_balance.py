"""Aim existing wall washes higher and restrain the conversation-stage highlights."""
from pathlib import Path
import hashlib,json,os,runpy,shutil,unreal
root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not level.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_name()=='L_Aurelion_M13'
sub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);actors=list(sub.get_all_level_actors())
selected=[a for a in actors if a.get_actor_label().startswith(('ENVL_M13_ChamberUplight_','ENVL_M13_TerminalConversation_'))]
assert len(selected)==11
helper=runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'))
before=helper['snapshot_actor_state']([a for a in actors if a not in selected])
def read(a):
    c=a.get_component_by_class(unreal.LightComponent)
    props={k:c.get_editor_property(k) for k in ('intensity','cast_shadows')}
    if isinstance(c,unreal.SpotLightComponent):
        props.update({k:c.get_editor_property(k) for k in ('outer_cone_angle','inner_cone_angle','source_radius','attenuation_radius')})
    return dict(pose=a.get_actor_transform().export_text(),properties=props)
old={a.get_actor_label():read(a) for a in selected}
lighting=runpy.run_path(str(root/'Scripts/Editor/refine_m13_chamber_lighting.py'))
assert set(lighting['apply']())=={a.get_actor_label() for a in selected if isinstance(a,unreal.SpotLight)}
for a in selected:
    c=a.get_component_by_class(unreal.LightComponent);a.modify();c.modify()
    if not isinstance(c,unreal.SpotLightComponent):c.set_intensity(2100)
expected={a.get_actor_label():read(a) for a in selected}
assert helper['snapshot_actor_state']([a for a in actors if a not in selected])==before
persist=bool(globals().get('SAVE_LIGHT_BALANCE',False))
report=dict(status='unsaved_preview',before=old,after=expected,other_actors_preserved=True)
if persist:
    m12=root/'Content/Aurelion/Maps/L_Aurelion_M12.umap';protected=hashlib.sha256(m12.read_bytes()).hexdigest()
    shutil.copy2(root/'Content/Aurelion/Maps/L_Aurelion_M13.umap',out/'L_Aurelion_M13.before.umap')
    assert level.save_current_level() and level.load_level('/Game/Aurelion/Maps/L_Aurelion_M13')
    current=list(sub.get_all_level_actors());lights=[a for a in current if a.get_actor_label() in expected]
    assert {a.get_actor_label():read(a) for a in lights}==expected
    assert helper['snapshot_actor_state']([a for a in current if a not in lights])==before
    assert hashlib.sha256(m12.read_bytes()).hexdigest()==protected
    report.update(status='saved_reloaded',m12_unchanged=True)
(out/'light-balance.json').write_text(json.dumps(report,indent=2))
runpy.run_path(str(root/'Scripts/Editor/preview_m13_route.py'),init_globals=dict(ALLOW_DIRTY_PREVIEW=not persist,
    M13_ROUTE_VIEWS=[('chamber',(0,33600,-1570)),('stage',(600,34100,-1570)),('ceiling',(0,35000,-1570))],
    M13_ROUTE_PITCHES={'ceiling':20}))
