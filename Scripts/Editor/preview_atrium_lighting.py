"""Calibrate the four existing bridge lights without moving scene geometry."""
import json,os,time,shutil,runpy
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY']);persist=bool(globals().get('PERSIST',False))
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world();assert world.get_name()=='L_Aurelion_M12'
subsystem=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);actors=list(subsystem.get_all_level_actors());by_label={a.get_actor_label():a for a in actors};assert len(actors)==2480
helpers=runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'));before=helpers['snapshot_actor_state'](actors)
recipe=json.loads((root/'Art/Source/Aurelion/AtriumCanopyKit/lighting-fit.json').read_text())
for row in recipe['lights']:
 c=by_label[row['actor']].get_component_by_class(unreal.RectLightComponent)
 assert c.get_editor_property('intensity_units')==unreal.LightUnits.LUMENS and c.get_editor_property('intensity')==row['before']['lumens'] and c.get_editor_property('attenuation_radius')==row['before']['radius']
 c.modify();c.set_intensity(row['after']['lumens']);c.set_attenuation_radius(row['after']['radius'])
assert helpers['snapshot_actor_state'](actors)==before
geometry=runpy.run_path(str(root/'Scripts/Editor/check_atrium_canopies.py'))['check_canopies'](world,actors)
if persist:
 shutil.copy2(root/'Content/Aurelion/Maps/L_Aurelion_M12.umap',out/'L_Aurelion_M12-before.umap');assert editor.save_current_level()
(out/'atrium-lighting-fit.json').write_text(json.dumps(dict(status='saved' if persist else 'unsaved_preview',lights=recipe['lights'],geometry=geometry),indent=2))
editor.editor_set_game_view(True)
capture=(root/'Scripts/Editor/fit_z01_lower_architecture.py').read_text(encoding='utf-8-sig').split("p=by_label['Z01_Entry_StandIn']",1)[1]
capture=capture.replace("('entry',unreal.Vector(p.x,p.y,p.z+165),unreal.Rotator(yaw=90),90)","('lighting-entry',unreal.Vector(-3100,-100,175),unreal.Rotator(pitch=16,yaw=0),90)").replace("('west-wall',unreal.Vector(-7600,-14900,165),unreal.Rotator(pitch=12,yaw=180),75)","('lighting-overview',unreal.Vector(-5900,-5900,4500),unreal.Rotator(pitch=-32,yaw=45),90)").replace('z01-','atrium-')
exec(compile("p=by_label['Z02_Entry_StandIn']"+capture,'atrium_lighting_capture','exec'))
