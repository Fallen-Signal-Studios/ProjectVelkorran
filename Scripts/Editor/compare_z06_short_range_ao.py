"""Unsaved screen-space occlusion isolation on the current saved architecture."""
from pathlib import Path
import json,os,time
import unreal
root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world();assert world.get_name()=='L_Aurelion_M12'
subsystem=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);by_label={a.get_actor_label():a for a in subsystem.get_all_level_actors()};assert len(by_label)==2843
rows=[]
for name in ('r.Lumen.ScreenProbeGather.ShortRangeAO','r.Lumen.DiffuseIndirect.SSAO','r.AmbientOcclusionLevels'):
    before=unreal.SystemLibrary.get_console_variable_int_value(name)
    unreal.SystemLibrary.execute_console_command(world,name+' 0');assert unreal.SystemLibrary.get_console_variable_int_value(name)==0
    rows.append(dict(variable=name,before=before,preview=0))
(out/'occlusion-isolation.json').write_text(json.dumps(dict(saved=False,settings=rows),indent=2))
editor.editor_set_game_view(True)
capture=(root/'Scripts/Editor/fit_z01_lower_architecture.py').read_text(encoding='utf-8-sig').split("p=by_label['Z01_Entry_StandIn']",1)[1]
capture=capture.replace("('entry',unreal.Vector(p.x,p.y,p.z+165),unreal.Rotator(yaw=90),90)","('climb-face',unreal.Vector(-850,9150,-340),unreal.Rotator(pitch=-10,yaw=150),65)").replace("('west-wall',unreal.Vector(-7600,-14900,165),unreal.Rotator(pitch=12,yaw=180),75)","('climb-landing',unreal.Vector(-700,9870,-100),unreal.Rotator(pitch=-20,yaw=-155),65)").replace('z01-','z06-')
exec(compile("p=by_label['Z06_Entry_StandIn']"+capture,'occlusion_capture','exec'),globals())
