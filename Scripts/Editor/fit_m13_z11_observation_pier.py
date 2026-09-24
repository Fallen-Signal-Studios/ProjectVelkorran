"""Guarded player-height preview/save of Z11's visual-only window pier cladding."""
from pathlib import Path
import hashlib
import json
import os
import runpy
import shutil

import unreal

root = Path(unreal.Paths.project_dir())
out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
source = root/'Art/Source/Aurelion/Z11ObservationPier'
spec = json.loads((source/'manifest.json').read_text())
assert json.loads((source/'verification.json').read_text())['status'] == 'round_trip_pass'
assert len(spec['modules']) == 1
row = spec['modules'][0]
save = os.environ.get('SOV_Z11_PIER_SAVE') == '1'
review = json.loads(Path(os.environ['SOV_Z11_PIER_REVIEW']).read_text()) if save else None

level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actor_sys = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world and world.get_name() == 'L_Aurelion_M13' and not level.is_in_play_in_editor()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()

existing = list(actor_sys.get_all_level_actors())
labels = {a.get_actor_label():a for a in existing}
assert 'Aurelion_Custom_Z11_ObservationPierCladding' not in labels
rib = labels['Z11_Rib_1_-1']
native_pos = rib.get_actor_location()
native_bounds = rib.get_actor_bounds(False)
assert abs(native_pos.x+1125) < .1 and abs(native_pos.y-43500) < .1 and abs(native_pos.z-300) < .1
assert all(abs(actual-expected) < .1 for actual,expected in zip(
    (native_bounds[1].x,native_bounds[1].y,native_bounds[1].z),(50,60,300)))
assert rib.get_actor_enable_collision()
native_component = rib.get_component_by_class(unreal.StaticMeshComponent)
assert native_component and native_component.get_collision_enabled() == unreal.CollisionEnabled.QUERY_AND_PHYSICS
surround = labels['Aurelion_Custom_Z11_ObservationWindowSurround']
assert not surround.get_actor_enable_collision()

maps = {name:root/'Content/Aurelion/Maps'/name for name in ('L_Aurelion_M12.umap','L_Aurelion_M13.umap')}
digest = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
before = {name:digest(path) for name,path in maps.items()}
assert not save or before == review['map_hashes_before']
source_hash = digest(source/(row['asset']+'.fbx'))
assert not save or source_hash == review['source_fbx_sha256']

helper = runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'))
before_actors = helper['snapshot_actor_state'](existing)
dest = '/Game/Aurelion/Environment/ArchitectureKit'
materials = {
    'M_Aurelion_IvoryStone':dest+'/Materials/M_AurelionKit_Ivory',
    'M_Aurelion_ObservationBlackStone':dest+'/Materials/M_AurelionKit_ObservationBlackStone',
    'M_Aurelion_ChannelShadow':dest+'/Materials/M_AurelionKit_Reveal',
    'M_Aurelion_AncientGold':dest+'/Materials/M_AurelionKit_Gold',
}
mesh = helper['import_owned_mesh'](row,source,dest+'/Meshes',materials)
assert mesh and mesh.get_name() == row['asset']

label = 'Aurelion_Custom_Z11_ObservationPierCladding'
actor = actor_sys.spawn_actor_from_class(unreal.StaticMeshActor,
    unreal.Vector(native_pos.x,native_pos.y,0),unreal.Rotator())
assert actor
actor.set_actor_label(label)
c = actor.get_component_by_class(unreal.StaticMeshComponent)
c.set_static_mesh(mesh)
c.set_collision_profile_name('NoCollision')
c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
c.set_editor_property('can_ever_affect_navigation',False)
actor.set_actor_enable_collision(False)
p,e = actor.get_actor_bounds(False)
assert abs(p.x-native_pos.x) < .1 and abs(p.y-native_pos.y) < .1
assert e.x <= 62.6 and e.y <= 69.1 and e.z <= 297
assert p.z-e.z >= -.2 and p.z+e.z <= 594
assert helper['snapshot_actor_state'](existing) == before_actors
assert rib.get_actor_enable_collision() and native_component.get_collision_enabled() == unreal.CollisionEnabled.QUERY_AND_PHYSICS
assert {name:digest(path) for name,path in maps.items()} == before

spawned = dict(label=label,transform=actor.get_actor_transform().export_text(),
               mesh=c.static_mesh.get_path_name(),bounds_origin=p.export_text(),
               bounds_extent=e.export_text(),collision=str(c.get_collision_enabled()))
report = dict(status='unsaved_preview',map_hashes_before=before,
              source_fbx_sha256=source_hash,measured_native_rib=dict(
                  label=rib.get_actor_label(),location=native_pos.export_text(),
                  bounds_extent=native_bounds[1].export_text()),
              native_collision_unchanged=True,existing_actors_unchanged=True,
              visual_shell=spawned)
if save:
    assert spawned == review['visual_shell']
    shutil.copy2(maps['L_Aurelion_M13.umap'],out/'L_Aurelion_M13.before.umap')
    assert level.save_current_level() and level.load_level('/Game/Aurelion/Maps/L_Aurelion_M13')
    restored = {a.get_actor_label():a for a in actor_sys.get_all_level_actors()}
    a = restored[label]
    rc = a.get_component_by_class(unreal.StaticMeshComponent)
    assert a.get_actor_transform().export_text() == spawned['transform']
    assert rc.static_mesh.get_path_name() == spawned['mesh']
    assert rc.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION
    assert not rc.get_editor_property('can_ever_affect_navigation') and not a.get_actor_enable_collision()
    rr = restored['Z11_Rib_1_-1']
    assert rr.get_actor_enable_collision()
    assert rr.get_component_by_class(unreal.StaticMeshComponent).get_collision_enabled() == unreal.CollisionEnabled.QUERY_AND_PHYSICS
    assert digest(maps['L_Aurelion_M12.umap']) == before['L_Aurelion_M12.umap']
    report.update(status='saved_reloaded',map_hash_after=digest(maps['L_Aurelion_M13.umap']))
(out/'z11-observation-pier-fit.json').write_text(json.dumps(report,indent=2))

runpy.run_path(str(root/'Scripts/Editor/preview_m13_route.py'),init_globals={
    'ALLOW_DIRTY_PREVIEW':not save,
    'M13_ROUTE_VIEWS':[('z11-pier-room',(700,43100,190)),
                       ('z11-pier-near',(0,43100,190))],
    'M13_ROUTE_YAWS':{'z11-pier-room':180,'z11-pier-near':180},
    'M13_ROUTE_PITCHES':{'z11-pier-room':23,'z11-pier-near':7},
})
print('M13_Z11_OBSERVATION_PIER_'+('SAVED' if save else 'UNSAVED_PREVIEW')+'_PASS')
