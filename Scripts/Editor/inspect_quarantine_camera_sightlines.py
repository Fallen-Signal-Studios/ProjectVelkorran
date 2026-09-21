"""Read-only shot collision survey; never save the protected M12 map."""
import json, os, sys
from pathlib import Path
import unreal

sys.path.insert(0, str(Path(__file__).resolve().parent))
import cinematic_content as cinematic
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_name() == 'L_Aurelion_M12'
sequence = unreal.load_asset('/Game/Aurelion/Cinematics/LS_SurvivorsClearAndQuarantine')
bindings = {b.get_name(): b for b in sequence.get_bindings()}
def values(binding):
    return [[[k.get_value() for k in c.get_keys()] for c in s.get_channels_by_type(unreal.MovieSceneScriptingDoubleChannel)]
            for t in binding.get_tracks() if isinstance(t, unreal.MovieScene3DTransformTrack) for s in t.get_sections()]
tracks = {n: values(b) for n,b in bindings.items()}
station = tuple(tracks['Hero'][0][i][0] for i in range(3))
rows = []
for offset in [(460,-520,220),(-340,-330,145),(0,-520,200),(-150,-430,140),(210,-400,170),(0,520,180)]:
    eye = cinematic._add(station, offset)
    rays = []
    for drift in (0.,15.,30.):
        for name in ('Hero','Partner'):
            for height in (-35.,35.,80.):
                target = tuple(tracks[name][0][i][0] + (height if i==2 else 0) for i in range(3))
                hit = cinematic._trace_hit(unreal.SystemLibrary.line_trace_single(world, unreal.Vector(eye[0]+drift,*eye[1:]), unreal.Vector(*target), unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [], unreal.DrawDebugTrace.NONE, True))
                rays.append(dict(drift=drift, participant=name, height=height, blocked=bool(hit and hit.to_tuple()[0]), hit=str(hit.to_tuple()) if hit and hit.to_tuple()[0] else None))
    rows.append(dict(offset=offset, position=eye, rays=rays))
out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
(out/'camera-survey.json').write_text(json.dumps(dict(station=station, tracks=tracks, candidates=rows),indent=2))
unreal.log('QUARANTINE_CAMERA_SURVEY_COMPLETE')
