"""Preview or save a four-bay custom Z11 east wall over native collision.

The obsolete decorative bench is visual-only; the authored table and six
chairs, structural rib, west window and all gameplay actors remain untouched.
"""
from pathlib import Path
import hashlib
import json
import os
import runpy
import shutil

import unreal

root=Path(unreal.Paths.project_dir())
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
survey=json.loads((root/'Art/Source/Aurelion/Z11QuietWall/east-baseline.json').read_text())
save=os.environ.get('SOV_Z11_EAST_WALL_SAVE')=='1'
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not level.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_name()=='L_Aurelion_M13'
maps={n:root/'Content/Aurelion/Maps'/n for n in ('L_Aurelion_M12.umap','L_Aurelion_M13.umap')}
digest=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
before={n:digest(p) for n,p in maps.items()}
assert before['L_Aurelion_M13.umap']==survey['map_hashes']['L_Aurelion_M13.umap']
review=None
if save:
    review=json.loads(Path(os.environ['SOV_Z11_EAST_WALL_REVIEW']).read_text())
    assert review['status']=='unsaved_preview' and review['map_hashes_before']==before

actorsub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors=list(actorsub.get_all_level_actors())
by_label={a.get_actor_label():a for a in actors}
owner=by_label[survey['owner']]
part=next(c for c in owner.get_components_by_class(unreal.InstancedStaticMeshComponent)
          if c.static_mesh and c.static_mesh.get_path_name()==survey['vendor_mesh'])
assert part.get_instance_count()==114
original=[part.get_instance_transform(i,world_space=True).export_text() for i in range(114)]
assert original==[row['transform'] for row in survey['vendor_instances']]
assert part.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION

seat=by_label['Aurelion_Radiance_ObservationGallerySeat']
assert seat.get_class()==unreal.StaticMeshActor.static_class()
seat_components=seat.get_components_by_class(unreal.PrimitiveComponent)
assert len(seat_components)==1 and isinstance(seat_components[0],unreal.StaticMeshComponent)
seat_mesh=seat_components[0]
assert seat_mesh.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
assert not seat.get_actor_enable_collision()
assert seat_mesh.get_editor_property('visible') and not seat_mesh.get_editor_property('hidden_in_game')
seat_before=dict(transform=seat.get_actor_transform().export_text(),
                 mesh=seat_mesh.static_mesh.get_path_name(),
                 collision=str(seat_mesh.get_collision_enabled()))
native=by_label['Z11_Wall_EW1']
native_c=native.get_component_by_class(unreal.StaticMeshComponent)
native_before=dict(transform=native.get_actor_transform().export_text(),
                   collision=str(native_c.get_collision_enabled()),
                   enabled=native.get_actor_enable_collision(),
                   visible=native_c.get_editor_property('visible'))
assert native_before['transform']==survey['native_east_wall']
assert native_c.get_collision_enabled()==unreal.CollisionEnabled.QUERY_AND_PHYSICS
assert not native_before['visible']
helpers=runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'))
other_before=helpers['snapshot_actor_state']([a for a in actors if a not in (owner,seat)])
owner_pose=owner.get_actor_transform().export_text()
owner_collision=owner.get_actor_enable_collision()
mesh_path='/Game/Aurelion/Environment/ArchitectureKit/Meshes/SM_Aurelion_KIT_Z11QuietWall_4p5x6'
mesh=unreal.load_asset(mesh_path)
assert mesh
mesh_file=root/'Content/Aurelion/Environment/ArchitectureKit/Meshes/SM_Aurelion_KIT_Z11QuietWall_4p5x6.uasset'
mesh_sha=digest(mesh_file)
if save:assert mesh_sha==review['mesh_sha256']

owner.modify();part.modify();part.clear_instances()
assert part.get_instance_count()==0
seat.modify();seat_mesh.modify()
seat_mesh.set_visibility(False,False)
seat_mesh.set_hidden_in_game(True,False)
assert not seat_mesh.get_editor_property('visible') and seat_mesh.get_editor_property('hidden_in_game')

scale_x=3.99/4.49
spawned={}
for y in (42500,42900,43300,43700):
    label='Aurelion_Custom_Z11_EastWall_'+str(y)
    assert label not in by_label
    a=actorsub.spawn_actor_from_class(unreal.StaticMeshActor,
                                      unreal.Vector(1200,y,0),unreal.Rotator(yaw=90))
    assert a
    a.set_actor_label(label)
    a.set_actor_scale3d(unreal.Vector(scale_x,1,1))
    c=a.get_component_by_class(unreal.StaticMeshComponent)
    c.set_static_mesh(mesh)
    c.set_collision_profile_name('NoCollision')
    c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    c.set_editor_property('can_ever_affect_navigation',False)
    a.set_actor_enable_collision(False)
    origin,extent=a.get_actor_bounds(False)
    assert abs(origin.x-1200)<1 and abs(origin.y-y)<1
    assert extent.x<=25.1 and 199.0<=extent.y<=200.1
    assert origin.z-extent.z>=-.2 and origin.z+extent.z<=600.2
    assert origin.y-extent.y>=42300-.2 and origin.y+extent.y<=43900+.2
    spawned[label]=dict(transform=a.get_actor_transform().export_text(),
                        mesh=c.static_mesh.get_path_name(),
                        collision=str(c.get_collision_enabled()))
assert len(spawned)==4
assert helpers['snapshot_actor_state']([a for a in actors if a not in (owner,seat)])==other_before
assert owner.get_actor_transform().export_text()==owner_pose
assert owner.get_actor_enable_collision()==owner_collision
assert dict(transform=native.get_actor_transform().export_text(),
            collision=str(native_c.get_collision_enabled()),
            enabled=native.get_actor_enable_collision(),
            visible=native_c.get_editor_property('visible'))==native_before

report=dict(status='unsaved_preview',map_hashes_before=before,mesh_sha256=mesh_sha,
            removed_stock_fin_instances=114,new_custom_bays=4,
            native_east_collision_preserved=True,west_window_and_route_unchanged=True,
            table_and_six_chairs_preserved=True,obsolete_visual_bench_hidden=True,
            seat_before=seat_before,native_before=native_before,spawned=spawned)
if save:
    assert review['spawned']==spawned and review['seat_before']==seat_before
    shutil.copy2(maps['L_Aurelion_M13.umap'],out/'L_Aurelion_M13.before.umap')
    assert level.save_current_level() and level.load_level('/Game/Aurelion/Maps/L_Aurelion_M13')
    restored={a.get_actor_label():a for a in actorsub.get_all_level_actors()}
    restored_owner=restored[survey['owner']]
    restored_part=next(c for c in restored_owner.get_components_by_class(unreal.InstancedStaticMeshComponent)
                       if c.static_mesh and c.static_mesh.get_path_name()==survey['vendor_mesh'])
    assert restored_part.get_instance_count()==0
    s=restored['Aurelion_Radiance_ObservationGallerySeat']
    sc=s.get_component_by_class(unreal.StaticMeshComponent)
    assert s.get_actor_transform().export_text()==seat_before['transform']
    assert not sc.get_editor_property('visible') and sc.get_editor_property('hidden_in_game')
    assert sc.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
    n=restored['Z11_Wall_EW1'];nc=n.get_component_by_class(unreal.StaticMeshComponent)
    assert dict(transform=n.get_actor_transform().export_text(),
                collision=str(nc.get_collision_enabled()),
                enabled=n.get_actor_enable_collision(),
                visible=nc.get_editor_property('visible'))==native_before
    for label,row in spawned.items():
        a=restored[label];c=a.get_component_by_class(unreal.StaticMeshComponent)
        assert a.get_actor_transform().export_text()==row['transform']
        assert c.static_mesh.get_path_name()==row['mesh']
        assert c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
        assert not a.get_actor_enable_collision() and not c.get_editor_property('can_ever_affect_navigation')
    assert digest(maps['L_Aurelion_M12.umap'])==before['L_Aurelion_M12.umap']
    report.update(status='saved_reloaded',map_hash_after=digest(maps['L_Aurelion_M13.umap']))
(out/'z11-east-wall-fit.json').write_text(json.dumps(report,indent=2))
runpy.run_path(str(root/'Scripts/Editor/preview_m13_route.py'),init_globals={
    'ALLOW_DIRTY_PREVIEW':not save,
    'M13_ROUTE_VIEWS':[('east-room',(-400,43100,190)),('east-close',(650,43100,190)),
                       ('z11-wall-center',(750,43100,190))],
    'M13_ROUTE_YAWS':{'east-room':0,'east-close':0,'z11-wall-center':180},
    'M13_ROUTE_PITCHES':{'east-room':-6,'east-close':-6,'z11-wall-center':-8},
})
print('M13_Z11_EAST_WALL_'+('SAVED' if save else 'UNSAVED_PREVIEW')+'_PASS')
