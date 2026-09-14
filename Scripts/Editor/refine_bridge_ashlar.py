"""Reimport owned bridge spandrels with recessed warm grout and fewer fine marks."""
import json,os,runpy,time,shutil
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world();assert world.get_name()=='L_Aurelion_M12'
subsystem=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);actors=list(subsystem.get_all_level_actors());by_label={a.get_actor_label():a for a in actors};assert len(actors)==2108
helpers=runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'));before=helpers['snapshot_actor_state'](actors)
destination='/Game/Aurelion/Environment/ArchitectureKit'
materials={key:destination+'/Materials/M_AurelionKit_'+value for key,value in {'M_Aurelion_IvoryStone':'PavingIvory','M_Aurelion_AncientGold':'Gold','M_Aurelion_ChannelShadow':'Reveal','M_Aurelion_StoneGrout':'StoneGrout'}.items()}
unreal.SystemLibrary.execute_console_command(world,'Interchange.FeatureFlags.Import.FBX 0')
changed=[]
for kit in ('ApproachBridgeKit','NorthBridgeKit'):
    source=root/'Art/Source/Aurelion'/kit
    for spec in json.loads((source/'manifest.json').read_text())['modules']:
        if not spec['asset'].endswith('ArchSupports'):continue
        mesh=helpers['import_owned_mesh'](spec,source/spec.get('source_subdir',''),destination+'/Meshes',materials);changed.append(mesh.get_path_name())
assert len(changed)==3 and helpers['snapshot_actor_state'](actors)==before
checks=runpy.run_path(str(root/'Scripts/Editor/check_z02_bridges.py'))
checks['check_bridges'](world,actors)
checks['check_bridges'](world,actors,'NorthBridgeKit','KIT_North_Bridge_')
runpy.run_path(str(root/'Scripts/Editor/check_north_bridge_seams.py'))['check_seams'](world,actors)
map_saved=False
if unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages():
    shutil.copy2(root/'Content/Aurelion/Maps/L_Aurelion_M12.umap',out/'L_Aurelion_M12-before.umap');assert editor.save_current_level();map_saved=True
(out/'bridge-ashlar-refinement.json').write_text(json.dumps(dict(status='assets_saved',map_saved=map_saved,assets=changed,actor_count=len(actors),qualification='Owned mesh/material refinement; full visual and gameplay acceptance pending.'),indent=2))
(out/'rendering-settings.json').write_text(json.dumps({key:unreal.SystemLibrary.get_console_variable_float_value(key) for key in ('r.ScreenPercentage','r.SecondaryScreenPercentage.GameViewport','r.AntiAliasingMethod','r.TemporalAA.Upsampling','sg.AntiAliasingQuality','r.TSR.History.ScreenPercentage','r.HighResScreenshotDelay')},indent=2))
editor.editor_set_game_view(True)
capture=(root/'Scripts/Editor/fit_z01_lower_architecture.py').read_text(encoding='utf-8-sig').split("p=by_label['Z01_Entry_StandIn']",1)[1]
capture=capture.replace("('entry',unreal.Vector(p.x,p.y,p.z+165),unreal.Rotator(yaw=90),90)","('ashlar-comparison',unreal.Vector(-2500,-10000,-700),unreal.Rotator(pitch=-14,yaw=132),80)").replace("('west-wall',unreal.Vector(-7600,-14900,165),unreal.Rotator(pitch=12,yaw=180),75)","('ashlar-detail',unreal.Vector(-5600,-4900,-1100),unreal.Rotator(pitch=-5,yaw=180),75)").replace('z01-','bridge-')
exec(compile("p=by_label['Z02_Entry_StandIn']"+capture,'masonry_capture','exec'))
