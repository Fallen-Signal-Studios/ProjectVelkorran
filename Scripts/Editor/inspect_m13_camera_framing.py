"""Read authored M13 camera/participant tracks and inspect the Grammar shot."""
import json
from pathlib import Path
import unreal

level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not level.is_in_play_in_editor()
assert level.load_level('/Game/Aurelion/Maps/L_Aurelion_M13')
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
rows=[]
for asset in unreal.EditorAssetLibrary.list_assets('/Game/Aurelion/Cinematics',False):
    if not asset.rsplit('/',1)[-1].startswith('LS_'): continue
    sequence=unreal.load_asset(asset)
    if not isinstance(sequence,unreal.LevelSequence): continue
    if not any(sequence.get_name() in a.get_actor_label() for a in actors if isinstance(a,unreal.CineCameraActor)): continue
    bindings=[]
    for binding in sequence.get_bindings():
        tracks=[]
        for track in binding.get_tracks():
            if not isinstance(track,unreal.MovieScene3DTransformTrack): continue
            for section in track.get_sections():
                channels=section.get_channels_by_type(unreal.MovieSceneScriptingDoubleChannel)
                tracks.append([[k.get_value() for k in channel.get_keys()] for channel in channels])
        bindings.append(dict(name=binding.get_name(),transforms=tracks))
    rows.append(dict(sequence=asset,bindings=bindings))
cameras=[dict(label=a.get_actor_label(),location=a.get_actor_location().export_text(),
    rotation=a.get_actor_rotation().export_text()) for a in actors if isinstance(a,unreal.CineCameraActor)]
out=Path(__file__).resolve().parents[2]/'Saved/Validation/Aurelion/M13Framing-20260913'
out.mkdir(parents=True,exist_ok=True)
(out/'before.json').write_text(json.dumps(dict(sequences=rows,cameras=cameras),indent=2),encoding='utf8')
camera=next(a for a in actors if isinstance(a,unreal.CineCameraActor) and 'LS_GrammarPropagation_CameraClose' in a.get_actor_label())
unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).set_level_viewport_camera_info(camera.get_actor_location(),camera.get_actor_rotation())
unreal.log('M13_FRAMING_INSPECTED '+str(out))
