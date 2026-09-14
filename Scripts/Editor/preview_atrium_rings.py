"""Fit two custom ring-floor types into all 64 existing sectors."""
import json,os,runpy,shutil,time
from pathlib import Path
import unreal
persist=bool(globals().get('PERSIST',False));root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world();assert world.get_name()=='L_Aurelion_M12'
subsystem=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);original=list(subsystem.get_all_level_actors());by_label={a.get_actor_label():a for a in original}
assert len(original)==2142 and not any(a.get_actor_label().startswith('KIT_Atrium_Ring_') for a in original)
helpers=runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'));before=helpers['snapshot_actor_state'](original)
source=root/'Art/Source/Aurelion/AtriumRingKit';baseline=json.loads((source/'placement-baseline.json').read_text())
destination='/Game/Aurelion/Environment/ArchitectureKit';materials={key:destination+'/Materials/M_AurelionKit_'+value for key,value in {'M_Aurelion_IvoryStone':'PavingIvory','M_Aurelion_StoneGrout':'StoneGrout','M_Aurelion_Basalt':'PavingBasalt','M_Aurelion_AncientGold':'Gold'}.items()}
unreal.SystemLibrary.execute_console_command(world,'Interchange.FeatureFlags.Import.FBX 0')
meshes={s['asset']:helpers['import_owned_mesh'](s,source,destination+'/Meshes',materials) for s in json.loads((source/'manifest.json').read_text())['modules']}
for group in baseline['meshes']:
 kind='Outer' if '28_36' in group['mesh'] else 'Inner'
 for row in group['users']:
  old=by_label[row['actor']];c=next(c for c in old.get_components_by_class(unreal.StaticMeshComponent) if c.get_name()==row['component'])
  assert c.get_world_transform().export_text()==row['transform'] and c.static_mesh.get_path_name().split('.')[0]==group['mesh']
  assert c.get_editor_property('visible')==row['visible'] and c.get_editor_property('hidden_in_game')==row['hidden'] and str(c.get_collision_enabled())==row['collision']
  if row['instances']:
   assert [c.get_instance_transform(i,world_space=True).export_text() for i in range(c.get_instance_count())]==row['instances']
  else:
   a=subsystem.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector());a.set_actor_transform(old.get_actor_transform(),False,False)
   a.set_actor_label('KIT_Atrium_Ring_'+row['actor']);a.set_folder_path('Aurelion/CustomArchitecture/Z05/RingFloors')
   nc=a.static_mesh_component;nc.set_static_mesh(meshes['SM_Aurelion_KIT_Atrium'+kind+'RingSector']);nc.set_collision_profile_name('NoCollision');nc.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION);a.set_actor_enable_collision(False)
  c.modify();c.set_visibility(False,False);c.set_hidden_in_game(True,False)
assert helpers['snapshot_actor_state'](original)==before
geometry=runpy.run_path(str(root/'Scripts/Editor/check_atrium_rings.py'))['check_rings'](world,list(subsystem.get_all_level_actors()))
if persist:
 shutil.copy2(root/'Content/Aurelion/Maps/L_Aurelion_M12.umap',out/'L_Aurelion_M12-before.umap');assert editor.save_current_level()
(out/'atrium-ring-fit.json').write_text(json.dumps(dict(status='saved' if persist else 'unsaved_preview',geometry=geometry,original_actors_preserved=len(original)),indent=2))
editor.editor_set_game_view(True)
capture=(root/'Scripts/Editor/fit_z01_lower_architecture.py').read_text(encoding='utf-8-sig').split("p=by_label['Z01_Entry_StandIn']",1)[1]
capture=capture.replace("('entry',unreal.Vector(p.x,p.y,p.z+165),unreal.Rotator(yaw=90),90)","('ring-overview',unreal.Vector(-5900,-5900,4500),unreal.Rotator(pitch=-32,yaw=45),90)").replace("('west-wall',unreal.Vector(-7600,-14900,165),unreal.Rotator(pitch=12,yaw=180),75)","('ring-detail',unreal.Vector(-4000,-2100,250),unreal.Rotator(pitch=-8,yaw=35),75)").replace('z01-','atrium-')
exec(compile("p=by_label['Z02_Entry_StandIn']"+capture,'atrium_ring_capture','exec'))
