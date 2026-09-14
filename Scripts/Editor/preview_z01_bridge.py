"""Measured bridge replacement preview; explicit save entry point persists it."""
import json
import os
from pathlib import Path
import runpy
import shutil
import time
import unreal
persist=bool(globals().get('PERSIST',False))
root=Path(unreal.Paths.project_dir()).resolve(); out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
helpers=runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'))
snapshot=helpers['snapshot_actor_state']; import_mesh=helpers['import_owned_mesh']
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world(); assert world.get_name()=='L_Aurelion_M12'
subsystem=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
original=list(subsystem.get_all_level_actors()); by_label={a.get_actor_label():a for a in original}
assert len(original)==1914 and 'KIT_Z01_StoneBridge' not in by_label
before=snapshot(original)
source=root/'Art/Source/Aurelion/BridgeKit'; destination='/Game/Aurelion/Environment/ArchitectureKit'
spec=json.loads((source/'manifest.json').read_text())['modules'][0]
materials={key:destination+'/Materials/M_AurelionKit_'+value for key,value in {'M_Aurelion_IvoryStone':'PavingIvory','M_Aurelion_ChannelShadow':'Reveal','M_Aurelion_AncientGold':'Gold'}.items()}
mesh=import_mesh(spec,source,destination+'/Meshes',materials)
bridge=subsystem.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(*spec['world_origin_cm']))
bridge.set_actor_label('KIT_Z01_StoneBridge'); bridge.set_folder_path('Aurelion/CustomArchitecture/Z01/Bridge')
bridge.static_mesh_component.set_static_mesh(mesh); bridge.static_mesh_component.set_collision_profile_name('BlockAll')
bridge.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.QUERY_AND_PHYSICS); bridge.set_actor_enable_collision(True)
old=by_label['ramp']; c=old.static_mesh_component
assert c.static_mesh.get_path_name()=='/Game/Aurelion/Art/Props/ramp.ramp'
c.modify(); c.set_visibility(False,False); c.set_hidden_in_game(True,False); c.set_collision_profile_name('NoCollision')
c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
expected=dict(before); transform,enabled,components=before[old.get_path_name()]
expected[old.get_path_name()]=(transform,enabled,[(identity,str(unreal.CollisionEnabled.NO_COLLISION) if identity==c.get_path_name() else value) for identity,value in components])
after=snapshot(original); changes={k:dict(before=expected[k],after=after[k]) for k in expected if expected[k]!=after[k]}
(out/'preservation-differences.json').write_text(json.dumps(changes,indent=2)); assert not changes
checks=runpy.run_path(str(root/'Scripts/Editor/check_z01_bridge_geometry.py'))['check_bridge'](world,bridge,spec,original)
if persist:
    shutil.copy2(root/'Content/Aurelion/Maps/L_Aurelion_M12.umap',out/'L_Aurelion_M12-before.umap'); assert editor.save_current_level()
(out/'bridge-fit.json').write_text(json.dumps(dict(status='saved' if persist else 'unsaved_preview',checks=checks,original_actor_count=len(original),intentional_changes='Replace original ramp visuals/collision with measured deck and 1.1m balustrades',qualification='Isolated authoring queries; live navigation, jumping, combat cover and performance unqualified'),indent=2))
editor.editor_set_game_view(True)
capture=(root/'Scripts/Editor/fit_z01_lower_architecture.py').read_text(encoding='utf-8-sig').split("p=by_label['Z01_Entry_StandIn']",1)[1]
capture=capture.replace("('west-wall',unreal.Vector(-7600,-14900,165),unreal.Rotator(pitch=12,yaw=180),75)","('bridge-deck',unreal.Vector(-6200,-16100,235),unreal.Rotator(pitch=0,yaw=90),80)")
exec(compile("p=by_label['Z01_Entry_StandIn']"+capture,'z01_bridge_capture','exec'))
