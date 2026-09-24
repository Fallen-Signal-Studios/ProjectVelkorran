"""Independent fresh M13 map-load check of the saved Z12 service registers."""
from pathlib import Path
import hashlib
import json
import os
import runpy
import unreal

root=Path(unreal.Paths.project_dir())
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
saved=Path(os.environ.get('SOV_Z12_REGISTER_SAVE_REPORT',
    root/'Saved/Validation/Aurelion/Z12RegisterSave-20260923-223859-67748463/service-register-preview.json'))
report=json.loads(saved.read_text())
assert report['status']=='saved_reloaded'
assert report['original_side_coffers']==32 and report['remaining_side_coffers']==24
assert len(report['replaced_indices'])==8
assert hashlib.sha256(Path(report['map_backup']).read_bytes()).hexdigest()==report['maps_before']['L_Aurelion_M13.umap']
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actor_sub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_name()=='L_Aurelion_M13' and not level.is_in_play_in_editor()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
actors=list(actor_sub.get_all_level_actors())
labels={a.get_actor_label():a for a in actors}
owner=labels['Aurelion_Art_M13_Z12_6_21a580']
coffer=next(c for c in owner.get_components_by_class(unreal.InstancedStaticMeshComponent)
            if c.static_mesh and c.static_mesh.get_name()=='SM_Aurelion_KIT_Z12CofferSide')
assert coffer.get_instance_count()==24
assert coffer.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
assert [coffer.get_instance_transform(i,world_space=True).export_text() for i in range(24)]==report['retained_transforms']
mesh=unreal.load_asset(report['mesh'])
assert mesh and mesh.get_editor_property('asset_import_data')
source=root/'Art/Source/Aurelion/Z12DepartureRegister'/('SM_Aurelion_KIT_Z12ServiceRegister.fbx')
assert Path(mesh.get_editor_property('asset_import_data').get_first_filename()).resolve()==source.resolve()
sm=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
assert sm.get_num_uv_channels(mesh,0)==2 and sm.get_nanite_settings(mesh).enabled
assert sm.get_simple_collision_count(mesh)==sm.get_convex_collision_count(mesh)==0
rows=[]
for i in report['replaced_indices']:
    label='PREVIEW_Aurelion_Z12_ServiceRegister_%02d'%i
    actor=labels[label]
    component=actor.static_mesh_component
    position=actor.get_actor_location()
    yaw=actor.get_actor_rotation().yaw
    assert component.static_mesh.get_path_name()==mesh.get_path_name()
    assert component.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
    assert not component.get_editor_property('can_ever_affect_navigation')
    assert not actor.get_actor_enable_collision()
    assert abs(abs(position.x)-2100)<.1 and min(abs(position.y-46900),abs(position.y-48100))<.1
    assert abs(position.z-150)<.1 and abs(abs(yaw)-90)<.1
    rows.append(dict(label=label,position=[position.x,position.y,position.z],yaw=yaw,
                     collision='NoCollision',navigation=False))
assert sum(a.get_actor_label().startswith('PREVIEW_Aurelion_Z12_ServiceRegister_') for a in actors)==8
for x in (-2100,2100):
    for y in (46900,48100):
        assert len([r for r in rows if abs(r['position'][0]-x)<.1 and abs(r['position'][1]-y)<.1])==2
for label in ('Z12_Wall_EW-1','Z12_Wall_EW1'):
    body=labels[label].static_mesh_component
    assert not body.get_editor_property('visible')
    assert body.get_collision_enabled()==unreal.CollisionEnabled.QUERY_AND_PHYSICS
hashes={name:hashlib.sha256((root/'Content/Aurelion/Maps'/name).read_bytes()).hexdigest()
        for name in ('L_Aurelion_M12.umap','L_Aurelion_M13.umap')}
assert hashes==report['map_hashes_after']
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
(out/'service-register-fresh.json').write_text(json.dumps(dict(
    status='fresh_map_pass',registers=rows,side_coffers=24,
    native_wall_collision_retained=True,source_fbx_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
    map_hashes=hashes,qualification='Fresh editor save/load and fixed-camera visual check; PIE recovery separate.'
),indent=2))
runpy.run_path(str(root/'Scripts/Editor/preview_m13_route.py'),init_globals={
    'M13_ROUTE_VIEWS':[('west-register',(-900,46900,180)),('east-register',(900,48100,180))],
    'M13_ROUTE_YAWS':{'west-register':180,'east-register':0},
    'M13_ROUTE_PITCHES':{'west-register':8,'east-register':8},
})
print('Z12_SERVICE_REGISTER_FRESH_MAP_PASS')
