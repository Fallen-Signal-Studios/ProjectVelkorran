"""Unsaved M13 fit review for rare custom 3D berth-side service coffers."""
from pathlib import Path
import hashlib
import json
import os
import runpy
import shutil
import unreal

root = Path(unreal.Paths.project_dir())
out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
source = root / 'Art/Source/Aurelion/Z12DepartureRegister'
manifest = json.loads((source / 'manifest.json').read_text())
assert json.loads((source / 'verification.json').read_text())['status'] == 'round_trip_pass'
assert len(manifest['modules']) == 1
spec = manifest['modules'][0]
persist = os.environ.get('SOV_Z12_REGISTER_SAVE') == '1'
review = None
if persist:
    review_path = Path(os.environ.get('SOV_Z12_REGISTER_REVIEWED',
        root / 'Saved/Validation/Aurelion/Z12RegisterPreviewTransform-20260923-223522-18ae92fb/service-register-preview.json'))
    review = json.loads(review_path.read_text())
    assert review['status'] == 'unsaved_preview'
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actor_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_name() == 'L_Aurelion_M13' and not level.is_in_play_in_editor()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
helper = runpy.run_path(str(root / 'Scripts/Editor/aurelion_architecture_helpers.py'))
actors = list(actor_sub.get_all_level_actors())
by_label = {a.get_actor_label(): a for a in actors}
owner = by_label['Aurelion_Art_M13_Z12_6_21a580']
owner_label = owner.get_actor_label()
other_state = helper['snapshot_actor_state']([a for a in actors if a is not owner])
native_collision = {label: (by_label[label].get_actor_transform().export_text(),
                            str(by_label[label].static_mesh_component.get_collision_enabled()))
                    for label in ('Z12_Wall_EW-1', 'Z12_Wall_EW1')}
assert all(value[1] == str(unreal.CollisionEnabled.QUERY_AND_PHYSICS)
           for value in native_collision.values())
component = next(c for c in owner.get_components_by_class(unreal.InstancedStaticMeshComponent)
                 if c.static_mesh and c.static_mesh.get_name() == 'SM_Aurelion_KIT_Z12CofferSide')
assert component.get_instance_count() == 32
assert component.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION
original = [component.get_instance_transform(i, world_space=True)
            for i in range(component.get_instance_count())]
targets = [i for i,t in enumerate(original)
           if abs(abs(t.translation.x)-2100)<.01
           and (abs(t.translation.y-46900)<.01 or abs(t.translation.y-48100)<.01)
           and abs(t.translation.z-150)<.01]
assert len(targets) == 8, targets
before = {name: hashlib.sha256((root / 'Content/Aurelion/Maps' / name).read_bytes()).hexdigest()
          for name in ('L_Aurelion_M12.umap','L_Aurelion_M13.umap')}
assert before['L_Aurelion_M13.umap'] == 'f50d390fb9dc648c092fb92d083d326ab364cbe5007c5dd005bd6da721726503'
source_hash = hashlib.sha256((source/(spec['asset']+'.fbx')).read_bytes()).hexdigest()
if persist:
    assert review['maps_before'] == before
    assert review['source_fbx_sha256'] == source_hash
    assert review['replaced_indices'] == targets
    assert review['replacement_transforms'] == [original[i].export_text() for i in targets]

base = '/Game/Aurelion/Environment/ArchitectureKit'
materials = {
    'M_Aurelion_IvoryStone': base+'/Materials/M_AurelionKit_PavingIvory',
    'M_Aurelion_AncientGold': base+'/Materials/M_AurelionKit_Gold',
    'M_Aurelion_ChannelShadow': base+'/Materials/M_AurelionKit_Reveal',
    'M_Aurelion_DarkSteel': base+'/Materials/M_AurelionKit_PavingBasalt',
    'M_Aurelion_BlackStone': base+'/Materials/M_AurelionKit_ObservationBlackStone',
    'M_Aurelion_LumenLens': base+'/Materials/M_AurelionKit_UplightLens',
}
mesh = helper['import_owned_mesh'](spec, source, base+'/Meshes', materials)
component.modify()
component.clear_instances()
for i,transform in enumerate(original):
    if i not in targets:
        component.add_instance(transform, world_space=True)
assert component.get_instance_count() == 24
assert [component.get_instance_transform(i, world_space=True).export_text() for i in range(24)] == [
    transform.export_text() for i,transform in enumerate(original) if i not in targets]

created = []
for i in targets:
    transform = original[i]
    actor = actor_sub.spawn_actor_from_class(unreal.StaticMeshActor, transform.translation,
                                             unreal.Rotator())
    actor.set_actor_transform(transform, False, False)
    actor.set_actor_label('PREVIEW_Aurelion_Z12_ServiceRegister_%02d' % i)
    actor.set_folder_path('Aurelion/Z12/ServiceRegisters')
    actor.static_mesh_component.set_static_mesh(mesh)
    actor.static_mesh_component.set_collision_profile_name('NoCollision')
    actor.static_mesh_component.set_editor_property('can_ever_affect_navigation', False)
    actor.set_actor_enable_collision(False)
    assert actor.static_mesh_component.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION
    actual = actor.get_actor_transform()
    assert (actual.translation-transform.translation).length()<.01, (actual.export_text(),transform.export_text())
    assert (actual.scale3d-transform.scale3d).length()<.0001, (actual.export_text(),transform.export_text())
    assert actual.rotation.angular_distance(transform.rotation)<.0001, (actual.export_text(),transform.export_text())
    created.append(actor)
assert helper['snapshot_actor_state']([a for a in actors if a is not owner]) == other_state
assert all((by_label[label].get_actor_transform().export_text(),
            str(by_label[label].static_mesh_component.get_collision_enabled())) == state
           for label,state in native_collision.items())
assert {name: hashlib.sha256((root / 'Content/Aurelion/Maps' / name).read_bytes()).hexdigest()
        for name in before} == before
report = dict(status='unsaved_preview', maps_before=before,
              mesh=mesh.get_path_name(), source_fbx_sha256=source_hash,
              original_side_coffers=32, remaining_side_coffers=24,
              replaced_indices=targets,
              retained_transforms=[t.export_text() for i,t in enumerate(original) if i not in targets],
              replacement_transforms=[t.export_text() for i,t in enumerate(original) if i in targets],
              native_wall_collision_preserved=True, other_actor_state_preserved=True,
              visual_only=True, no_navigation=True)
if persist:
    labels = [a.get_actor_label() for a in created]
    backup = out/'L_Aurelion_M13.before.umap'
    shutil.copy2(root/'Content/Aurelion/Maps/L_Aurelion_M13.umap',backup)
    assert hashlib.sha256(backup.read_bytes()).hexdigest() == before['L_Aurelion_M13.umap']
    assert level.save_current_level()
    assert level.load_level('/Game/Aurelion/Maps/L_Aurelion_M13')
    reloaded = list(actor_sub.get_all_level_actors())
    found = {a.get_actor_label():a for a in reloaded}
    restored_owner = found[owner_label]
    restored_component = next(c for c in restored_owner.get_components_by_class(unreal.InstancedStaticMeshComponent)
                              if c.static_mesh and c.static_mesh.get_name() == 'SM_Aurelion_KIT_Z12CofferSide')
    assert restored_component.get_instance_count() == 24
    assert [restored_component.get_instance_transform(i, world_space=True).export_text()
            for i in range(24)] == report['retained_transforms']
    for i,label in zip(targets,labels):
        actor = found[label]
        assert actor.static_mesh_component.static_mesh.get_path_name() == mesh.get_path_name()
        assert actor.static_mesh_component.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION
        assert not actor.static_mesh_component.get_editor_property('can_ever_affect_navigation')
        assert not actor.get_actor_enable_collision()
        actual = actor.get_actor_transform()
        expected = original[i]
        assert (actual.translation-expected.translation).length()<.01
        assert (actual.scale3d-expected.scale3d).length()<.0001
        assert actual.rotation.angular_distance(expected.rotation)<.0001
    assert helper['snapshot_actor_state']([a for a in reloaded
                                          if a.get_actor_label() not in labels and a is not restored_owner]) == other_state
    assert all((found[label].get_actor_transform().export_text(),
                str(found[label].static_mesh_component.get_collision_enabled())) == state
               for label,state in native_collision.items())
    after = {name: hashlib.sha256((root/'Content/Aurelion/Maps'/name).read_bytes()).hexdigest()
             for name in before}
    assert after['L_Aurelion_M12.umap'] == before['L_Aurelion_M12.umap']
    assert after['L_Aurelion_M13.umap'] != before['L_Aurelion_M13.umap']
    assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    report.update(status='saved_reloaded', map_hashes_after=after, map_backup=str(backup),
                  other_actor_state_preserved_after_reload=True)
(out/'service-register-preview.json').write_text(json.dumps(report,indent=2))
runpy.run_path(str(root/'Scripts/Editor/preview_m13_route.py'),init_globals={
    'ALLOW_DIRTY_PREVIEW': not persist,
    'M13_ROUTE_VIEWS': [('west-register-near',(-900,46900,180)),
                        ('east-register-near',(900,48100,180)),
                        ('west-window',(-900,47200,180)),
                        ('east-window',(900,47800,180))],
    'M13_ROUTE_YAWS': {'west-register-near':180,'east-register-near':0,
                       'west-window':180,'east-window':0},
    'M13_ROUTE_PITCHES': {name: 8 for name in
                          ('west-register-near','east-register-near','west-window','east-window')},
})
print('Z12_SERVICE_REGISTER_'+('SAVED_RELOADED' if persist else 'UNSAVED_PREVIEW')+'_PASS')
