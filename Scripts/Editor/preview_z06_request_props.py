"""Replace three console visuals with authored stone lecterns; preserve request owners."""
from pathlib import Path
import json,os,runpy,shutil,time
import unreal
root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY']);source=root/'Art/Source/Aurelion/RequestConsoleKit'
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world();assert world.get_name()=='L_Aurelion_M12'
subsystem=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);original=list(subsystem.get_all_level_actors());by_label={a.get_actor_label():a for a in original};assert len(original)==2842
helpers=runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'));before=helpers['snapshot_actor_state'](original)
destination='/Game/Aurelion/Environment/ArchitectureKit';materials={key:destination+'/Materials/M_AurelionKit_'+value for key,value in {'M_Aurelion_IvoryStone':'PavingIvory','M_Aurelion_ChannelShadow':'Reveal','M_Aurelion_AncientGold':'Gold','M_Aurelion_UplightLens':'UplightLens'}.items()}
unreal.SystemLibrary.execute_console_command(world,'Interchange.FeatureFlags.Import.FBX 0')
meshes={s['asset']:helpers['import_owned_mesh'](s,source,destination+'/Meshes',materials) for s in json.loads((source/'manifest.json').read_text())['modules']}
for row in json.loads((source/'request-baseline.json').read_text())['actors']:
    a=by_label[row['actor']];v=a.visual;retry='RetryEncounter' in row['actor'];children=[(c,c.get_world_transform()) for c in a.get_components_by_class(unreal.SceneComponent) if c.get_attach_parent()==v]
    v.modify();v.set_static_mesh(meshes['SM_Aurelion_KIT_RequestConsole'+('' if retry else 'RaisedBase')]);v.set_relative_location(unreal.Vector(0,12.068254,0),False,False);v.set_relative_rotation(unreal.Rotator(yaw=180 if retry else 0),False,False);v.set_relative_scale3d(unreal.Vector(1,1,1));v.set_editor_property('override_materials',[])
    for c,t in children:
        assert retry and c.get_name()=='StaticMesh';c.modify();c.set_world_transform(t,False,False);c.set_visibility(False,False);c.set_hidden_in_game(True,False)
assert helpers['snapshot_actor_state'](original)==before
geometry=runpy.run_path(str(root/'Scripts/Editor/check_z06_request_props.py'))['check_z06_request_props'](world,original)
persist=bool(globals().get('SAVE_REQUEST_PROPS',False))
if persist:
    shutil.copy2(root/'Content/Aurelion/Maps/L_Aurelion_M12.umap',out/'L_Aurelion_M12-before.umap');assert editor.save_current_level()
(out/'request-prop-fit.json').write_text(json.dumps(dict(status='saved' if persist else 'unsaved_preview',geometry=geometry),indent=2))
editor.editor_set_game_view(True)
capture=(root/'Scripts/Editor/fit_z01_lower_architecture.py').read_text(encoding='utf-8-sig').split("p=by_label['Z01_Entry_StandIn']",1)[1]
capture=capture.replace("('entry',unreal.Vector(p.x,p.y,p.z+165),unreal.Rotator(yaw=90),90)","('rescue-console',unreal.Vector(620,8160,-410),unreal.Rotator(pitch=-14,yaw=58),65)").replace("('west-wall',unreal.Vector(-7600,-14900,165),unreal.Rotator(pitch=12,yaw=180),75)","('refuge-console',unreal.Vector(830,9020,-300),unreal.Rotator(pitch=-16,yaw=53),65)").replace('z01-','z06-')
capture=capture.replace('unreal.Rotator(pitch=-16,yaw=53),65)]',"unreal.Rotator(pitch=-16,yaw=53),65),('retry-console',unreal.Vector(220,6300,-400),unreal.Rotator(pitch=-20,yaw=90),55)]")
capture=capture.replace('phase<4','phase<6').replace('phase==4 and elapsed>60','phase==6 and elapsed>80')
exec(compile("p=by_label['Z06_Entry_StandIn']"+capture,'request_prop_capture','exec'),globals())
