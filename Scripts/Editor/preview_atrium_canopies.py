"""Install four fitted split-vault porticos over the existing radial bridges."""
import json,os,runpy,shutil,time,math
from pathlib import Path
import unreal
persist=bool(globals().get('PERSIST',False));root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world();assert world.get_name()=='L_Aurelion_M12'
subsystem=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);original=list(subsystem.get_all_level_actors());by_label={a.get_actor_label():a for a in original};assert len(original)==2472 and not any(a.get_actor_label().startswith('KIT_Atrium_Canopy_') for a in original)
helpers=runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'));before=helpers['snapshot_actor_state'](original);source=root/'Art/Source/Aurelion/AtriumCanopyKit'
destination='/Game/Aurelion/Environment/ArchitectureKit';materials={key:destination+'/Materials/M_AurelionKit_'+value for key,value in {'M_Aurelion_IvoryStone':'PavingIvory','M_Aurelion_StoneGrout':'StoneGrout','M_Aurelion_AncientGold':'Gold','M_Aurelion_ChannelShadow':'Reveal'}.items()}
unreal.SystemLibrary.execute_console_command(world,'Interchange.FeatureFlags.Import.FBX 0')
meshes={s['asset']:helpers['import_owned_mesh'](s,source,destination+'/Meshes',materials) for s in json.loads((source/'manifest.json').read_text())['modules']}
for angle in (0,90,180,270):
 for kind in ('Frame','Vault'):
  r=math.radians(angle);a=subsystem.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(2300*math.cos(r),2300*math.sin(r),0),unreal.Rotator(yaw=angle+90));a.set_actor_label(f'KIT_Atrium_Canopy_{angle}_{kind}');a.set_folder_path('Aurelion/CustomArchitecture/Z05/Canopies')
  c=a.static_mesh_component;c.set_static_mesh(meshes['SM_Aurelion_KIT_AtriumCanopy'+kind]);c.set_collision_profile_name('NoCollision');c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION);a.set_actor_enable_collision(False)
for row in json.loads((source/'canopy-baseline.json').read_text())['actors']:
 if 'BridgeCanopy' in row['actor'] or 'CeramicCanopyPylon' in row['actor']:
  c=by_label[row['actor']].static_mesh_component;assert c.get_world_transform().export_text()==row['component_transform'];c.modify();c.set_visibility(False,False);c.set_hidden_in_game(True,False)
assert helpers['snapshot_actor_state'](original)==before
geometry=runpy.run_path(str(root/'Scripts/Editor/check_atrium_canopies.py'))['check_canopies'](world,list(subsystem.get_all_level_actors()))
if persist:
 shutil.copy2(root/'Content/Aurelion/Maps/L_Aurelion_M12.umap',out/'L_Aurelion_M12-before.umap');assert editor.save_current_level()
(out/'atrium-canopy-fit.json').write_text(json.dumps(dict(status='saved' if persist else 'unsaved_preview',geometry=geometry,original_actors_preserved=len(original)),indent=2))
editor.editor_set_game_view(True)
capture=(root/'Scripts/Editor/fit_z01_lower_architecture.py').read_text(encoding='utf-8-sig').split("p=by_label['Z01_Entry_StandIn']",1)[1]
capture=capture.replace("('entry',unreal.Vector(p.x,p.y,p.z+165),unreal.Rotator(yaw=90),90)","('canopy-overview',unreal.Vector(-5900,-5900,4500),unreal.Rotator(pitch=-32,yaw=45),90)").replace("('west-wall',unreal.Vector(-7600,-14900,165),unreal.Rotator(pitch=12,yaw=180),75)","('canopy-entry',unreal.Vector(-3100,-100,175),unreal.Rotator(pitch=16,yaw=0),90)").replace('z01-','atrium-')
exec(compile("p=by_label['Z02_Entry_StandIn']"+capture,'atrium_canopy_capture','exec'))
