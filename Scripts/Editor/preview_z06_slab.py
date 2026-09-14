"""Fit fallen masonry to the retained combat obstacle without changing encounter actors."""
import json,os,runpy,shutil,time
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY']);persist=bool(globals().get('PERSIST',False))
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world();assert world.get_name()=='L_Aurelion_M12'
subsystem=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);original=list(subsystem.get_all_level_actors());by_label={a.get_actor_label():a for a in original};assert len(original)==2831
source=root/'Art/Source/Aurelion/Z06FallenSlabKit';fit=json.loads((source/'slab-baseline.json').read_text());helpers=runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'));before=helpers['snapshot_actor_state'](original)
row=next(r for r in fit['art'] if r['count']==48);c=by_label[row['actor']].get_component_by_class(unreal.InstancedStaticMeshComponent)
assert [c.get_instance_transform(i,world_space=True).export_text() for i in range(c.get_instance_count())]==row['all_transforms']
destination='/Game/Aurelion/Environment/ArchitectureKit';materials={key:destination+'/Materials/M_AurelionKit_'+value for key,value in {'M_Aurelion_IvoryStone':'PavingIvory','M_Aurelion_ChannelShadow':'Reveal','M_Aurelion_AncientGold':'Gold'}.items()}
unreal.SystemLibrary.execute_console_command(world,'Interchange.FeatureFlags.Import.FBX 0')
mesh=helpers['import_owned_mesh'](json.loads((source/'manifest.json').read_text())['modules'][0],source,destination+'/Meshes',materials)
c.modify()
for r in sorted(row['selected'],key=lambda r:r['index'],reverse=True):assert c.remove_instance(r['index'])
old=by_label['Z06_Fallen_Plate'];a=subsystem.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(0,8600,-600),old.get_actor_rotation());a.set_actor_label('KIT_Z06_FallenMasonry');a.set_folder_path('Aurelion/CustomArchitecture/Z06/FallenMasonry')
c=a.static_mesh_component;c.set_static_mesh(mesh);c.set_collision_profile_name('NoCollision');c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION);a.set_actor_enable_collision(False)
assert helpers['snapshot_actor_state'](original)==before
geometry=runpy.run_path(str(root/'Scripts/Editor/check_z06_slab.py'))['check_z06_slab'](world,list(subsystem.get_all_level_actors()))
if persist:
    shutil.copy2(root/'Content/Aurelion/Maps/L_Aurelion_M12.umap',out/'L_Aurelion_M12-before.umap');assert editor.save_current_level()
(out/'z06-slab-fit.json').write_text(json.dumps(dict(status='saved' if persist else 'unsaved_preview',geometry=geometry),indent=2))
editor.editor_set_game_view(True)
capture=(root/'Scripts/Editor/fit_z01_lower_architecture.py').read_text(encoding='utf-8-sig').split("p=by_label['Z01_Entry_StandIn']",1)[1]
capture=capture.replace("('entry',unreal.Vector(p.x,p.y,p.z+165),unreal.Rotator(yaw=90),90)","('slab-front',unreal.Vector(-750,7550,-300),unreal.Rotator(pitch=-5,yaw=55),90)").replace("('west-wall',unreal.Vector(-7600,-14900,165),unreal.Rotator(pitch=12,yaw=180),75)","('slab-top',unreal.Vector(200,9700,-40),unreal.Rotator(pitch=-30,yaw=-100),90)").replace('z01-','z06-')
exec(compile("p=by_label['Z06_Entry_StandIn']"+capture,'slab_fit_capture','exec'))
