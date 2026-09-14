"""Deploy the fitted capture-gallery wall kit with native collision retained."""
import json,os,runpy,shutil,time
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY']);persist=bool(globals().get('PERSIST',False))
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world();assert world.get_name()=='L_Aurelion_M12'
subsystem=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);original=list(subsystem.get_all_level_actors());by_label={a.get_actor_label():a for a in original}
assert len(original)==2897 and not any(a.get_actor_label().startswith('KIT_Z07_Wall_') for a in original)
source=root/'Art/Source/Aurelion/Z07WallKit';fit=json.loads((source/'wall-fit.json').read_text());helpers=runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'));before=helpers['snapshot_actor_state'](original)
destination='/Game/Aurelion/Environment/ArchitectureKit'
materials={key:destination+'/Materials/M_AurelionKit_'+value for key,value in {'M_Aurelion_IvoryStone':'PavingIvory','M_Aurelion_AncientGold':'Gold','M_Aurelion_ChannelShadow':'Reveal'}.items()}
meshes={s['asset']:helpers['import_owned_mesh'](s,source,destination+'/Meshes',materials) for s in json.loads((source/'manifest.json').read_text())['modules']}
for row in fit['placements']:
    m=meshes['SM_Aurelion_KIT_'+row['suffix']];b=m.get_bounds();coordinate=row['face']+row['sign']*(b.origin.y+b.box_extent.y)
    p=unreal.Vector(coordinate,row['along'],row['z']) if row['axis']=='x' else unreal.Vector(row['along'],coordinate,row['z'])
    a=subsystem.spawn_actor_from_class(unreal.StaticMeshActor,p,unreal.Rotator(yaw=row['yaw']));a.set_actor_label(row['label']);a.set_folder_path('Aurelion/CustomArchitecture/Z07/Walls')
    c=a.static_mesh_component;c.set_static_mesh(m);c.set_collision_profile_name('NoCollision');c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION);a.set_actor_enable_collision(False)
for row in fit['retired']:
    a=by_label[row['actor']];c=a.get_component_by_class(unreal.InstancedStaticMeshComponent)
    assert c.static_mesh.get_path_name()==row['mesh'] and [c.get_instance_transform(i,world_space=True).export_text() for i in range(c.get_instance_count())]==row['all_instance_transforms']
    assert c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
    c.modify();c.set_visibility(False,False);c.set_hidden_in_game(True,False)
assert helpers['snapshot_actor_state'](original)==before
geometry=runpy.run_path(str(root/'Scripts/Editor/check_z07_walls.py'))['check_z07_walls'](world,list(subsystem.get_all_level_actors()))
if persist:
    shutil.copy2(root/'Content/Aurelion/Maps/L_Aurelion_M12.umap',out/'L_Aurelion_M12-before.umap');assert editor.save_current_level()
(out/'z07-walls-fit.json').write_text(json.dumps(dict(status='saved' if persist else 'unsaved_preview',geometry=geometry,preserved_actor_states=len(original)),indent=2))
if not globals().get('SKIP_WALL_CAPTURE',False):
    editor.editor_set_game_view(True)
    capture=(root/'Scripts/Editor/fit_z01_lower_architecture.py').read_text(encoding='utf-8-sig').split("p=by_label['Z01_Entry_StandIn']",1)[1]
    capture=capture.replace("('west-wall',unreal.Vector(-7600,-14900,165),unreal.Rotator(pitch=12,yaw=180),75)","('wall-detail',unreal.Vector(-500,14500,-700),unreal.Rotator(pitch=15,yaw=145),85)").replace('z01-','z07-')
    exec(compile("p=by_label['Z07_Entry_StandIn']"+capture,'z07_wall_capture','exec'))
