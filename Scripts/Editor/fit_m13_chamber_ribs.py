"""Fit the authored support mesh without moving any upright or diagonal instance."""
from pathlib import Path
import hashlib,json,os,runpy,shutil,unreal
root=Path(unreal.Paths.project_dir());source=root/'Art/Source/Aurelion/Z10ChamberRibs'
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not level.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_name()=='L_Aurelion_M13'
sub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);actors=list(sub.get_all_level_actors())
baseline=json.loads((source/'rib-baseline.json').read_text());owner=next(a for a in actors if a.get_actor_label()==baseline['actor'])
c=owner.get_component_by_class(unreal.InstancedStaticMeshComponent);old=c.static_mesh
assert old.get_path_name()==baseline['mesh'] and c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
poses=[c.get_instance_transform(i,world_space=True).export_text() for i in range(c.get_instance_count())]
assert poses==baseline['instances']
helper=runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'));before=helper['snapshot_actor_state'](actors)
spec=json.loads((source/'manifest.json').read_text())['modules'][0];dest='/Game/Aurelion/Environment/ArchitectureKit'
materials={k:dest+'/Materials/M_AurelionKit_'+v for k,v in {'M_Aurelion_IvoryStone':'PavingIvory','M_Aurelion_AncientGold':'Gold','M_Aurelion_ChannelShadow':'Reveal'}.items()}
persist=bool(globals().get('SAVE_CHAMBER_RIBS',False))
mesh=unreal.load_asset(dest+'/Meshes/'+spec['asset']) if persist else helper['import_owned_mesh'](spec,source,dest+'/Meshes',materials)
assert mesh
b=mesh.get_bounds();original=old.get_bounds()
for axis in ('x','y','z'):
    assert getattr(b.origin,axis)-getattr(b.box_extent,axis)>=getattr(original.origin,axis)-getattr(original.box_extent,axis)-.1
    assert getattr(b.origin,axis)+getattr(b.box_extent,axis)<=getattr(original.origin,axis)+getattr(original.box_extent,axis)+.1
c.modify();c.set_static_mesh(mesh);c.set_editor_property('override_materials',[])
assert [c.get_instance_transform(i,world_space=True).export_text() for i in range(c.get_instance_count())]==poses
assert helper['snapshot_actor_state'](actors)==before
report=dict(status='unsaved_preview',instances=len(poses),mesh=mesh.get_path_name(),bounds_fit=True,all_actor_transforms_and_collision_preserved=True)
if persist:
    m12=root/'Content/Aurelion/Maps/L_Aurelion_M12.umap';protected=hashlib.sha256(m12.read_bytes()).hexdigest()
    shutil.copy2(root/'Content/Aurelion/Maps/L_Aurelion_M13.umap',out/'L_Aurelion_M13.before.umap')
    assert level.save_current_level() and level.load_level('/Game/Aurelion/Maps/L_Aurelion_M13')
    current=list(sub.get_all_level_actors());assert helper['snapshot_actor_state'](current)==before
    restored=next(a for a in current if a.get_actor_label()==baseline['actor']).get_component_by_class(unreal.InstancedStaticMeshComponent)
    assert restored.static_mesh==mesh
    assert [restored.get_instance_transform(i,world_space=True).export_text() for i in range(restored.get_instance_count())]==poses
    assert hashlib.sha256(m12.read_bytes()).hexdigest()==protected
    report.update(status='saved_reloaded',m12_unchanged=True)
(out/'chamber-rib-fit.json').write_text(json.dumps(report,indent=2))
runpy.run_path(str(root/'Scripts/Editor/preview_m13_route.py'),init_globals=dict(ALLOW_DIRTY_PREVIEW=not persist,
    M13_ROUTE_VIEWS=[('chamber',(0,33600,-1570)),('rib-detail',(3700,37000,-1100)),('braces',(0,35000,-1570))],
    M13_ROUTE_YAWS={'rib-detail':15},M13_ROUTE_PITCHES={'rib-detail':15,'braces':20}))
