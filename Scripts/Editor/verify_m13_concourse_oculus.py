"""Independent fresh-editor verification of the two saved Z12 oculus bays."""
from pathlib import Path
import hashlib
import json
import os
import unreal

root=Path(unreal.Paths.project_dir())
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actor_api=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_name()=='L_Aurelion_M13' and not level.is_in_play_in_editor()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
digest=lambda path:hashlib.sha256(path.read_bytes()).hexdigest()
m12=digest(root/'Content/Aurelion/Maps/L_Aurelion_M12.umap')
m13=digest(root/'Content/Aurelion/Maps/L_Aurelion_M13.umap')
assert m12=='64a4517bb88719694793a46ae859f0eb6fbc861eef10e956e0574d8962b844a4'
assert m13=='32d6fcafc66c06e5e53f5f351bbef74893ed833b7d73748c4bbe4aad42df576e'
actors={a.get_actor_label():a for a in actor_api.get_all_level_actors()}
assert not any(label.startswith('PREVIEW_Z12_SovereignOculus_') for label in actors)
source=root/'Art/Source/Aurelion/Z12ConcourseOculus'
spec=json.loads((source/'manifest.json').read_text())['module']
assert json.loads((source/'verification.json').read_text())['status']=='round_trip_pass'
meshpath='/Game/Aurelion/Environment/ArchitectureKit/Meshes/'+spec['asset']
mesh=unreal.load_asset(meshpath)
assert mesh and mesh.get_editor_property('asset_import_data').get_first_filename()
assert Path(mesh.get_editor_property('asset_import_data').get_first_filename()).resolve()==(source/(spec['asset']+'.fbx')).resolve()
sm=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
assert sm.get_num_uv_channels(mesh,0)==2
assert len(mesh.get_editor_property('static_materials'))==len(spec['materials'])
assert sm.get_simple_collision_count(mesh)==sm.get_convex_collision_count(mesh)==0
records=[]
for old_label,new_label,x,y in (
    ('Z12_ConcourseRoof_2_1_B','Aurelion_Z12_Dominion_SovereignOculus',-600,47200),
    ('Z12_ConcourseRoof_4_2_A','Aurelion_Z12_Reformation_SovereignOculus',600,47800)):
    old=actors[old_label]
    old_component=old.static_mesh_component
    assert not old_component.get_editor_property('visible') and old_component.get_editor_property('hidden_in_game')
    assert old_component.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
    new=actors[new_label]
    body=new.static_mesh_component
    assert body.static_mesh.get_path_name().split('.')[0]==meshpath
    assert body.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
    assert not body.get_editor_property('can_ever_affect_navigation')
    assert not new.get_actor_enable_collision()
    p=new.get_actor_location()
    assert abs(p.x-x)<.01 and abs(p.y-y)<.01 and abs(p.z-560)<.01
    center,extent=new.get_actor_bounds(False)
    assert 560<center.z-extent.z<565 and 639<center.z+extent.z<641
    records.append(dict(label=new_label,location=[p.x,p.y,p.z],
                        z_bounds_cm=[center.z-extent.z,center.z+extent.z],
                        old_roof_hidden=True,collision='NoCollision',navigation=False))
for label in ('Z12_Wall_EW-1','Z12_Wall_EW1'):
    assert actors[label].static_mesh_component.get_collision_enabled()==unreal.CollisionEnabled.QUERY_AND_PHYSICS
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
assert digest(root/'Content/Aurelion/Maps/L_Aurelion_M13.umap')==m13
(out/'concourse-oculus-fresh.json').write_text(json.dumps(dict(
    status='fresh_editor_pass',m12_sha256=m12,m13_sha256=m13,
    source_fbx_sha256=digest(source/(spec['asset']+'.fbx')),
    mesh=meshpath,uv_channels=2,material_slots=len(spec['materials']),
    actors=records,native_side_wall_collision_retained=True,
    map_files_unchanged=True),indent=2))
print('Z12_SOVEREIGN_OCULUS_FRESH_EDITOR_PASS')
