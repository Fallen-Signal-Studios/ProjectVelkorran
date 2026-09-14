"""Fit visible stone parapets to the existing north connector guards."""
import json
import os
from pathlib import Path
import runpy
import shutil
import time
import unreal
persist=bool(globals().get('PERSIST',False));root=Path(unreal.Paths.project_dir()).resolve();out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
helpers=runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'));checks=runpy.run_path(str(root/'Scripts/Editor/check_north_guards.py'))
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world();assert world.get_name()=='L_Aurelion_M12'
subsystem=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);original=list(subsystem.get_all_level_actors());by_label={a.get_actor_label():a for a in original}
assert len(original)==2074 and not any(a.get_actor_label().startswith('KIT_North_ApproachGuard_') for a in original)
source=root/'Art/Source/Aurelion/ParapetClosingKit'
unaffected=original;before=helpers['snapshot_actor_state'](original)
destination='/Game/Aurelion/Environment/ArchitectureKit'
# Use the legacy FBX factory validated for the owned gallery's authored UCX sets.
unreal.SystemLibrary.execute_console_command(world,'Interchange.FeatureFlags.Import.FBX 0')
materials={key:destination+'/Materials/M_AurelionKit_'+value for key,value in {'M_Aurelion_IvoryStone':'PavingIvory','M_Aurelion_AncientGold':'Gold','M_Aurelion_ChannelShadow':'Reveal'}.items()}
meshes={s['asset'].removeprefix('SM_Aurelion_KIT_'):helpers['import_owned_mesh'](s,source,destination+'/Meshes',materials) for s in json.loads((source/'manifest.json').read_text())['modules']}
meshes['Parapet_4m']=unreal.load_asset(destination+'/Meshes/SM_Aurelion_KIT_Parapet_4m')
for name,pos,yaw in checks['placements']():
    a=subsystem.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(*pos),unreal.Rotator(yaw=yaw));a.set_actor_label('KIT_North_ApproachGuard_'+name);a.set_folder_path('Aurelion/CustomArchitecture/Z02/Approaches')
    c=a.static_mesh_component;c.set_static_mesh(meshes['Parapet_2m' if name.endswith('_14') else 'Parapet_4m']);c.set_collision_profile_name('NoCollision');c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION);a.set_actor_enable_collision(False)
fit=json.loads((source/'north-guard-fit.json').read_text())
rail_actor=by_label[fit['railing_actor']]
rail=next(c for c in rail_actor.get_components_by_class(unreal.InstancedStaticMeshComponent) if c.get_name()==fit['railing_component'])
assert rail.static_mesh.get_path_name()==fit['railing_mesh']
assert rail.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
instance_before=[rail.get_instance_transform(i,world_space=True).export_text() for i in range(rail.get_instance_count())]
indices={item['index'] for item in fit['removed_instances']}
for item in fit['removed_instances']:assert instance_before[item['index']]==item['transform']
expected=sorted(t for i,t in enumerate(instance_before) if i not in indices)
rail.modify()
for i in sorted(indices,reverse=True):assert rail.remove_instance(i)
instance_after=sorted(rail.get_instance_transform(i,world_space=True).export_text() for i in range(rail.get_instance_count()))
assert instance_after==expected, 'Unselected railing instances changed'
(out/'north-railing-retained.json').write_text(json.dumps(dict(before_count=len(instance_before),removed_count=len(indices),after_count=len(instance_after),transforms=instance_after),indent=2))
assert helpers['snapshot_actor_state'](unaffected)==before,'Existing transforms/collision changed'
geometry=checks['check_guards'](world,list(subsystem.get_all_level_actors()))
if persist:
    shutil.copy2(root/'Content/Aurelion/Maps/L_Aurelion_M12.umap',out/'L_Aurelion_M12-before.umap');assert editor.save_current_level()
(out/'north-guard-fit-result.json').write_text(json.dumps(dict(status='saved' if persist else 'unsaved_preview',geometry=geometry,original_actor_count=len(original)),indent=2))
editor.editor_set_game_view(True)
capture=(root/'Scripts/Editor/fit_z01_lower_architecture.py').read_text(encoding='utf-8-sig').split("p=by_label['Z01_Entry_StandIn']",1)[1]
capture=capture.replace("('entry',unreal.Vector(p.x,p.y,p.z+165),unreal.Rotator(yaw=90),90)","('north-connector',unreal.Vector(-7000,-7900,175),unreal.Rotator(pitch=0,yaw=90),90)").replace("('west-wall',unreal.Vector(-7600,-14900,165),unreal.Rotator(pitch=12,yaw=180),75)","('parapet-detail',unreal.Vector(-7100,-5200,150),unreal.Rotator(pitch=-8,yaw=140),75)").replace('z01-','north-')
exec(compile("p=by_label['Z02_Entry_StandIn']"+capture,'z02_guard_capture','exec'))
