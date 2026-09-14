"""Replace the retained wall instance with the fitted climb panel."""
from pathlib import Path
import json,os,runpy,shutil,time
import unreal
root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY']);source=root/'Art/Source/Aurelion/Z06ClimbKit'
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world();assert world.get_name()=='L_Aurelion_M12'
subsystem=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);original=list(subsystem.get_all_level_actors());by_label={a.get_actor_label():a for a in original};assert len(original)==2842
helpers=runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'));before=helpers['snapshot_actor_state'](original)
fit=json.loads((source/'climb-baseline.json').read_text());c=by_label[fit['art']['actor']].get_component_by_class(unreal.InstancedStaticMeshComponent)
assert c.static_mesh.get_path_name()==fit['art']['mesh'] and [c.get_instance_transform(i,world_space=True).export_text() for i in range(c.get_instance_count())]==fit['art']['all_instance_transforms']
transforms=[c.get_instance_transform(i,world_space=True) for i in range(1)]
destination='/Game/Aurelion/Environment/ArchitectureKit';materials={key:destination+'/Materials/M_AurelionKit_'+value for key,value in {'M_Aurelion_IvoryStone':'PavingIvory','M_Aurelion_StoneGrout':'StoneGrout','M_Aurelion_AncientGold':'Gold','M_Aurelion_ChannelShadow':'Reveal'}.items()}
unreal.SystemLibrary.execute_console_command(world,'Interchange.FeatureFlags.Import.FBX 0')
mesh=helpers['import_owned_mesh'](json.loads((source/'manifest.json').read_text())['modules'][0],source,destination+'/Meshes',materials)
c.modify();c.set_static_mesh(mesh);c.set_editor_property('override_materials',[])
t=transforms[0];t.translation=unreal.Vector(-1384,9430,-600);t.scale3d=unreal.Vector(1,1,1)
assert c.update_instance_transform(0,t,True,True,True)
assert helpers['snapshot_actor_state'](original)==before
geometry=runpy.run_path(str(root/'Scripts/Editor/check_z06_climb.py'))['check_z06_climb'](world,original)
persist=bool(globals().get('SAVE_CLIMB_PANEL',False))
if persist:
    shutil.copy2(root/'Content/Aurelion/Maps/L_Aurelion_M12.umap',out/'L_Aurelion_M12-before.umap');assert editor.save_current_level()
(out/'climb-panel-fit.json').write_text(json.dumps(dict(status='saved' if persist else 'unsaved_preview',geometry=geometry),indent=2))
editor.editor_set_game_view(True)
capture=(root/'Scripts/Editor/fit_z01_lower_architecture.py').read_text(encoding='utf-8-sig').split("p=by_label['Z01_Entry_StandIn']",1)[1]
capture=capture.replace("('entry',unreal.Vector(p.x,p.y,p.z+165),unreal.Rotator(yaw=90),90)","('climb-face',unreal.Vector(-850,9150,-340),unreal.Rotator(pitch=-10,yaw=150),65)").replace("('west-wall',unreal.Vector(-7600,-14900,165),unreal.Rotator(pitch=12,yaw=180),75)","('climb-landing',unreal.Vector(-700,9870,-100),unreal.Rotator(pitch=-20,yaw=-155),65)").replace('z01-','z06-')
exec(compile("p=by_label['Z06_Entry_StandIn']"+capture,'climb_panel_capture','exec'),globals())
