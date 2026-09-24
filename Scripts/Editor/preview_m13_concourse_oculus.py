"""Preview the custom Z12 axial oculus over two measured roof bays, unsaved."""
from pathlib import Path
import hashlib
import json
import os
import runpy
import shutil
import unreal

root=Path(unreal.Paths.project_dir())
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
source=root/'Art/Source/Aurelion/Z12ConcourseOculus'
manifest=json.loads((source/'manifest.json').read_text())
spec=manifest['module']
assert json.loads((source/'verification.json').read_text())['status']=='round_trip_pass'
persist=os.environ.get('SOV_Z12_OCULUS_SAVE')=='1'
review=json.loads(Path(os.environ['SOV_Z12_OCULUS_REVIEWED']).read_text()) if persist else None
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actor_api=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_name()=='L_Aurelion_M13' and not level.is_in_play_in_editor()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
helper=runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'))
existing=list(actor_api.get_all_level_actors())
by_label={a.get_actor_label():a for a in existing}
labels=['Z12_ConcourseRoof_2_1_B','Z12_ConcourseRoof_4_2_A']
old=[by_label[label] for label in labels]
assert all(a.static_mesh_component.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION for a in old)
assert all(a.static_mesh_component.get_editor_property('visible') for a in old)
assert all(abs(a.get_actor_location().x-x)<.01 and
           abs(a.get_actor_location().y-y)<.01 and
           abs(a.get_actor_location().z-560)<.01
           for a,x,y in zip(old,(-600,600),(47200,47800)))
roof_meshes=[a.static_mesh_component.static_mesh.get_name() for a in old]
assert all(name.startswith('SM_Aurelion_KIT_Z12ConcourseCeiling_') for name in roof_meshes)
other_state=helper['snapshot_actor_state']([a for a in existing if a not in old])
maps={name:root/'Content/Aurelion/Maps'/name
      for name in ('L_Aurelion_M12.umap','L_Aurelion_M13.umap')}
digest=lambda path:hashlib.sha256(path.read_bytes()).hexdigest()
before={name:digest(path) for name,path in maps.items()}
assert before['L_Aurelion_M13.umap']=='63b3ee34c524df7f474c0f62722bf63279c4a3243e5e48470d394b4d94b3814b'
source_hash=digest(source/(spec['asset']+'.fbx'))
if persist:
    assert review['status']=='unsaved_preview'
    assert review['map_hashes_before']==before
    assert review['source_fbx_sha256']==source_hash
    assert review['original_roof_labels']==labels
    assert review['original_roof_meshes']==roof_meshes
base='/Game/Aurelion/Environment/ArchitectureKit'
materials={
    'M_Aurelion_IvoryStone':base+'/Materials/M_AurelionKit_PavingIvory',
    'M_Aurelion_AncientGold':base+'/Materials/M_AurelionKit_Gold',
    'M_Aurelion_ChannelShadow':base+'/Materials/M_AurelionKit_Reveal',
    'M_Aurelion_BlackStone':base+'/Materials/M_AurelionKit_ObservationBlackStone',
    'M_Aurelion_LumenLens':base+'/Materials/M_AurelionKit_UplightLens',
}
mesh=helper['import_owned_mesh'](spec,source,base+'/Meshes',materials)
created=[]
for old_actor in old:
    old_actor.static_mesh_component.set_visibility(False)
    old_actor.static_mesh_component.set_hidden_in_game(True)
    position=old_actor.get_actor_location()
    new=actor_api.spawn_actor_from_class(unreal.StaticMeshActor,position)
    new.set_actor_label('PREVIEW_Z12_SovereignOculus_'+str(round(position.y)))
    new.set_folder_path('Aurelion/Z12/ConcourseOculus')
    component=new.static_mesh_component
    component.set_static_mesh(mesh)
    component.set_collision_profile_name('NoCollision')
    component.set_editor_property('can_ever_affect_navigation',False)
    new.set_actor_enable_collision(False)
    assert component.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
    center,extent=new.get_actor_bounds(False)
    assert 560<center.z-extent.z<565 and 639<center.z+extent.z<641
    created.append(dict(label=new.get_actor_label(),position=[position.x,position.y,position.z],
                        z_bounds_cm=[center.z-extent.z,center.z+extent.z]))
assert helper['snapshot_actor_state']([a for a in existing if a not in old])==other_state
assert {name:digest(path) for name,path in maps.items()}==before
report=dict(
    status='unsaved_preview',source_fbx_sha256=source_hash,
    original_roof_labels=labels,original_roof_meshes=roof_meshes,
    original_roof_collision='NoCollision',preview_actors=created,
    unrelated_actor_state_preserved=True,map_hashes_before=before,
    map_files_unchanged=True,visual_only=True)
if persist:
    backup=out/'L_Aurelion_M13.before.umap'
    shutil.copy2(maps['L_Aurelion_M13.umap'],backup)
    assert digest(backup)==before['L_Aurelion_M13.umap']
    assert level.save_current_level()
    assert level.load_level('/Game/Aurelion/Maps/L_Aurelion_M13')
    reloaded=list(actor_api.get_all_level_actors())
    found={a.get_actor_label():a for a in reloaded}
    for old_label,record in zip(labels,created):
        old_body=found[old_label].static_mesh_component
        assert not old_body.get_editor_property('visible') and old_body.get_editor_property('hidden_in_game')
        assert old_body.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
        new=found[record['label']]
        assert new.static_mesh_component.static_mesh.get_path_name()==mesh.get_path_name()
        assert new.static_mesh_component.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
        assert not new.static_mesh_component.get_editor_property('can_ever_affect_navigation')
        assert not new.get_actor_enable_collision()
        assert all(abs(v-e)<.01 for v,e in zip(
            (new.get_actor_location().x,new.get_actor_location().y,new.get_actor_location().z),
            record['position']))
    assert helper['snapshot_actor_state']([a for a in reloaded
        if a.get_actor_label() not in labels+[c['label'] for c in created]])==other_state
    after={name:digest(path) for name,path in maps.items()}
    assert after['L_Aurelion_M12.umap']==before['L_Aurelion_M12.umap']
    assert after['L_Aurelion_M13.umap']!=before['L_Aurelion_M13.umap']
    assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    report.update(status='saved_reloaded',map_hashes_after=after,
                  map_backup=str(backup),map_files_unchanged=False,
                  unrelated_actor_state_preserved_after_reload=True)
(out/'concourse-oculus-preview.json').write_text(json.dumps(report,indent=2))
runpy.run_path(str(root/'Scripts/Editor/preview_m13_route.py'),init_globals={
    'ALLOW_DIRTY_PREVIEW':not persist,
    'M13_ROUTE_VIEWS':[('oculus-axial-west',(0,47200,180)),
                       ('oculus-axial-east',(0,47800,180)),
                       ('oculus-oblique-west',(-900,47200,180)),
                       ('oculus-oblique-east',(900,47800,180))],
    'M13_ROUTE_YAWS':{'oculus-axial-west':180,'oculus-axial-east':0,
                      'oculus-oblique-west':180,'oculus-oblique-east':0},
    'M13_ROUTE_PITCHES':{'oculus-axial-west':30,'oculus-axial-east':30,
                         'oculus-oblique-west':25,'oculus-oblique-east':25},
})
print('Z12_SOVEREIGN_OCULUS_'+('SAVED_RELOADED' if persist else 'UNSAVED_PREVIEW_READY'))
