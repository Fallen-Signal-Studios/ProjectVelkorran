"""Replace the measured breach-rescue ground paving with the authored Blender kit."""
import json,os,runpy,shutil,time
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY']);persist=bool(globals().get('PERSIST',False))
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world();assert world.get_name()=='L_Aurelion_M12'
subsystem=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);original=list(subsystem.get_all_level_actors());by_label={a.get_actor_label():a for a in original}
assert len(original)==2509 and not any(a.get_actor_label().startswith('KIT_Z06_Paving_') for a in original)
fit=json.loads((root/'Art/Source/Aurelion/Z06PavingFit/floor-fit.json').read_text())
helpers=runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'))
guide_names={r['actor'] for r in fit['guides']};unchanged=[a for a in original if a.get_actor_label() not in guide_names];before=helpers['snapshot_actor_state'](unchanged)
c=by_label[fit['floor']['actor']].get_component_by_class(unreal.InstancedStaticMeshComponent)
assert [c.get_instance_transform(i,world_space=True).export_text() for i in range(c.get_instance_count())]==fit['floor']['all_instance_transforms']
c.modify()
for row in sorted(fit['selected'],key=lambda r:r['index'],reverse=True):assert c.remove_instance(row['index'])
meshes={col['suffix']:unreal.load_asset('/Game/Aurelion/Environment/ArchitectureKit/Meshes/SM_Aurelion_KIT_'+col['suffix']) for col in fit['columns']}
assert all(meshes.values())
for ci,col in enumerate(fit['columns']):
    for ri,y in enumerate(fit['rows']):
        mesh=meshes[col['suffix']];b=mesh.get_bounds()
        a=subsystem.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(col['x'],y,-600-b.origin.z-b.box_extent.z))
        a.set_actor_label(f'KIT_Z06_Paving_{ci:02}_{ri:02}');a.set_folder_path('Aurelion/CustomArchitecture/Z06/Paving')
        c=a.static_mesh_component;c.set_static_mesh(mesh);c.set_collision_profile_name('NoCollision');c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION);a.set_actor_enable_collision(False)
for row in fit['guides']:
    a=by_label[row['actor']];c=a.static_mesh_component
    assert a.get_actor_transform().export_text()==row['actor_transform'] and c.static_mesh.get_path_name()==row['mesh']
    assert c.get_collision_enabled()==unreal.CollisionEnabled.QUERY_AND_PHYSICS
    a.modify();c.modify();a.set_actor_enable_collision(False);c.set_collision_profile_name('NoCollision');c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION);c.set_visibility(False,False);c.set_hidden_in_game(True,False)
c=by_label[fit['centerline']['actor']].get_component_by_class(unreal.InstancedStaticMeshComponent)
assert c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
c.modify();c.set_visibility(False,False);c.set_hidden_in_game(True,False)
assert helpers['snapshot_actor_state'](unchanged)==before,'Unrelated actor transform/collision changed'
geometry=runpy.run_path(str(root/'Scripts/Editor/check_z06_paving.py'))['check_z06_paving'](world,list(subsystem.get_all_level_actors()))
if persist:
    shutil.copy2(root/'Content/Aurelion/Maps/L_Aurelion_M12.umap',out/'L_Aurelion_M12-before.umap');assert editor.save_current_level()
(out/'z06-paving-fit.json').write_text(json.dumps(dict(status='saved' if persist else 'unsaved_preview',geometry=geometry,preserved_actor_states=len(unchanged)),indent=2))
editor.editor_set_game_view(True)
capture=(root/'Scripts/Editor/fit_z01_lower_architecture.py').read_text(encoding='utf-8-sig').split("p=by_label['Z01_Entry_StandIn']",1)[1]
capture=capture.replace("('west-wall',unreal.Vector(-7600,-14900,165),unreal.Rotator(pitch=12,yaw=180),75)","('room-return',unreal.Vector(0,11350,-350),unreal.Rotator(pitch=6,yaw=-90),90)").replace('z01-','z06-')
exec(compile("p=by_label['Z06_Entry_StandIn']"+capture,'z06_paving_capture','exec'))
