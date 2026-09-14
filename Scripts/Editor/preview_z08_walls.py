"""Fit detailed masonry to the existing visual-only wall component."""
from pathlib import Path
import json,os,runpy,shutil,time
import unreal
root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world();assert world.get_name()=='L_Aurelion_M12'
subsystem=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);original=list(subsystem.get_all_level_actors());by_label={a.get_actor_label():a for a in original};assert len(original)==3140
source=root/'Art/Source/Aurelion/Z08WallKit';fit=json.loads((source/'wall-fit.json').read_text());helpers=runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'));before=helpers['snapshot_actor_state'](original)
dest='/Game/Aurelion/Environment/ArchitectureKit';materials={k:dest+'/Materials/M_AurelionKit_'+v for k,v in {'M_Aurelion_IvoryStone':'PavingIvory','M_Aurelion_AncientGold':'Gold','M_Aurelion_ChannelShadow':'Reveal'}.items()}
meshes={s['asset']:helpers['import_owned_mesh'](s,source,dest+'/Meshes',materials) for s in json.loads((source/'manifest.json').read_text())['modules']}
row=fit['old'];c=by_label[row['actor']].get_component_by_class(unreal.InstancedStaticMeshComponent)
assert c.static_mesh.get_path_name()==row['mesh'] and [c.get_instance_transform(i,world_space=True).export_text() for i in range(c.get_instance_count())]==row['all_instance_transforms']
assert c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
c.modify();c.clear_instances();c.set_static_mesh(meshes['SM_Aurelion_KIT_Z08WallAssembly']);c.set_editor_property('override_materials',[]);c.add_instance(unreal.Transform(location=unreal.Vector(0,20800,-1200),rotation=unreal.Rotator(),scale=unreal.Vector(1,1,1)),world_space=True)
assert helpers['snapshot_actor_state'](original)==before
geometry=runpy.run_path(str(root/'Scripts/Editor/check_z08_walls.py'))['check_z08_walls'](world,list(subsystem.get_all_level_actors()))
persist=bool(globals().get('PERSIST',False))
if persist:
    shutil.copy2(root/'Content/Aurelion/Maps/L_Aurelion_M12.umap',out/'L_Aurelion_M12-before.umap');assert editor.save_current_level()
(out/'z08-wall-fit.json').write_text(json.dumps(dict(status='saved' if persist else 'unsaved_preview',geometry=geometry,preserved_actor_states=len(original)),indent=2))
if not globals().get('SKIP_WALL_CAPTURE',False):
    editor.editor_set_game_view(True)
    capture=(root/'Scripts/Editor/fit_z01_lower_architecture.py').read_text(encoding='utf-8-sig').split("p=by_label['Z01_Entry_StandIn']",1)[1]
    capture=capture.replace("('entry',unreal.Vector(p.x,p.y,p.z+165),unreal.Rotator(yaw=90),90)","('masonry-entry',unreal.Vector(-500,19800,-950),unreal.Rotator(pitch=8,yaw=75),90)")
    capture=capture.replace("('west-wall',unreal.Vector(-7600,-14900,165),unreal.Rotator(pitch=12,yaw=180),75)","('masonry-detail',unreal.Vector(-2700,20500,-980),unreal.Rotator(pitch=15,yaw=170),85)").replace('z01-','z08-')
    exec(compile("p=by_label['Z08_Entry_StandIn']"+capture,'z08_wall_review','exec'))
