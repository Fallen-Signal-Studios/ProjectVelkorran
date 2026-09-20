"""Replace only M13's 639 visual wall panels; preserve native collision and poses."""
from pathlib import Path
import hashlib
import json
import os
import runpy
import shutil
import unreal

root=Path(unreal.Paths.project_dir());source=root/'Art/Source/Aurelion/Z10ChamberWall'
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not level.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_name()=='L_Aurelion_M13'
sub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors=list(sub.get_all_level_actors())
owner=next(a for a in actors if a.get_actor_label()=='Aurelion_Art_Z10_Chamber_walls')
component=owner.get_component_by_class(unreal.InstancedStaticMeshComponent)
assert component and component.get_instance_count()==639
assert component.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
old=component.static_mesh
assert old.get_path_name()=='/Game/SpaceshipInterior/Meshes/SM_Scifi_Wall_03_4m.SM_Scifi_Wall_03_4m'
poses=[component.get_instance_transform(i,world_space=True).export_text() for i in range(639)]
helper=runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'))
before=helper['snapshot_actor_state'](actors)
spec=json.loads((source/'manifest.json').read_text())['modules'][0]
dest='/Game/Aurelion/Environment/ArchitectureKit'
materials={k:dest+'/Materials/M_AurelionKit_'+v for k,v in {
    'M_Aurelion_IvoryStone':'PavingIvory','M_Aurelion_AncientGold':'Gold','M_Aurelion_ChannelShadow':'Reveal'}.items()}
mesh=unreal.load_asset(dest+'/Meshes/'+spec['asset']) if globals().get('SAVE_CHAMBER_COFFERS',False) else helper['import_owned_mesh'](spec,source,dest+'/Meshes',materials)
assert mesh
bounds=mesh.get_bounds();original=old.get_bounds()
for axis in ('x','y','z'):
    assert getattr(bounds.origin,axis)-getattr(bounds.box_extent,axis)>=getattr(original.origin,axis)-getattr(original.box_extent,axis)-.1, (axis,'min',bounds)
    assert getattr(bounds.origin,axis)+getattr(bounds.box_extent,axis)<=getattr(original.origin,axis)+getattr(original.box_extent,axis)+.1, (axis,'max',bounds)
component.modify();component.set_static_mesh(mesh);component.set_editor_property('override_materials',[])
assert [component.get_instance_transform(i,world_space=True).export_text() for i in range(639)]==poses
assert helper['snapshot_actor_state'](actors)==before
report=dict(status='unsaved_preview',instances=639,mesh=mesh.get_path_name(),
    all_actor_transforms_and_collision_preserved=True,instance_transforms_preserved=True,
    old_origin=original.origin.export_text(),new_origin=bounds.origin.export_text(),
    qualification='Visual panel replacement; runtime route and GPU cost remain unqualified.')
if globals().get('SAVE_CHAMBER_COFFERS',False):
    m12=root/'Content/Aurelion/Maps/L_Aurelion_M12.umap'
    protected=hashlib.sha256(m12.read_bytes()).hexdigest()
    shutil.copy2(root/'Content/Aurelion/Maps/L_Aurelion_M13.umap',out/'L_Aurelion_M13.before.umap')
    assert level.save_current_level()
    assert level.load_level('/Game/Aurelion/Maps/L_Aurelion_M13')
    current=list(sub.get_all_level_actors());assert helper['snapshot_actor_state'](current)==before
    restored=next(a for a in current if a.get_actor_label()=='Aurelion_Art_Z10_Chamber_walls').get_component_by_class(unreal.InstancedStaticMeshComponent)
    assert restored.static_mesh==mesh
    assert [restored.get_instance_transform(i,world_space=True).export_text() for i in range(639)]==poses
    assert hashlib.sha256(m12.read_bytes()).hexdigest()==protected
    report.update(status='saved_reloaded',m12_unchanged=True)
(out/'chamber-coffer-fit.json').write_text(json.dumps(report,indent=2))
