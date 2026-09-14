"""Verify shot sight lines and unchanged non-camera tracks before saving."""
import json
from pathlib import Path
import unreal
import sys
sys.path.insert(0,str(Path(__file__).resolve().parent))
import cinematic_content as cinematic

level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not level.is_in_play_in_editor()
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_name()=='L_Aurelion_M13'
out=Path(__file__).resolve().parents[2]/'Saved/Validation/Aurelion/M13Framing-20260913'
baseline=json.loads((out/'before.json').read_text(encoding='utf8'))
original=next(s for s in baseline['sequences'] if 'LS_GrammarPropagation.' in s['sequence'])
preview=json.loads((out/'preview.json').read_text(encoding='utf8'))
sequence=unreal.load_asset('/Game/Aurelion/Cinematics/LS_GrammarPropagation')
bindings={b.get_name():b for b in sequence.get_bindings()}
def values(binding):
    return [[ [k.get_value() for k in channel.get_keys()]
        for channel in section.get_channels_by_type(unreal.MovieSceneScriptingDoubleChannel)]
        for track in binding.get_tracks() if isinstance(track,unreal.MovieScene3DTransformTrack)
        for section in track.get_sections()]
for binding in original['bindings']:
    if not binding['name'].startswith('Camera'):
        assert binding['transforms']==values(bindings[binding['name']]),'Participant movement changed'
rays=[]
for shot in preview['cameras']:
    channels=values(bindings[shot['name']])[0]
    for axis in range(3):
        expected=[shot['position'][axis],shot['position'][axis]+(30. if axis==0 else 0.)]
        assert len(channels[axis])==2 and all(abs(a-b)<.01 for a,b in zip(channels[axis],expected)), 'Camera translation keys differ from preview'
    for drift in (0.,15.,30.):
        eye=unreal.Vector(shot['position'][0]+drift,*shot['position'][1:])
        for name in ('Hero','Partner'):
            channels=values(bindings[name])[0]
            for height in (-35.,35.,80.):
                point=unreal.Vector(channels[0][0],channels[1][0],channels[2][0]+height)
                hit=cinematic._trace_hit(unreal.SystemLibrary.line_trace_single(world,eye,point,
                    unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,False,[],unreal.DrawDebugTrace.NONE,True))
                blocked=bool(hit and hit.to_tuple()[0])
                rays.append(dict(camera=shot['name'],drift=drift,participant=name,height=height,blocked=blocked,
                    hit=str(hit.to_tuple()) if blocked else None))
report=dict(status='PASS' if not any(r['blocked'] for r in rays) else 'OCCLUDED',
    rays=rays,participant_tracks_unchanged=True,
    scope='Static Visibility collision rays only; final animated character/rendered scene and non-colliding geometry still require review')
(out/'sight-lines.json').write_text(json.dumps(report,indent=2),encoding='utf8')
assert report['status']=='PASS', 'Camera sight lines remain blocked; inspect report before saving'
unreal.log('GRAMMAR_PREVIEW_SIGHT_LINES_PASS')
