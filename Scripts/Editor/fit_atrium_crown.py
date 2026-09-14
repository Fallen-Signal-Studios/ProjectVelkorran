"""Replace detached pitched railing spans with an open architectural crown."""
import json,os,runpy,shutil,time,math
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY']);persist=bool(globals().get('PERSIST',False))
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world();assert world.get_name()=='L_Aurelion_M12'
subsystem=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);original=list(subsystem.get_all_level_actors());by_label={a.get_actor_label():a for a in original};assert len(original)==2484
source=root/'Art/Source/Aurelion/AtriumCrownKit';baseline=json.loads((source/'placement-baseline.json').read_text());owned={r['actor'] for r in baseline['actors']};retained=[a for a in original if a.get_actor_label() not in owned]
settled={r['actor']:r for r in json.loads((source/'collision-baseline.json').read_text())['settled']}
helpers=runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'));before=helpers['snapshot_actor_state'](retained)
checks=runpy.run_path(str(root/'Scripts/Editor/check_atrium_crown.py'));lanes_before=checks['ring_lanes'](world);assert all(r['blocker'] is None for r in lanes_before),lanes_before
destination='/Game/Aurelion/Environment/ArchitectureKit';materials={key:destination+'/Materials/M_AurelionKit_'+value for key,value in {'M_Aurelion_IvoryStone':'PavingIvory','M_Aurelion_StoneGrout':'StoneGrout','M_Aurelion_AncientGold':'Gold'}.items()}
unreal.SystemLibrary.execute_console_command(world,'Interchange.FeatureFlags.Import.FBX 0')
meshes={s['asset']:helpers['import_owned_mesh'](s,source,destination+'/Meshes',materials) for s in json.loads((source/'manifest.json').read_text())['modules']}
for row in baseline['actors']:
 a=by_label[row['actor']];c=a.static_mesh_component
 assert a.get_actor_transform().export_text()==row['actor_transform'] and c.get_world_transform().export_text()==row['component_transform'] and c.static_mesh.get_path_name()==row['mesh']
 expected=settled[row['actor']]
 assert a.get_actor_enable_collision()==expected['actor_collision'] and str(c.get_collision_enabled())==expected['collision'] and str(c.get_collision_profile_name())==expected['profile'],(row['actor'],str(c.get_collision_profile_name()),expected['profile'])
 for name,value in (('Pawn',2),('Visibility',3),('Camera',4)):assert str(c.get_collision_response_to_channel(unreal.CollisionChannel.cast(value)))==expected['responses'][name]
 assert c.get_editor_property('visible')==row['visible'] and c.get_editor_property('hidden_in_game')==row['hidden']
 a.modify();c.modify();a.set_actor_enable_collision(False);c.set_collision_profile_name('NoCollision');c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION);c.set_visibility(False,False);c.set_hidden_in_game(True,False)
for i in range(9):
 angle=22.5+i*45 if i<8 else 0;r=math.radians(angle);p=unreal.Vector(3230*math.cos(r),3230*math.sin(r),0) if i<8 else unreal.Vector()
 a=subsystem.spawn_actor_from_class(unreal.StaticMeshActor,p,unreal.Rotator(yaw=angle));a.set_actor_label(f'KIT_Atrium_Crown_Pier_{i:02}' if i<8 else 'KIT_Atrium_Crown_Ribs');a.set_folder_path('Aurelion/CustomArchitecture/Z05/OpenCrown')
 c=a.static_mesh_component;c.set_static_mesh(meshes['SM_Aurelion_KIT_AtriumCrownPier' if i<8 else 'SM_Aurelion_KIT_AtriumOpenCrown']);c.set_collision_profile_name('BlockAll' if i<8 else 'NoCollision');c.set_collision_enabled(unreal.CollisionEnabled.QUERY_AND_PHYSICS if i<8 else unreal.CollisionEnabled.NO_COLLISION);a.set_actor_enable_collision(i<8)
assert helpers['snapshot_actor_state'](retained)==before
geometry=checks['check_crown'](world,list(subsystem.get_all_level_actors()));assert geometry['ring_lanes']==lanes_before
if persist:
 shutil.copy2(root/'Content/Aurelion/Maps/L_Aurelion_M12.umap',out/'L_Aurelion_M12-before.umap');assert editor.save_current_level()
(out/'atrium-crown-fit.json').write_text(json.dumps(dict(status='saved' if persist else 'unsaved_preview',geometry=geometry,unrelated_actors_preserved=len(retained)),indent=2))
editor.editor_set_game_view(True)
capture=(root/'Scripts/Editor/fit_z01_lower_architecture.py').read_text(encoding='utf-8-sig').split("p=by_label['Z01_Entry_StandIn']",1)[1]
capture=capture.replace("('entry',unreal.Vector(p.x,p.y,p.z+165),unreal.Rotator(yaw=90),90)","('crown-entry',unreal.Vector(-3200,-500,175),unreal.Rotator(pitch=22,yaw=10),90)").replace("('west-wall',unreal.Vector(-7600,-14900,165),unreal.Rotator(pitch=12,yaw=180),75)","('crown-overview',unreal.Vector(-5900,-5900,3600),unreal.Rotator(pitch=-26,yaw=45),90)").replace('z01-','atrium-')
exec(compile("p=by_label['Z02_Entry_StandIn']"+capture,'atrium_crown_capture','exec'))
