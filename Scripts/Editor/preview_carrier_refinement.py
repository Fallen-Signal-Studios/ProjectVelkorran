"""Refine saved carrier art and add a measured local exterior fill."""
from pathlib import Path
import json,os,runpy,shutil,unreal
root=Path(unreal.Paths.project_dir());source=root/'Art/Source/Aurelion/CarrierKit';out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
actors=list(unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors())
labels={a.get_actor_label():a for a in actors};assert len(actors)==3140
helper=runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'))
before=helper['snapshot_actor_state'](actors)
old_lights=[c for a in actors for c in a.get_components_by_class(unreal.LightComponent)]
def light_state(c):
    return (c.get_world_transform().export_text(),c.get_editor_property('intensity'),c.get_editor_property('light_color').export_text(),c.get_editor_property('visible'),c.get_editor_property('cast_shadows'))
light_before=[light_state(c) for c in old_lights]
manifest=json.loads((source/'manifest.json').read_text());persist=bool(globals().get('PERSIST_CARRIER_REFINEMENT',False))
dest='/Game/Aurelion/Environment/ArchitectureKit'
if not persist and globals().get('REIMPORT_CARRIER_HULL',True):
    spec=next(r for r in manifest['modules'] if r['asset'].endswith('Hull'))
    meshfile=root/'Content/Aurelion/Environment/ArchitectureKit/Meshes'/ (spec['asset']+'.uasset')
    shutil.copy2(meshfile,out/(spec['asset']+'-before.uasset'))
    materials={k:dest+'/Materials/M_AurelionKit_'+v for k,v in {'M_Aurelion_IvoryStone':'PavingIvory','M_Aurelion_AncientGold':'Gold','M_Aurelion_ChannelShadow':'Reveal'}.items()}
    helper['import_owned_mesh'](spec,source,dest+'/Meshes',materials)
check=runpy.run_path(str(root/'Scripts/Editor/check_carrier_kit.py'))
carrier_labels={a.get_actor_label() for a in actors if a.get_actor_label().startswith('Aurelion_Carrier_')}
for label in carrier_labels:
    a=labels[label];c=a.get_component_by_class(unreal.InstancedStaticMeshComponent)
    assert c.static_mesh.get_name()==check['mesh_name'](label) and c.get_instance_count()==1
    c.modify();c.clear_instances();c.add_instance(check['art_transform'](label,labels),world_space=True)
fit=json.loads((source/'lighting-fit.json').read_text())
lighting=runpy.run_path(str(root/'Scripts/Editor/fit_carrier_fill.py'))
owner=labels[lighting['OWNER']]
added_billboards=len(fit['lights'])-sum(c.get_owner()==owner for c in old_lights)
lighting['fit_carrier_fill'](actors,fit)
after=helper['snapshot_actor_state'](actors)
expected=dict(before)
pose,collision,primitives=before[owner.get_path_name()]
expected[owner.get_path_name()]=(pose,collision,sorted(primitives+[(owner.get_path_name()+':BillboardComponent',str(unreal.CollisionEnabled.NO_COLLISION))]*added_billboards))
actor_diff={key:dict(before=before[key],after=after.get(key)) for key in before if before[key]!=after.get(key)}
light_diff=[dict(component=c.get_path_name(),before=prior,after=light_state(c)) for c,prior in zip(old_lights,light_before) if light_state(c)!=prior]
(out/'carrier-refinement-state-diff.json').write_text(json.dumps(dict(actors=actor_diff,lights=light_diff,expected_editor_only_billboards=added_billboards),indent=2))
assert after==expected and not light_diff,'Unexpected state change; see carrier-refinement-state-diff.json'
settings=check['check_carrier_kit'](actors);light_settings=lighting['check_carrier_fill'](actors,fit)
if persist:
    shutil.copy2(root/'Content/Aurelion/Maps/L_Aurelion_M12.umap',out/'L_Aurelion_M12-before.umap')
    assert editor.save_current_level()
(out/'carrier-refinement.json').write_text(json.dumps(dict(status='saved' if persist else 'unsaved_preview',geometry=settings,lighting=light_settings,preserved_lights=len(old_lights)),indent=2))
# Reuse phase-isolated cameras and their restoration, not placement mutations.
if not globals().get('SKIP_CARRIER_CAPTURE',False):
    labels=carrier_labels
    capture=(root/'Scripts/Editor/preview_carrier_kit.py').read_text().split("capture=(root/'Scripts/Editor/review_eclipse_wall_scars.py')",1)[1]
    exec(compile("capture=(root/'Scripts/Editor/review_eclipse_wall_scars.py')"+capture,'carrier_refinement_capture','exec'),globals())
