"""Fit custom vault stonework without changing original room collision or interactions."""
import json
import os
from pathlib import Path
import runpy
import shutil
import time
import unreal
persist=bool(globals().get('PERSIST',False)); root=Path(unreal.Paths.project_dir()).resolve(); out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
helpers=runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'))
checks=runpy.run_path(str(root/'Scripts/Editor/check_z02_vault.py'))
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world(); assert world.get_name()=='L_Aurelion_M12'
subsystem=unreal.get_editor_subsystem(unreal.EditorActorSubsystem); original=list(subsystem.get_all_level_actors()); by_label={a.get_actor_label():a for a in original}
assert len(original)==1974 and not any(a.get_actor_label().startswith('KIT_Z02_Vault_') for a in original)
before=helpers['snapshot_actor_state'](original)
source=root/'Art/Source/Aurelion/Z02VaultKit'; fit=json.loads((source/'measured-profile.json').read_text())
destination='/Game/Aurelion/Environment/ArchitectureKit'
materials={key:destination+'/Materials/M_AurelionKit_'+value for key,value in {'M_Aurelion_IvoryStone':'PavingIvory','M_Aurelion_AncientGold':'Gold','M_Aurelion_ChannelShadow':'Reveal'}.items()}
meshes={s['asset'].removeprefix('SM_Aurelion_KIT_'):helpers['import_owned_mesh'](s,source,destination+'/Meshes',materials) for s in json.loads((source/'manifest.json').read_text())['modules']}
for name,suffix,pos in checks['placements'](meshes,fit):
    a=subsystem.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(*pos)); a.set_actor_label('KIT_Z02_Vault_'+name); a.set_folder_path('Aurelion/CustomArchitecture/Z02/Vault')
    c=a.static_mesh_component; c.set_static_mesh(meshes[suffix]); c.set_collision_profile_name('NoCollision'); c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION); a.set_actor_enable_collision(False)
for label in ('Cube_2','Cube_4','Cube_5'):
    c=by_label[label].static_mesh_component; c.modify(); c.set_visibility(False,False); c.set_hidden_in_game(True,False)
assert helpers['snapshot_actor_state'](original)==before,'Existing transforms/collision changed'
geometry=checks['check_vault'](world,list(subsystem.get_all_level_actors()))
if persist:
    shutil.copy2(root/'Content/Aurelion/Maps/L_Aurelion_M12.umap',out/'L_Aurelion_M12-before.umap'); assert editor.save_current_level()
(out/'z02-vault-fit.json').write_text(json.dumps(dict(status='saved' if persist else 'unsaved_preview',geometry=geometry,original_actor_count=len(original),changes='13 fitted vault actors; old inner shell and central ceiling hidden; original collision and transforms retained'),indent=2))
editor.editor_set_game_view(True)
capture=(root/'Scripts/Editor/fit_z01_lower_architecture.py').read_text(encoding='utf-8-sig').split("p=by_label['Z01_Entry_StandIn']",1)[1]
capture=capture.replace("('west-wall',unreal.Vector(-7600,-14900,165),unreal.Rotator(pitch=12,yaw=180),75)","('vault-detail',unreal.Vector(-6920,-9220,170),unreal.Rotator(pitch=24,yaw=160),85)").replace('z01-','z02-')
exec(compile("p=by_label['Z02_Entry_StandIn']"+capture,'z02_vault_capture','exec'))
