"""Save the reviewed visual-only Z12 open side-girder replacement in M13."""
from pathlib import Path
import hashlib
import json
import os
import runpy
import shutil
import unreal

root=Path(unreal.Paths.project_dir())
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
review_dir=root/'Saved/Validation/Aurelion/Z12SparseCanopyGirder-20260924-012740-ee28df30'
review=json.loads((review_dir/'sparse-canopy-girder-preview.json').read_text())
assert review['status']=='unsaved_visual_preview' and review['hidden_roof_visuals']==66
assert len(review['placed_girders'])==16 and review['girder_visual_only']
assert review['all_other_actor_state_retained'] and review['map_files_unchanged']
assert all((review_dir/(name+'.png')).is_file() for name in ('west-sparse-girder','east-sparse-girder'))
source=root/'Art/Source/Aurelion/Z12OpenCanopyGirder'
manifest=json.loads((source/'manifest.json').read_text())
spec=manifest['modules'][0]
assert json.loads((source/'verification.json').read_text())['status']=='round_trip_pass'
digest=lambda path:hashlib.sha256(path.read_bytes()).hexdigest()
assert digest(source/(spec['asset']+'.fbx'))==review['mesh_source_sha256']
mesh=unreal.load_asset(review['mesh_path'])
assert isinstance(mesh,unreal.StaticMesh)
sm=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
assert sm.get_num_uv_channels(mesh,0)==2
assert sm.get_simple_collision_count(mesh)==0 and sm.get_convex_collision_count(mesh)==0
assert Path(mesh.get_editor_property('asset_import_data').get_first_filename()).resolve()==(source/(spec['asset']+'.fbx')).resolve()

level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
api=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_name()=='L_Aurelion_M13' and not level.is_in_play_in_editor()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
maps={name:root/'Content/Aurelion/Maps'/name for name in ('L_Aurelion_M12.umap','L_Aurelion_M13.umap')}
before={name:digest(path) for name,path in maps.items()}
assert before==review['map_hashes_before']
old_manifest=json.loads((root/'Art/Source/Aurelion/Z12DepartureCanopy/manifest.json').read_text())
plan=runpy.run_path(str(root/'Scripts/Editor/aurelion_z12_canopy_plan.py'))['plan'](old_manifest)
roof=[row for row in plan if '_Canopy_Coffer_' in row['label'] or '_Canopy_Rib_' in row['label']]
assert len(roof)==66
existing=list(api.get_all_level_actors())
by_label={a.get_actor_label():a for a in existing}
targets=[by_label[r['label']] for r in roof]
assert all(a.static_mesh_component.get_editor_property('visible') and
           a.static_mesh_component.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
           for a in targets)
helper=runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'))
others=helper['snapshot_actor_state']([a for a in existing if a not in targets])
assert not any(label in by_label for label in review['placed_girders'])
for actor in targets:
    actor.static_mesh_component.set_visibility(False)
    actor.static_mesh_component.set_hidden_in_game(True)
created=[]
for dock,cx in (('Dominion',-3800),('Reformation',3800)):
    for side,x in (('Left',-13.2),('Right',13.2)):
        for i,y in enumerate((-6.225,-2.075,2.075,6.225)):
            label='Aurelion_Z12_%s_OpenCanopyGirder_%s_%d'%(dock,side,i)
            actor=api.spawn_actor_from_class(unreal.StaticMeshActor,
                unreal.Vector(cx+x*100,47500+y*100,750))
            assert actor
            actor.set_actor_label(label)
            actor.set_folder_path('Aurelion/Z12/OpenCanopy')
            comp=actor.static_mesh_component
            comp.set_static_mesh(mesh)
            comp.set_collision_profile_name('NoCollision')
            comp.set_editor_property('can_ever_affect_navigation',False)
            actor.set_actor_enable_collision(False)
            created.append(actor)
assert [a.get_actor_label().replace('Aurelion_','PREVIEW_Aurelion_',1) for a in created]==review['placed_girders']
assert helper['snapshot_actor_state']([a for a in existing if a not in targets])==others
assert all(a.static_mesh_component.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
           and not a.static_mesh_component.get_editor_property('can_ever_affect_navigation')
           for a in created)
assert {name:digest(path) for name,path in maps.items()}==before
backup=out/'L_Aurelion_M13.before.umap'
shutil.copy2(maps['L_Aurelion_M13.umap'],backup)
assert level.save_current_level()
after={name:digest(path) for name,path in maps.items()}
assert after['L_Aurelion_M12.umap']==before['L_Aurelion_M12.umap']
assert after['L_Aurelion_M13.umap']!=before['L_Aurelion_M13.umap']
(out/'sparse-canopy-save.json').write_text(json.dumps(dict(
    status='saved_requires_fresh_editor_verification',reviewed_preview=str(review_dir),
    hidden_roof_labels=[a.get_actor_label() for a in targets],
    new_girder_labels=[a.get_actor_label() for a in created],
    retained_feet_and_pendants=36,visual_only=True,
    mesh_path=mesh.get_path_name(),mesh_source_sha256=review['mesh_source_sha256'],
    map_hashes_before=before,map_hashes_after=after,map_backup=str(backup),
    all_other_actor_state_retained=True,new_collision_and_nav_disabled=True),indent=2))
print('M13_SPARSE_CANOPY_SAVED_REQUIRES_FRESH_EDITOR_VERIFICATION')
