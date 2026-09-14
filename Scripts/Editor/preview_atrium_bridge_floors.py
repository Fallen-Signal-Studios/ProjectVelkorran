"""Fit chord-ended paving to the four retained radial bridge slabs."""
import json,os,runpy,shutil,time,math
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY']);persist=bool(globals().get('PERSIST',False))
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world();assert world.get_name()=='L_Aurelion_M12'
subsystem=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);original=list(subsystem.get_all_level_actors());by_label={a.get_actor_label():a for a in original};assert len(original)==2480
helpers=runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'));before=helpers['snapshot_actor_state'](original)
source=root/'Art/Source/Aurelion/AtriumBridgeFloorKit';fit=json.loads((source/'floor-fit.json').read_text());baseline=json.loads((root/'Art/Source/Aurelion/AtriumCanopyKit/canopy-baseline.json').read_text())
floor=baseline['floor'];c=next(c for c in by_label[floor['actor']].get_components_by_class(unreal.InstancedStaticMeshComponent) if c.get_name()==floor['component'])
assert [c.get_instance_transform(i,world_space=True).export_text() for i in range(c.get_instance_count())]==[r['transform'] for r in floor['instances']]
destination='/Game/Aurelion/Environment/ArchitectureKit';materials={key:destination+'/Materials/M_AurelionKit_'+value for key,value in {'M_Aurelion_IvoryStone':'PavingIvory','M_Aurelion_StoneGrout':'StoneGrout','M_Aurelion_Basalt':'PavingBasalt','M_Aurelion_AncientGold':'Gold'}.items()}
unreal.SystemLibrary.execute_console_command(world,'Interchange.FeatureFlags.Import.FBX 0')
spec=json.loads((source/'manifest.json').read_text())['modules'][0];mesh=helpers['import_owned_mesh'](spec,source,destination+'/Meshes',materials)
c.modify()
for row in sorted(fit['selected'],key=lambda r:r['index'],reverse=True):assert c.remove_instance(row['index'])
for angle in (0,90,180,270):
 r=math.radians(angle);a=subsystem.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(2300*math.cos(r),2300*math.sin(r),0),unreal.Rotator(yaw=angle));a.set_actor_label(f'KIT_Atrium_BridgeFloor_{angle}');a.set_folder_path('Aurelion/CustomArchitecture/Z05/BridgeFloors')
 nc=a.static_mesh_component;nc.set_static_mesh(mesh);nc.set_collision_profile_name('NoCollision');nc.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION);a.set_actor_enable_collision(False)
 old=by_label[f'Z05_Bridge_{angle}'].static_mesh_component;old.modify();old.set_visibility(False,False);old.set_hidden_in_game(True,False)
assert helpers['snapshot_actor_state'](original)==before
geometry=runpy.run_path(str(root/'Scripts/Editor/check_atrium_bridge_floors.py'))['check_bridge_floors'](world,list(subsystem.get_all_level_actors()))
if persist:
 shutil.copy2(root/'Content/Aurelion/Maps/L_Aurelion_M12.umap',out/'L_Aurelion_M12-before.umap');assert editor.save_current_level()
(out/'atrium-bridge-floor-fit.json').write_text(json.dumps(dict(status='saved' if persist else 'unsaved_preview',geometry=geometry),indent=2))
editor.editor_set_game_view(True)
capture=(root/'Scripts/Editor/fit_z01_lower_architecture.py').read_text(encoding='utf-8-sig').split("p=by_label['Z01_Entry_StandIn']",1)[1]
capture=capture.replace("('entry',unreal.Vector(p.x,p.y,p.z+165),unreal.Rotator(yaw=90),90)","('bridge-floor-entry',unreal.Vector(-3100,-100,175),unreal.Rotator(pitch=-12,yaw=0),90)").replace("('west-wall',unreal.Vector(-7600,-14900,165),unreal.Rotator(pitch=12,yaw=180),75)","('bridge-floor-overview',unreal.Vector(-4000,-2600,1650),unreal.Rotator(pitch=-25,yaw=35),80)").replace('z01-','atrium-')
exec(compile("p=by_label['Z02_Entry_StandIn']"+capture,'atrium_bridge_floor_capture','exec'))
