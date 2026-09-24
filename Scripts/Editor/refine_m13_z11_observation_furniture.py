"""Preview/save two versioned Z11 furniture visuals; retain all native collision."""
from pathlib import Path
import hashlib
import json
import os
import runpy

import unreal

root = Path(unreal.Paths.project_dir())
out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
source = root/'Art/Source/Aurelion/Z11ObservationFurniture'
specs = json.loads((source/'manifest.json').read_text(encoding='utf8'))['modules']
verify = json.loads((source/'verification.json').read_text(encoding='utf8'))
assert verify['status'] == 'round_trip_pass'
assert {r['asset'] for r in specs} == {r['asset'] for r in verify['modules']}
save = os.environ.get('SOV_Z11_FURNITURE_SAVE') == '1'
review = json.loads(Path(os.environ['SOV_Z11_FURNITURE_REVIEW']).read_text(encoding='utf8')) if save else None
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not level.is_in_play_in_editor()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world and world.get_name() == 'L_Aurelion_M13'
digest = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
maps = {name:root/'Content/Aurelion/Maps'/name for name in
        ('L_Aurelion_M12.umap','L_Aurelion_M13.umap')}
before = {name:digest(path) for name,path in maps.items()}
assert before['L_Aurelion_M13.umap'] == '041d982679eb677b93d2f12cc54e300cf110339499933b5204f83fbbdef1b0fc'
assert before['L_Aurelion_M12.umap'] == '64a4517bb88719694793a46ae859f0eb6fbc861eef10e956e0574d8962b844a4'
helper = runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'))
actors = list(unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors())
actor_state = helper['snapshot_actor_state'](actors)
by_label = {a.get_actor_label():a for a in actors}
visuals = ['Aurelion_Custom_Z11_ObservationTable'] + [
    'Aurelion_Custom_Z11_Chair_'+str(i) for i in range(1,7)]
assert all(label in by_label for label in visuals)
for label in visuals:
    a = by_label[label]
    component = a.get_component_by_class(unreal.StaticMeshComponent)
    assert component and not a.get_actor_enable_collision()
    assert component.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION
    assert not component.get_editor_property('can_ever_affect_navigation')
    expected = ('SM_Aurelion_KIT_Z11ObservationTable' if label.endswith('ObservationTable')
                else 'SM_Aurelion_KIT_Z11ObservationChair')
    assert component.static_mesh.get_name() == expected
for label in ['Z11_Conversation_Table'] + [
    'Z11_Chair_'+str(i)+'_'+part for i in range(1,7) for part in ('Seat','Back')]:
    a = by_label[label]
    component = a.get_component_by_class(unreal.StaticMeshComponent)
    assert a.get_actor_enable_collision() and component.get_collision_enabled() == unreal.CollisionEnabled.QUERY_AND_PHYSICS
    assert component.static_mesh.get_path_name() == '/Engine/BasicShapes/Cube.Cube'

dest = '/Game/Aurelion/Environment/ArchitectureKit'
material = {
    'M_Aurelion_BlackStone':dest+'/Materials/M_AurelionKit_ObservationBlackStone',
    'M_Aurelion_IvoryStone':dest+'/Materials/M_AurelionKit_Ivory',
    'M_Aurelion_AncientGold':dest+'/Materials/M_AurelionKit_Gold',
    'M_Aurelion_ChannelShadow':dest+'/Materials/M_AurelionKit_Reveal',
}
source_hashes = {r['asset']:digest(source/(r['asset']+'.fbx')) for r in specs}
asset_paths = {r['asset']:root/'Content/Aurelion/Environment/ArchitectureKit/Meshes'/(r['asset']+'.uasset') for r in specs}
assert not save or (review['status'] == 'unsaved_preview' and
    review['maps_before'] == before and review['source_hashes'] == source_hashes)
result = {}
loaded = {}
for spec in specs:
    mesh = (unreal.load_asset(dest+'/Meshes/'+spec['asset']) if save else
            helper['import_owned_mesh'](spec,source,dest+'/Meshes',material))
    assert mesh and mesh.get_name() == spec['asset']
    loaded[spec['asset']] = mesh
    result[spec['asset']] = dict(path=mesh.get_path_name(),
        material_slots=[mesh.get_material(i).get_path_name() for i in range(len(mesh.get_editor_property('static_materials')))],
        source_sha256=source_hashes[spec['asset']],
        asset_after=digest(asset_paths[spec['asset']]))
    assert not save or result[spec['asset']] == review['visuals'][spec['asset']]
for label in visuals:
    a = by_label[label]
    component = a.get_component_by_class(unreal.StaticMeshComponent)
    name = ('SM_Aurelion_KIT_Z11ConversationTable' if label.endswith('ObservationTable')
            else 'SM_Aurelion_KIT_Z11ConversationChair')
    component.set_static_mesh(loaded[name])
    assert component.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION
    assert not a.get_actor_enable_collision()
assert helper['snapshot_actor_state'](actors) == actor_state
assert {name:digest(path) for name,path in maps.items()} == before
report = dict(status='unsaved_preview',maps_before=before,
              actor_count=len(actors),actor_transforms_and_collision_unchanged=True,
              native_furniture_collision_preserved=True,source_hashes=source_hashes,visuals=result)
if save:
    assert level.save_current_level()
    assert level.load_level('/Game/Aurelion/Maps/L_Aurelion_M13')
    restored = {a.get_actor_label():a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()}
    assert set(visuals).issubset(restored)
    for label in visuals:
        a = restored[label]
        c = a.get_component_by_class(unreal.StaticMeshComponent)
        name = ('SM_Aurelion_KIT_Z11ConversationTable' if label.endswith('ObservationTable')
                else 'SM_Aurelion_KIT_Z11ConversationChair')
        assert c.static_mesh.get_name() == name
        assert c.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION
        assert not a.get_actor_enable_collision()
    for label in ['Z11_Conversation_Table'] + [
        'Z11_Chair_'+str(i)+'_'+part for i in range(1,7) for part in ('Seat','Back')]:
        a = restored[label]
        c = a.get_component_by_class(unreal.StaticMeshComponent)
        assert c.static_mesh.get_path_name() == '/Engine/BasicShapes/Cube.Cube'
        assert a.get_actor_enable_collision() and c.get_collision_enabled() == unreal.CollisionEnabled.QUERY_AND_PHYSICS
    after = {name:digest(path) for name,path in maps.items()}
    assert after['L_Aurelion_M12.umap'] == before['L_Aurelion_M12.umap']
    assert after['L_Aurelion_M13.umap'] != before['L_Aurelion_M13.umap']
    assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    report.update(status='saved_reloaded',maps_after=after)
(out/'z11-furniture-refinement.json').write_text(json.dumps(report,indent=2),encoding='utf8')

old = os.environ['SOV_AURELION_RUN_DIRECTORY']
captures = out/'After'
captures.mkdir(exist_ok=True)
try:
    os.environ['SOV_AURELION_RUN_DIRECTORY'] = str(captures)
    runpy.run_path(str(root/'Scripts/Editor/preview_m13_route.py'),init_globals={
        'M13_ROUTE_VIEWS': [('furniture-room',(750,43100,190)),
                            ('furniture-close',(-440,42800,150))],
        'M13_ROUTE_YAWS': {'furniture-room':180,'furniture-close':75},
        'M13_ROUTE_PITCHES': {'furniture-room':-8,'furniture-close':-12},
        'ALLOW_DIRTY_PREVIEW': not save,
    })
finally:
    os.environ['SOV_AURELION_RUN_DIRECTORY'] = old
print('Z11_FURNITURE_REIMPORT_STARTED',out)
