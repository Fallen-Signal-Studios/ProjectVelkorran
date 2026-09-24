"""Guarded preview/save of the 24 visual-only Z11 observation ceiling cassettes."""
from pathlib import Path
import hashlib
import json
import os
import runpy
import shutil
import unreal

root = Path(unreal.Paths.project_dir())
out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
source = root/'Art/Source/Aurelion/Z11ObservationCeiling'
spec = json.loads((source/'manifest.json').read_text())
assert spec['layout'] == [6,4] and spec['room_dimensions_m'] == [24,16]
assert json.loads((source/'verification.json').read_text())['status'] == 'round_trip_pass'
save = os.environ.get('SOV_Z11_CEILING_SAVE') == '1'
review = json.loads(Path(os.environ['SOV_Z11_CEILING_REVIEW']).read_text()) if save else None
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actor_sys = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_name() == 'L_Aurelion_M13' and not level.is_in_play_in_editor()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()

existing = list(actor_sys.get_all_level_actors())
by_label = {a.get_actor_label():a for a in existing}
owner = by_label['Aurelion_Art_M13_Z11_23_5cccb0']
owner_label = owner.get_actor_label()
part = next(c for c in owner.get_components_by_class(unreal.InstancedStaticMeshComponent)
            if c.static_mesh and c.static_mesh.get_name() == 'SM_Scifi_Floor_04')
assert part.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION
assert part.get_instance_count() == 24
floor_count = sum(c.get_instance_count() for label in ('Aurelion_Art_M13_Z11_0_4e2230',
                                                     'Aurelion_Art_M13_Z11_20_3eb110')
                  for c in by_label[label].get_components_by_class(unreal.InstancedStaticMeshComponent)
                  if c.static_mesh and c.static_mesh.get_name() == 'SM_Scifi_Floor_04')
assert floor_count >= 24
roof = [(i,part.get_instance_transform(i, world_space=True)) for i in range(24)]
assert len(roof) == 24
assert all(abs(t.translation.z-604.62399) < .1 for _,t in roof)
assert all(abs(t.rotation.x) > .999 and abs(t.rotation.w) < .001 for _,t in roof)

# The imported stock tile pivots at a corner. Determine actual world centres
# from its local bounds rather than treating HISM translations as centres.
bounds = part.static_mesh.get_bounds()
origin = bounds.origin
extent = bounds.box_extent
assert 199 < extent.x < 201 and 199 < extent.y < 201
centres = [(round(t.translation.x+origin.x,3),
            round(t.translation.y-origin.y,3)) for _,t in roof]
expected = [(-1000+400*ix,42500+400*iy) for ix in range(6) for iy in range(4)]
survey = dict(stock_local_bounds_origin=origin.export_text(),
              stock_local_bounds_extent=extent.export_text(),
              roof_stock_centres=centres, expected_centres=expected,
              stock_roof_instances=24, separate_stock_floor_instances=floor_count)
(out/'z11-ceiling-survey.json').write_text(json.dumps(survey,indent=2))
assert sorted(centres) == sorted(expected), 'Stock tile centring differs from the planned 4 m grid'

helper = runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'))
other_before = helper['snapshot_actor_state']([a for a in existing if a != owner])
owner_before = owner.get_actor_transform().export_text()
maps = {name:root/'Content/Aurelion/Maps'/name for name in ('L_Aurelion_M12.umap','L_Aurelion_M13.umap')}
digest = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
before = {name:digest(path) for name,path in maps.items()}
assert not save or before == review['map_hashes_before']

dest = '/Game/Aurelion/Environment/ArchitectureKit'
materials = {
    'M_Aurelion_IvoryStone':dest+'/Materials/M_AurelionKit_Ivory',
    'M_Aurelion_ObservationBlackStone':dest+'/Materials/M_AurelionKit_ObservationBlackStone',
    'M_Aurelion_ChannelShadow':dest+'/Materials/M_AurelionKit_Reveal',
    'M_Aurelion_AncientGold':dest+'/Materials/M_AurelionKit_Gold',
    'M_Aurelion_ObservationContact':'/Game/Aurelion/Art/Props/aURELION_pILLAR/Materials/M_GoldEmmissive',
}
meshes = {row['asset']:helper['import_owned_mesh'](row,source,dest+'/Meshes',materials)
          for row in spec['modules']}

owner.modify(); part.modify(); part.clear_instances()
assert part.get_instance_count() == 0
assert part.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION

spawned = {}
for ix in range(6):
    for iy in range(4):
        variant = 'B' if (ix+iy)%3 == 0 else 'A'
        asset = 'SM_Aurelion_KIT_Z11ObservationCeiling_'+variant
        x,y = expected[ix*4+iy]
        label = f'Aurelion_Custom_Z11_ObservationCeiling_{ix}_{iy}_{variant}'
        actor = actor_sys.spawn_actor_from_class(unreal.StaticMeshActor,
                                                 unreal.Vector(x,y,560),unreal.Rotator())
        assert actor
        actor.set_actor_label(label)
        c = actor.get_component_by_class(unreal.StaticMeshComponent)
        c.set_static_mesh(meshes[asset])
        c.set_collision_profile_name('NoCollision')
        c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        c.set_editor_property('can_ever_affect_navigation',False)
        actor.set_actor_enable_collision(False)
        p,e = actor.get_actor_bounds(False)
        assert abs(p.x-x) < .1 and abs(p.y-y) < .1
        assert e.x <= 200.1 and e.y <= 200.1
        assert p.z-e.z >= 559.5 and p.z+e.z <= 602
        spawned[label] = dict(transform=actor.get_actor_transform().export_text(),
                              mesh=c.static_mesh.get_path_name(),
                              collision=str(c.get_collision_enabled()))
assert len(spawned) == 24
assert helper['snapshot_actor_state']([a for a in existing if a != owner]) == other_before
assert owner.get_actor_transform().export_text() == owner_before
assert {name:digest(path) for name,path in maps.items()} == before

report = dict(status='unsaved_preview',map_hashes_before=before,
              removed_visual_ceiling_instances=24,retained_separate_visual_floor_instances=floor_count,
              new_custom_visual_ceiling_bays=24,native_collision_unchanged=True,
              other_actors_unchanged=True,spawned=spawned,
              source_fbx_sha256={row['asset']:digest(source/(row['asset']+'.fbx')) for row in spec['modules']})
if save:
    assert spawned == review['spawned']
    assert report['source_fbx_sha256'] == review['source_fbx_sha256']
    shutil.copy2(maps['L_Aurelion_M13.umap'],out/'L_Aurelion_M13.before.umap')
    assert level.save_current_level() and level.load_level('/Game/Aurelion/Maps/L_Aurelion_M13')
    restored = {a.get_actor_label():a for a in actor_sys.get_all_level_actors()}
    restored_part = next(c for c in restored[owner_label].get_components_by_class(unreal.InstancedStaticMeshComponent)
                         if c.static_mesh and c.static_mesh.get_name() == 'SM_Scifi_Floor_04')
    assert restored_part.get_instance_count() == 0
    assert restored_part.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION
    for label,row in spawned.items():
        a = restored[label]; c = a.get_component_by_class(unreal.StaticMeshComponent)
        assert a.get_actor_transform().export_text() == row['transform']
        assert c.static_mesh.get_path_name() == row['mesh']
        assert c.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION
        assert not a.get_actor_enable_collision() and not c.get_editor_property('can_ever_affect_navigation')
    assert digest(maps['L_Aurelion_M12.umap']) == before['L_Aurelion_M12.umap']
    report.update(status='saved_reloaded',map_hash_after=digest(maps['L_Aurelion_M13.umap']))
(out/'z11-observation-ceiling-fit.json').write_text(json.dumps(report,indent=2))
runpy.run_path(str(root/'Scripts/Editor/preview_m13_route.py'),init_globals={
    'ALLOW_DIRTY_PREVIEW':not save,
    'M13_ROUTE_VIEWS':[('z11-ceiling-entry',(650,42650,190)),
                       ('z11-ceiling-room',(700,43100,190)),
                       ('z11-ceiling-under',(0,43100,190))],
    'M13_ROUTE_YAWS':{'z11-ceiling-entry':170,'z11-ceiling-room':180,'z11-ceiling-under':90},
    'M13_ROUTE_PITCHES':{'z11-ceiling-entry':19,'z11-ceiling-room':23,'z11-ceiling-under':42},
})
print('M13_Z11_OBSERVATION_CEILING_'+('SAVED' if save else 'UNSAVED_PREVIEW')+'_PASS')
