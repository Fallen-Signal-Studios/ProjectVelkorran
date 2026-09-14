"""Fit the custom coffer kit, retaining all existing room collision."""
import json,os,runpy,shutil,time
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY']);persist=bool(globals().get('PERSIST',False))
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world();assert world.get_name()=='L_Aurelion_M12'
subsystem=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);original=list(subsystem.get_all_level_actors());by_label={a.get_actor_label():a for a in original}
assert len(original)==2635 and not any(a.get_actor_label().startswith('KIT_Z06_Ceiling_') for a in original)
source=root/'Art/Source/Aurelion/Z06CeilingKit';fit=json.loads((source/'ceiling-fit.json').read_text())
helpers=runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'));before=helpers['snapshot_actor_state'](original)
row=fit['roof_baseline'];c=by_label[row['actor']].get_component_by_class(unreal.InstancedStaticMeshComponent)
assert [c.get_instance_transform(i,world_space=True).export_text() for i in range(c.get_instance_count())]==row['all_instance_transforms']
assert c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
destination='/Game/Aurelion/Environment/ArchitectureKit'
materials={key:destination+'/Materials/M_AurelionKit_'+value for key,value in {'M_Aurelion_IvoryStone':'PavingIvory','M_Aurelion_AncientGold':'Gold','M_Aurelion_ChannelShadow':'Reveal'}.items()}
unreal.SystemLibrary.execute_console_command(world,'Interchange.FeatureFlags.Import.FBX 0')
meshes={spec['asset']:helpers['import_owned_mesh'](spec,source,destination+'/Meshes',materials) for spec in json.loads((source/'manifest.json').read_text())['modules']}
for ci,col in enumerate(fit['columns']):
    for ri,y in enumerate(fit['rows']):
        m=meshes['SM_Aurelion_KIT_'+col['suffix']];b=m.get_bounds()
        a=subsystem.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(col['x'],y,100-b.origin.z-b.box_extent.z));a.set_actor_label(f'KIT_Z06_Ceiling_{ci:02}_{ri:02}');a.set_folder_path('Aurelion/CustomArchitecture/Z06/Ceiling')
        nc=a.static_mesh_component;nc.set_static_mesh(m);nc.set_collision_profile_name('NoCollision');nc.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION);a.set_actor_enable_collision(False)
c.modify();c.set_visibility(False,False);c.set_hidden_in_game(True,False)
assert helpers['snapshot_actor_state'](original)==before,'Original actor transform or collision changed'
geometry=runpy.run_path(str(root/'Scripts/Editor/check_z06_ceiling.py'))['check_z06_ceiling'](world,list(subsystem.get_all_level_actors()))
if persist:
    shutil.copy2(root/'Content/Aurelion/Maps/L_Aurelion_M12.umap',out/'L_Aurelion_M12-before.umap');assert editor.save_current_level()
(out/'z06-ceiling-fit.json').write_text(json.dumps(dict(status='saved' if persist else 'unsaved_preview',geometry=geometry,original_actor_states_preserved=len(original)),indent=2))
editor.editor_set_game_view(True)
capture=(root/'Scripts/Editor/fit_z01_lower_architecture.py').read_text(encoding='utf-8-sig').split("p=by_label['Z01_Entry_StandIn']",1)[1]
capture=capture.replace("('west-wall',unreal.Vector(-7600,-14900,165),unreal.Rotator(pitch=12,yaw=180),75)","('ceiling-detail',unreal.Vector(0,10500,-350),unreal.Rotator(pitch=35,yaw=-90),90)").replace('z01-','z06-')
exec(compile("p=by_label['Z06_Entry_StandIn']"+capture,'z06_ceiling_capture','exec'))
