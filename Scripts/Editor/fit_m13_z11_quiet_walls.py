"""Preview or save the custom Z11 side walls over untouched native collision.

The authored bay occupies only the 4.5 x .5 x 6 m south/north wall spans.
The six-metre central opening, west observation window and east wall remain.
"""
from pathlib import Path
import hashlib
import json
import os
import runpy
import shutil

import unreal

root = Path(unreal.Paths.project_dir())
source = root/'Art/Source/Aurelion/Z11QuietWall'
out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
baseline = json.loads((source/'baseline.json').read_text())
spec = json.loads((source/'manifest.json').read_text())['modules'][0]
save = os.environ.get('SOV_Z11_QUIET_WALL_SAVE') == '1'
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not level.is_in_play_in_editor()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_name() == 'L_Aurelion_M13'
maps = {name: root/'Content/Aurelion/Maps'/name
        for name in ('L_Aurelion_M12.umap','L_Aurelion_M13.umap')}
digest = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
map_before = {name:digest(path) for name,path in maps.items()}
assert map_before['L_Aurelion_M13.umap'] == baseline['map_hashes']['L_Aurelion_M13.umap']
fbx = source/(spec['asset']+'.fbx')
fbx_sha = digest(fbx)
review = None
if save:
    review = json.loads(Path(os.environ['SOV_Z11_QUIET_WALL_REVIEW']).read_text())
    assert review['status'] == 'unsaved_preview'
    assert review['map_hashes_before'] == map_before and review['fbx_sha256'] == fbx_sha

actorsub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors = list(actorsub.get_all_level_actors())
by_label = {actor.get_actor_label():actor for actor in actors}
owner = by_label[baseline['owner']]
assert not any(label.startswith('Aurelion_Custom_Z11_QuietWall_') for label in by_label)
parts = [c for c in owner.get_components_by_class(unreal.InstancedStaticMeshComponent)
         if c.static_mesh and c.static_mesh.get_path_name() == baseline['mesh']]
assert len(parts) == 1
part = parts[0]
assert part.get_instance_count() == 146
assert part.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION
original = [part.get_instance_transform(i,world_space=True) for i in range(146)]
assert [t.export_text() for t in original] == [row['transform'] for row in baseline['instances']]
selected = [i for i,t in enumerate(original) if abs(t.scale3d.x-1.125)<.00001]
assert len(selected) == 32
assert {round(original[i].translation.x) for i in selected} == {-975,-525,525,975}
assert {round(original[i].translation.y) for i in selected} == {42275,42300,43875,43900}
assert {round(original[i].translation.z) for i in selected} == {150,450}
retained = [original[i] for i in range(146) if i not in selected]
assert len(retained) == 114
helpers = runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'))
other_before = helpers['snapshot_actor_state']([a for a in actors if a != owner])
owner_pose = owner.get_actor_transform().export_text()
owner_collision = owner.get_actor_enable_collision()
native_before = {}
for label,row in baseline['native'].items():
    a = by_label[label]
    c = a.get_component_by_class(unreal.StaticMeshComponent)
    native_before[label] = (a.get_actor_transform().export_text(),
                            a.get_actor_enable_collision(),
                            str(c.get_collision_enabled()),
                            c.get_editor_property('visible'))
    assert native_before[label][0] == row['transform']
    assert native_before[label][2] == row['collision']
    assert not native_before[label][3]

dest = '/Game/Aurelion/Environment/ArchitectureKit'
asset = dest+'/Meshes/'+spec['asset']
asset_file = root/'Content/Aurelion/Environment/ArchitectureKit/Meshes'/(spec['asset']+'.uasset')
materials = {
    'M_Aurelion_IvoryStone':dest+'/Materials/M_AurelionKit_Ivory',
    'M_Aurelion_BlackStone':dest+'/Materials/M_AurelionKit_ObservationBlackStone',
    'M_Aurelion_AncientGold':dest+'/Materials/M_AurelionKit_Gold',
    'M_Aurelion_ChannelShadow':dest+'/Materials/M_AurelionKit_Reveal',
}
mesh = unreal.load_asset(asset) if save else helpers['import_owned_mesh'](spec,source,dest+'/Meshes',materials)
assert mesh
mesh_sha = digest(asset_file)
if save: assert mesh_sha == review['mesh_sha256']

owner.modify()
part.modify()
part.clear_instances()
for transform in retained:
    part.add_instance(transform,world_space=True)
assert part.get_instance_count() == 114
assert [part.get_instance_transform(i,world_space=True).export_text() for i in range(114)] == [t.export_text() for t in retained]
assert part.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION

spawned = {}
for y in (42300,43900):
    for x in (-975,-525,525,975):
        label = 'Aurelion_Custom_Z11_QuietWall_'+str(y)+'_'+str(x)
        actor = actorsub.spawn_actor_from_class(unreal.StaticMeshActor,
                                                unreal.Vector(x,y,0),unreal.Rotator())
        assert actor
        actor.set_actor_label(label)
        c = actor.get_component_by_class(unreal.StaticMeshComponent)
        c.set_static_mesh(mesh)
        c.set_collision_profile_name('NoCollision')
        c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        c.set_editor_property('can_ever_affect_navigation',False)
        actor.set_actor_enable_collision(False)
        origin,extent = actor.get_actor_bounds(False)
        assert abs(origin.x-x)<1 and abs(origin.y-y)<1
        assert extent.x<=225.1 and extent.y<=25.1
        assert origin.z-extent.z>=-.2 and origin.z+extent.z<=600.2
        # Every bay remains in its original wall span, outside the central
        # six-metre opening and wholly clear of the west observation wall.
        assert abs(x)-extent.x>=299.8 and abs(x)+extent.x<=1200.1
        spawned[label] = dict(transform=actor.get_actor_transform().export_text(),
                              mesh=c.static_mesh.get_path_name(),
                              collision=str(c.get_collision_enabled()),
                              nav=c.get_editor_property('can_ever_affect_navigation'))

assert helpers['snapshot_actor_state']([a for a in actors if a != owner]) == other_before
assert owner.get_actor_transform().export_text() == owner_pose
assert owner.get_actor_enable_collision() == owner_collision
for label,expected in native_before.items():
    a = by_label[label]
    c = a.get_component_by_class(unreal.StaticMeshComponent)
    assert (a.get_actor_transform().export_text(),a.get_actor_enable_collision(),
            str(c.get_collision_enabled()),c.get_editor_property('visible')) == expected

report = dict(status='unsaved_preview',map_hashes_before=map_before,
              fbx_sha256=fbx_sha,mesh_sha256=mesh_sha,old_wall_instances=32,
              retained_east_wall_instances=114,new_custom_wall_bays=8,
              native_collision_preserved=True,observation_window_preserved=True,
              central_opening_preserved=True,other_actors_preserved=True,
              spawned=spawned)
if save:
    assert spawned == review['spawned']
    shutil.copy2(maps['L_Aurelion_M13.umap'],out/'L_Aurelion_M13.before.umap')
    assert level.save_current_level()
    assert level.load_level('/Game/Aurelion/Maps/L_Aurelion_M13')
    restored = {a.get_actor_label():a for a in actorsub.get_all_level_actors()}
    assert all(label in restored for label in spawned)
    restored_part = next(c for c in restored[baseline['owner']].get_components_by_class(unreal.InstancedStaticMeshComponent)
                         if c.static_mesh and c.static_mesh.get_path_name() == baseline['mesh'])
    assert restored_part.get_instance_count() == 114
    assert [restored_part.get_instance_transform(i,world_space=True).export_text() for i in range(114)] == [t.export_text() for t in retained]
    assert restored_part.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION
    for label,row in spawned.items():
        a = restored[label]
        c = a.get_component_by_class(unreal.StaticMeshComponent)
        assert a.get_actor_transform().export_text() == row['transform']
        assert c.static_mesh.get_path_name() == row['mesh']
        assert str(c.get_collision_enabled()) == row['collision']
        assert not a.get_actor_enable_collision() and not c.get_editor_property('can_ever_affect_navigation')
    for label,expected in native_before.items():
        a=restored[label];c=a.get_component_by_class(unreal.StaticMeshComponent)
        assert (a.get_actor_transform().export_text(),a.get_actor_enable_collision(),
                str(c.get_collision_enabled()),c.get_editor_property('visible')) == expected
    assert digest(maps['L_Aurelion_M12.umap']) == map_before['L_Aurelion_M12.umap']
    report.update(status='saved_reloaded',map_hash_after=digest(maps['L_Aurelion_M13.umap']))
(out/'z11-quiet-wall-fit.json').write_text(json.dumps(report,indent=2))
runpy.run_path(str(root/'Scripts/Editor/preview_m13_route.py'),init_globals={
    'ALLOW_DIRTY_PREVIEW':not save,
    'M13_ROUTE_VIEWS':[('z11-wall-center',(750,43100,190)),
                       ('z11-wall-oblique',(600,42650,190))],
    'M13_ROUTE_YAWS':{'z11-wall-center':180,'z11-wall-oblique':170},
    'M13_ROUTE_PITCHES':{'z11-wall-center':-8,'z11-wall-oblique':-6},
})
print('M13_Z11_QUIET_WALL_'+('SAVED' if save else 'UNSAVED_PREVIEW')+'_PASS')
