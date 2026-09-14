"""Fit authored long walls and piers while preserving all room collision."""
import json,os,runpy,shutil,time
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY']);persist=bool(globals().get('PERSIST',False))
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world();assert world.get_name()=='L_Aurelion_M12'
subsystem=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);original=list(subsystem.get_all_level_actors());by_label={a.get_actor_label():a for a in original}
assert len(original)==2761 and not any(a.get_actor_label().startswith('KIT_Z06_Side_') for a in original)
fit=json.loads((root/'Art/Source/Aurelion/Z06WallFit/wall-fit.json').read_text());helpers=runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'));before=helpers['snapshot_actor_state'](original)
c=by_label[fit['wall']['actor']].get_component_by_class(unreal.InstancedStaticMeshComponent)
assert [c.get_instance_transform(i,world_space=True).export_text() for i in range(c.get_instance_count())]==fit['wall']['all_instance_transforms']
assert c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
c.modify()
for row in sorted(fit['selected'],key=lambda r:r['index'],reverse=True):assert c.remove_instance(row['index'])
for side,sign,yaw in [('West',-1,-90),('East',1,90)]:
    for kind,suffix,stations,face in [('Wall','WallPlain_4x7',fit['wall_centers_y'],1674),('Pier','Pier_1x7',fit['pier_centers_y'],1574)]:
        m=unreal.load_asset('/Game/Aurelion/Environment/ArchitectureKit/Meshes/SM_Aurelion_KIT_'+suffix);assert m
        b=m.get_bounds();x=sign*(face+b.origin.y+b.box_extent.y)
        for i,y in enumerate(stations):
            a=subsystem.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(x,y,-600),unreal.Rotator(yaw=yaw));a.set_actor_label(f'KIT_Z06_Side_{side}_{kind}_{i:02}');a.set_folder_path('Aurelion/CustomArchitecture/Z06/SideWalls')
            nc=a.static_mesh_component;nc.set_static_mesh(m);nc.set_collision_profile_name('NoCollision');nc.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION);a.set_actor_enable_collision(False)
c=by_label[fit['columns']['actor']].get_component_by_class(unreal.InstancedStaticMeshComponent)
assert c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
c.modify();c.set_visibility(False,False);c.set_hidden_in_game(True,False)
assert helpers['snapshot_actor_state'](original)==before,'Existing transform or collision changed'
geometry=runpy.run_path(str(root/'Scripts/Editor/check_z06_walls.py'))['check_z06_walls'](world,list(subsystem.get_all_level_actors()))
if persist:
    shutil.copy2(root/'Content/Aurelion/Maps/L_Aurelion_M12.umap',out/'L_Aurelion_M12-before.umap');assert editor.save_current_level()
(out/'z06-wall-fit.json').write_text(json.dumps(dict(status='saved' if persist else 'unsaved_preview',geometry=geometry,original_actor_states_preserved=len(original)),indent=2))
editor.editor_set_game_view(True)
capture=(root/'Scripts/Editor/fit_z01_lower_architecture.py').read_text(encoding='utf-8-sig').split("p=by_label['Z01_Entry_StandIn']",1)[1]
capture=capture.replace("('west-wall',unreal.Vector(-7600,-14900,165),unreal.Rotator(pitch=12,yaw=180),75)","('wall-detail',unreal.Vector(-600,8000,-435),unreal.Rotator(pitch=10,yaw=-145),90)").replace('z01-','z06-')
exec(compile("p=by_label['Z06_Entry_StandIn']"+capture,'z06_wall_capture','exec'))
