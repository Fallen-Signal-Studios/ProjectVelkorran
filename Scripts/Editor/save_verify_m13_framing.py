"""Save only the reviewed M13 map/sequence, reload and verify authored values."""
import json
import math
from pathlib import Path
import runpy
import unreal

root=Path(__file__).resolve().parent
out=root.parents[1]/'Saved/Validation/Aurelion/M13Framing-20260913'
assert json.loads((out/'sight-lines.json').read_text())['status']=='PASS'
assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor()
sequence=unreal.load_asset('/Game/Aurelion/Cinematics/LS_GrammarPropagation')
assert unreal.EditorAssetLibrary.save_loaded_asset(sequence)
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_name()=='L_Aurelion_M13'
assert level.save_current_level()
level.eject_pilot_level_actor()
assert level.load_level('/Game/Aurelion/Maps/L_Aurelion_M13')
runpy.run_path(str(root/'verify_m13_grammar_preview.py'))
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
preview=json.loads((out/'preview.json').read_text())
for shot in preview['cameras']:
    camera=next(a for a in actors if a.get_actor_label()==shot['actor'])
    position=camera.get_actor_location()
    assert math.dist((position.x,position.y,position.z),shot['position'])<.1
    assert abs(camera.get_cine_camera_component().current_focal_length-shot['focal'])<.01
for index,y in enumerate((34100.,35400.,36700.)):
    light=next(a for a in actors if a.get_actor_label()=='ENVL_M13_TerminalConversation_'+str(index+1))
    position=light.get_actor_location()
    assert math.dist((position.x,position.y,position.z),(600.,y,-650.))<.1
    c=light.get_component_by_class(unreal.RectLightComponent)
    for name,value in [('intensity',3000.),('attenuation_radius',2100.),('source_width',1400.),('source_height',1000.)]:
        assert abs(c.get_editor_property(name)-value)<.01
(out/'saved.json').write_text(json.dumps(dict(status='PASS',camera_count=2,light_count=3,
    reloaded_map=True,participant_tracks_unchanged=True,
    pending='Animated scene, all other scenes, GPU cost and broad visual fidelity'),indent=2),encoding='utf8')
level.pilot_level_actor(camera)
unreal.log('M13_FRAMING_SAVED_RELOAD_PASS')
