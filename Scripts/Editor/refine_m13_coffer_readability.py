"""Refresh the owned wall module; preserve placements, collision, and both maps."""
from pathlib import Path
import hashlib,json,os,runpy,shutil,unreal
root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not level.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_name()=='L_Aurelion_M13'
maps={name:root/('Content/Aurelion/Maps/'+name+'.umap') for name in ('L_Aurelion_M12','L_Aurelion_M13')}
digest=lambda path:hashlib.sha256(path.read_bytes()).hexdigest()
hashes={name:digest(path) for name,path in maps.items()}
actors=list(unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors())
owner=next(a for a in actors if a.get_actor_label()=='Aurelion_Art_Z10_Chamber_walls')
c=owner.get_component_by_class(unreal.InstancedStaticMeshComponent)
assert c.get_instance_count()==639 and c.static_mesh.get_name()=='SM_Aurelion_KIT_Z10ChamberCoffer'
poses=[c.get_instance_transform(i,world_space=True).export_text() for i in range(639)]
helper=runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'))
before=helper['snapshot_actor_state'](actors);bounds=c.static_mesh.get_bounds()
source=root/'Art/Source/Aurelion/Z10ChamberWall';spec=json.loads((source/'manifest.json').read_text())['modules'][0]
dest='/Game/Aurelion/Environment/ArchitectureKit'
materials={k:dest+'/Materials/M_AurelionKit_'+v for k,v in {
    'M_Aurelion_IvoryStone':'PavingIvory','M_Aurelion_AncientGold':'Gold','M_Aurelion_ChannelShadow':'Reveal'}.items()}
if not globals().get('READBACK_ONLY',False):
    shutil.copy2(root/'Content/Aurelion/Environment/ArchitectureKit/Meshes/SM_Aurelion_KIT_Z10ChamberCoffer.uasset',out/'Coffer.before.uasset')
    mesh=helper['import_owned_mesh'](spec,source,dest+'/Meshes',materials)
else:mesh=c.static_mesh
assert c.static_mesh==mesh and c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
sm=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
nanite=sm.get_nanite_settings(mesh)
assert nanite.enabled and nanite.position_precision==10 and nanite.fallback_percent_triangles==1
assert sm.get_num_uv_channels(mesh,0)==2 and sm.get_simple_collision_count(mesh)==0 and sm.get_convex_collision_count(mesh)==0
after=mesh.get_bounds()
for axis in ('x','y','z'):
    assert abs(getattr(after.origin,axis)-getattr(bounds.origin,axis))<.1
    assert abs(getattr(after.box_extent,axis)-getattr(bounds.box_extent,axis))<.1
assert poses==[c.get_instance_transform(i,world_space=True).export_text() for i in range(639)]
assert helper['snapshot_actor_state'](actors)==before
assert all(digest(path)==hashes[name] for name,path in maps.items())
(out/'coffer-readability.json').write_text(json.dumps(dict(status='fresh_readback' if globals().get('READBACK_ONLY',False) else 'reimported',
    instances=639,source_triangles=spec['triangles'],map_hashes=hashes,actor_states_preserved=True,
    asset_sha256=digest(root/'Content/Aurelion/Environment/ArchitectureKit/Meshes/SM_Aurelion_KIT_Z10ChamberCoffer.uasset'),
    instance_transforms_preserved=True,native_collision_preserved=True),indent=2))
runpy.run_path(str(root/'Scripts/Editor/preview_m13_route.py'),init_globals=dict(ALLOW_DIRTY_PREVIEW=True,
    M13_ROUTE_VIEWS=[('chamber',(0,33600,-1570)),('wall-detail',(3400,37000,-1300))],M13_ROUTE_YAWS={'wall-detail':30}))
