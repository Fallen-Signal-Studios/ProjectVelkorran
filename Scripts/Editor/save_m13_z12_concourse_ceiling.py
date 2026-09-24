"""Persist only the reviewed Z12 roof bay layout, then verify M13 reload."""
from pathlib import Path
import hashlib
import json
import os
import runpy
import shutil
import unreal

root=Path(unreal.Paths.project_dir())
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
source=root/'Art/Source/Aurelion/Z12ConcourseCeiling'
review_dir=root/'Saved/Validation/Aurelion/Z12CeilingPreview-20260923-192812-dda6cc91'
review=json.loads((review_dir/'z12-ceiling-preview.json').read_text())
manifest=json.loads((source/'manifest.json').read_text())
assert review['status']=='unsaved_preview' and review['retained_other_actors']
assert len(review['removed_visual_instances'])==22 and len(review['created_visual_only'])==28
assert review['source_fbx_sha256']=={s['asset']:hashlib.sha256(
    (source/(s['asset']+'.fbx')).read_bytes()).hexdigest() for s in manifest['modules']}
assert all((review_dir/(name+'.png')).is_file() for name in ('center-east','center-west','center-up'))
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert world.get_name()=='L_Aurelion_M13' and not level.is_in_play_in_editor()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
existing=list(actors.get_all_level_actors())
bylabel={a.get_actor_label():a for a in existing}
owner=bylabel[review['owner_label']]
component=next(c for c in owner.get_components_by_class(unreal.InstancedStaticMeshComponent)
    if c.static_mesh and c.static_mesh.get_name()=='SM_Scifi_Floor_04')
assert component.get_instance_count()==22
assert component.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
original=[component.get_instance_transform(i,world_space=True).export_text() for i in range(22)]
assert original==review['removed_visual_instances']
helper=runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'))
baseline=helper['snapshot_actor_state']([a for a in existing if a!=owner])
maps={name:root/'Content/Aurelion/Maps'/name for name in ('L_Aurelion_M12.umap','L_Aurelion_M13.umap')}
digest=lambda path:hashlib.sha256(path.read_bytes()).hexdigest()
before={name:digest(path) for name,path in maps.items()}
assert before==review['map_sha256']
meshes={s['asset']:unreal.load_asset('/Game/Aurelion/Environment/ArchitectureKit/Meshes/'+s['asset'])
        for s in manifest['modules']}
assert all(meshes.values())
assert {k:v.get_path_name() for k,v in meshes.items()}==review['imported_meshes']
rows=[]
for preview in review['created_visual_only']:
    label=preview['label'].removeprefix('PREVIEW_')
    assert label.startswith('Z12_ConcourseRoof_')
    loc=preview['location']
    assert loc[0] in range(-1800,1801,600) and loc[1] in range(46600,48401,600) and loc[2]==560
    rows.append(dict(label=label,asset=preview['asset'],location=loc))
assert len({r['label'] for r in rows})==28
assert not any(a.get_actor_label() in {r['label'] for r in rows} for a in existing)

component.modify();component.clear_instances()
assert component.get_instance_count()==0
created=[]
for row in rows:
    actor=actors.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(*row['location']))
    assert actor
    actor.set_actor_label(row['label'])
    actor.set_folder_path('Aurelion/Z12/ConcourseCeiling')
    c=actor.static_mesh_component;c.set_static_mesh(meshes[row['asset']])
    c.set_collision_profile_name('NoCollision')
    c.set_editor_property('can_ever_affect_navigation',False)
    created.append(actor)
assert all(a.static_mesh_component.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
           and not a.static_mesh_component.get_editor_property('can_ever_affect_navigation') for a in created)
assert helper['snapshot_actor_state']([a for a in existing if a!=owner])==baseline
assert {name:digest(path) for name,path in maps.items()}==before
backup=out/'L_Aurelion_M13.before.umap'
shutil.copy2(maps['L_Aurelion_M13.umap'],backup)
assert level.save_current_level() and level.load_level('/Game/Aurelion/Maps/L_Aurelion_M13')
reloaded=list(actors.get_all_level_actors())
labels={a.get_actor_label():a for a in reloaded}
restored_owner=labels[review['owner_label']]
restored_component=next(c for c in restored_owner.get_components_by_class(unreal.InstancedStaticMeshComponent)
    if c.static_mesh and c.static_mesh.get_name()=='SM_Scifi_Floor_04')
assert restored_component.get_instance_count()==0
assert helper['snapshot_actor_state']([a for a in reloaded if a!=restored_owner and
    a.get_actor_label() not in {r['label'] for r in rows}])==baseline
for row in rows:
    assert sum(a.get_actor_label()==row['label'] for a in reloaded)==1
    actor=labels[row['label']]
    assert actor.static_mesh_component.static_mesh.get_path_name()==meshes[row['asset']].get_path_name()
    p=actor.get_actor_location()
    assert max(abs(a-b) for a,b in zip((p.x,p.y,p.z),row['location']))<.1
    assert actor.static_mesh_component.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
    assert not actor.static_mesh_component.get_editor_property('can_ever_affect_navigation')
assert digest(maps['L_Aurelion_M12.umap'])==before['L_Aurelion_M12.umap']
after=digest(maps['L_Aurelion_M13.umap'])
assert after!=before['L_Aurelion_M13.umap']
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
(out/'z12-ceiling-save.json').write_text(json.dumps(dict(status='saved_reloaded',
    reviewed_preview=str(review_dir),m13_backup=str(backup),
    m12_sha256=before['L_Aurelion_M12.umap'],m13_sha256_before=before['L_Aurelion_M13.umap'],
    m13_sha256_after=after,retired_noncolliding_panels=22,installed_noncolliding_bays=28,
    other_actor_state_preserved=True,source_fbx_sha256=review['source_fbx_sha256'],
    qualification='Editor save/reload and visual review; earned PIE and target hardware remain separate.'
),indent=2))
runpy.run_path(str(root/'Scripts/Editor/preview_m13_route.py'),init_globals={
    'M13_ROUTE_VIEWS':[('center-east',(0,47500,180)),('center-west',(0,47500,180)),
                       ('center-up',(0,47500,180))],
    'M13_ROUTE_YAWS':{'center-east':0,'center-west':180,'center-up':90},
    'M13_ROUTE_PITCHES':{'center-east':5,'center-west':5,'center-up':25}})
print('Z12_CONCOURSE_CEILING_SAVED_RELOADED_PASS')
