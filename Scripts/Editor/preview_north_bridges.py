"""Replace remaining north bridge visuals and collision with custom stone decks and arched supports."""
import json
import os
from pathlib import Path
import runpy
import shutil
import time
import unreal
persist=bool(globals().get('PERSIST',False));root=Path(unreal.Paths.project_dir()).resolve();out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
helpers=runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'));checks=runpy.run_path(str(root/'Scripts/Editor/check_z02_bridges.py'))
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world();assert world.get_name()=='L_Aurelion_M12'
subsystem=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);original=list(subsystem.get_all_level_actors());by_label={a.get_actor_label():a for a in original}
assert len(original)==2104 and not any(a.get_actor_label().startswith('KIT_North_Bridge_') for a in original)
source=root/'Art/Source/Aurelion/NorthBridgeKit'
unaffected=[a for a in original if a.get_actor_label() not in ('bridge4','bridge5')];before=helpers['snapshot_actor_state'](unaffected)
route_before=checks['route_controls'](world,original,('bridge4','bridge5'))
destination='/Game/Aurelion/Environment/ArchitectureKit'
# Use the legacy FBX factory validated for the owned gallery's authored UCX sets.
unreal.SystemLibrary.execute_console_command(world,'Interchange.FeatureFlags.Import.FBX 0')
materials={key:destination+'/Materials/M_AurelionKit_'+value for key,value in {'M_Aurelion_IvoryStone':'PavingIvory','M_Aurelion_AncientGold':'Gold','M_Aurelion_ChannelShadow':'Reveal','M_Aurelion_StoneGrout':'StoneGrout'}.items()}
meshes={s['asset'].removeprefix('SM_Aurelion_KIT_'):helpers['import_owned_mesh'](s,source/s['source_subdir'],destination+'/Meshes',materials) for s in json.loads((source/'manifest.json').read_text())['modules']}
for b in json.loads((source/'bridge-fit.json').read_text())['baseline']:
    old_name=b['actor'];old=by_label[old_name]
    for suffix in (b['asset_prefix']+'Deck',b['asset_prefix']+'ArchSupports'):
        a=subsystem.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(*b['placement_cm']),unreal.Rotator());a.set_actor_label('KIT_North_Bridge_'+old_name+'_'+suffix);a.set_folder_path('Aurelion/CustomArchitecture/Z02/Bridges')
        c=a.static_mesh_component;c.set_static_mesh(meshes[suffix]);c.set_collision_profile_name('BlockAll');c.set_collision_enabled(unreal.CollisionEnabled.QUERY_AND_PHYSICS);a.set_actor_enable_collision(True)
    c=old.static_mesh_component;c.modify();c.set_visibility(False,False);c.set_hidden_in_game(True,False);c.set_collision_profile_name('NoCollision');c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION);old.set_actor_enable_collision(False)
assert helpers['snapshot_actor_state'](unaffected)==before,'Existing transforms/collision changed'
geometry=checks['check_bridges'](world,list(subsystem.get_all_level_actors()),'NorthBridgeKit','KIT_North_Bridge_')
route_after=checks['route_controls'](world,list(subsystem.get_all_level_actors()),('bridge4','bridge5'))
for before_route,after_route in zip(route_before,route_after):
    assert before_route['blocker'] is not None or after_route['blocker'] is None,('New centre-route obstruction',before_route,after_route)
geometry.update(route_before=route_before,route_after=route_after)
geometry['seams']=runpy.run_path(str(root/'Scripts/Editor/check_north_bridge_seams.py'))['check_seams'](world,list(subsystem.get_all_level_actors()))
if persist:
    shutil.copy2(root/'Content/Aurelion/Maps/L_Aurelion_M12.umap',out/'L_Aurelion_M12-before.umap');assert editor.save_current_level()
(out/'north-bridge-fit-result.json').write_text(json.dumps(dict(status='saved' if persist else 'unsaved_preview',geometry=geometry,original_actor_count=len(original)),indent=2))
editor.editor_set_game_view(True)
capture=(root/'Scripts/Editor/fit_z01_lower_architecture.py').read_text(encoding='utf-8-sig').split("p=by_label['Z01_Entry_StandIn']",1)[1]
capture=capture.replace("('entry',unreal.Vector(p.x,p.y,p.z+165),unreal.Rotator(yaw=90),90)","('north-crossing',unreal.Vector(-7000,-5900,175),unreal.Rotator(pitch=0,yaw=90),90)").replace("('west-wall',unreal.Vector(-7600,-14900,165),unreal.Rotator(pitch=12,yaw=180),75)","('north-exterior',unreal.Vector(-2500,-10000,-700),unreal.Rotator(pitch=-14,yaw=132),80)").replace('z01-','north-')
exec(compile("p=by_label['Z02_Entry_StandIn']"+capture,'z02_guard_capture','exec'))
