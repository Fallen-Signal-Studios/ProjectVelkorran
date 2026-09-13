"""Preview only the two GrammarPropagation cameras; preserve scene/gameplay tracks."""
import json
from pathlib import Path
import unreal
import sys
sys.path.insert(0,str(Path(__file__).resolve().parent))
import cinematic_content as cinematic
from aurelion_camera_profiles import grammar_propagation_shots

level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not level.is_in_play_in_editor()
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_name()=='L_Aurelion_M13'
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
sequence=unreal.load_asset('/Game/Aurelion/Cinematics/LS_GrammarPropagation')
bindings={b.get_name():b for b in sequence.get_bindings()}
def tracks(binding):
    return [[[[k.get_value() for k in c.get_keys()] for c in s.get_channels_by_type(unreal.MovieSceneScriptingDoubleChannel)]
        for s in t.get_sections()] for t in binding.get_tracks() if isinstance(t,unreal.MovieScene3DTransformTrack)]
before={name:tracks(b) for name,b in bindings.items() if not name.startswith('Camera')}
station=tuple(before['Hero'][0][0][i][0] for i in range(3))
shots=grammar_propagation_shots(station)
rows=[]
with unreal.ScopedEditorTransaction('Clear terminal obstruction from Grammar cameras'):
    sequence.modify()
    for name,key in [('CameraWide','wide'),('CameraClose','close')]:
        camera=next(a for a in actors if isinstance(a,unreal.CineCameraActor) and 'LS_GrammarPropagation_'+name in a.get_actor_label())
        position,target,focal=shots[key]
        camera.modify()
        camera.set_actor_location(unreal.Vector(*position),False,False)
        camera.set_actor_rotation(cinematic._look_at(position,target),False)
        camera.get_cine_camera_component().set_editor_property('current_focal_length',focal)
        binding=bindings[name]
        assert all(isinstance(t,unreal.MovieScene3DTransformTrack) for t in binding.get_tracks())
        cinematic._clear_binding_presentation(binding)
        ending=cinematic._add(position,(30.,0.,0.))
        cinematic._transform_track(binding,sequence.get_playback_end(),position,ending,
            cinematic._look_at(position,target),cinematic._look_at(ending,target))
        rows.append(dict(name=name,position=position,target=target,focal=focal,actor=camera.get_actor_label()))
assert before=={name:tracks(b) for name,b in bindings.items() if not name.startswith('Camera')}
out=Path(__file__).resolve().parents[2]/'Saved/Validation/Aurelion/M13Framing-20260913'
(out/'preview.json').write_text(json.dumps(dict(status='UNSAVED_PREVIEW',cameras=rows,
    participant_tracks_unchanged=True),indent=2),encoding='utf8')
level.pilot_level_actor(camera)
unreal.log('GRAMMAR_CAMERAS_PREVIEW_UNSAVED')
