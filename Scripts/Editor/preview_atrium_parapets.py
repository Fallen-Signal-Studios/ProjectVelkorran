"""Replace ring and radial-bridge railings using fitted miter profiles."""
import json,os,runpy,shutil,time
from pathlib import Path
import unreal
persist=bool(globals().get('PERSIST',False));root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
helpers=runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'));checks=runpy.run_path(str(root/'Scripts/Editor/check_atrium_parapets.py'))
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world();assert world.get_name()=='L_Aurelion_M12'
subsystem=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);original=list(subsystem.get_all_level_actors());by_label={a.get_actor_label():a for a in original}
assert len(original)==2206 and not any(a.get_actor_label().startswith('KIT_Atrium_Parapet_') for a in original)
before=helpers['snapshot_actor_state'](original);passages=checks['passage_controls'](world);source=root/'Art/Source/Aurelion/AtriumParapetKit';fit=checks['get_fit']()
destination='/Game/Aurelion/Environment/ArchitectureKit';materials={key:destination+'/Materials/M_AurelionKit_'+value for key,value in {'M_Aurelion_IvoryStone':'PavingIvory','M_Aurelion_AncientGold':'Gold','M_Aurelion_ChannelShadow':'Reveal'}.items()}
unreal.SystemLibrary.execute_console_command(world,'Interchange.FeatureFlags.Import.FBX 0')
meshes={s['asset']:helpers['import_owned_mesh'](s,source,destination+'/Meshes',materials) for s in json.loads((source/'manifest.json').read_text())['modules']}
for spec in fit['placements']:
 a=subsystem.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(*spec['position']),unreal.Rotator(yaw=spec['yaw']));a.set_actor_label('KIT_Atrium_Parapet_'+spec['actor']);a.set_folder_path('Aurelion/CustomArchitecture/Z05/Parapets')
 c=a.static_mesh_component;c.set_static_mesh(meshes[spec['mesh']]);c.set_collision_profile_name('NoCollision');c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION);a.set_actor_enable_collision(False)
rail=next(c for c in by_label[fit['railing_actor']].get_components_by_class(unreal.InstancedStaticMeshComponent) if c.get_name()==fit['railing_component'])
assert rail.static_mesh.get_path_name()==fit['railing_mesh'] and rail.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
instance_before=[rail.get_instance_transform(i,world_space=True).export_text() for i in range(rail.get_instance_count())];assert len(instance_before)==680
indices={item['index'] for item in fit['removed_instances']};assert len(indices)==564
for item in fit['removed_instances']:assert instance_before[item['index']]==item['transform']
expected=sorted(t for i,t in enumerate(instance_before) if i not in indices);rail.modify()
for i in sorted(indices,reverse=True):assert rail.remove_instance(i)
after=sorted(rail.get_instance_transform(i,world_space=True).export_text() for i in range(rail.get_instance_count()));assert after==expected and len(after)==116
(out/'ring-railing-retained.json').write_text(json.dumps(dict(before_count=680,removed_count=564,after_count=116,transforms=after),indent=2))
assert helpers['snapshot_actor_state'](original)==before
geometry=checks['check_parapets'](world,list(subsystem.get_all_level_actors()));assert geometry['passage_controls']==passages
if persist:
 shutil.copy2(root/'Content/Aurelion/Maps/L_Aurelion_M12.umap',out/'L_Aurelion_M12-before.umap');assert editor.save_current_level()
(out/'atrium-parapet-fit.json').write_text(json.dumps(dict(status='saved' if persist else 'unsaved_preview',geometry=geometry,original_actors_preserved=len(original)),indent=2))
editor.editor_set_game_view(True)
capture=(root/'Scripts/Editor/fit_z01_lower_architecture.py').read_text(encoding='utf-8-sig').split("p=by_label['Z01_Entry_StandIn']",1)[1]
capture=capture.replace("('entry',unreal.Vector(p.x,p.y,p.z+165),unreal.Rotator(yaw=90),90)","('parapet-overview',unreal.Vector(-5900,-5900,4500),unreal.Rotator(pitch=-32,yaw=45),90)").replace("('west-wall',unreal.Vector(-7600,-14900,165),unreal.Rotator(pitch=12,yaw=180),75)","('parapet-detail',unreal.Vector(-3200,-650,175),unreal.Rotator(pitch=-3,yaw=35),85)").replace('z01-','atrium-')
exec(compile("p=by_label['Z02_Entry_StandIn']"+capture,'atrium_parapet_capture','exec'))
