"""Save only quarantine camera transform tracks; never save the M12 map."""
import hashlib,json,os,sys
from pathlib import Path
import unreal
sys.path.insert(0,str(Path(__file__).resolve().parent))
import cinematic_content as cinematic
from aurelion_camera_profiles import quarantine_shots

project=Path(unreal.Paths.project_dir())
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
map_file=project/'Content/Aurelion/Maps/L_Aurelion_M12.umap'
map_hash=hashlib.sha256(map_file.read_bytes()).hexdigest()
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not level.is_in_play_in_editor()
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_name()=='L_Aurelion_M12'
sequence=unreal.load_asset('/Game/Aurelion/Cinematics/LS_SurvivorsClearAndQuarantine')
bindings={b.get_name():b for b in sequence.get_bindings()}
def values(binding):
    return [[[k.get_value() for k in c.get_keys()] for c in s.get_channels_by_type(unreal.MovieSceneScriptingDoubleChannel)]
        for t in binding.get_tracks() if isinstance(t,unreal.MovieScene3DTransformTrack) for s in t.get_sections()]
before={n:values(b) for n,b in bindings.items()}
station=tuple(before['Hero'][0][i][0] for i in range(3))
shots=quarantine_shots(station)
rays=[]
for name,key in [('CameraWide','wide'),('CameraClose','close')]:
    position,target,focal=shots[key]
    for drift in (0.,15.,30.):
        for participant in ('Hero','Partner'):
            for height in (-35.,35.,80.):
                point=tuple(before[participant][0][i][0]+(height if i==2 else 0) for i in range(3))
                hit=cinematic._trace_hit(unreal.SystemLibrary.line_trace_single(world,
                    unreal.Vector(position[0]+drift,*position[1:]),unreal.Vector(*point),
                    unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,False,[],unreal.DrawDebugTrace.NONE,True))
                rays.append(dict(camera=name,drift=drift,participant=participant,height=height,
                    blocked=bool(hit and hit.to_tuple()[0]),hit=str(hit.to_tuple()) if hit and hit.to_tuple()[0] else None))
(out/'sight-lines.json').write_text(json.dumps(rays,indent=2))
assert not any(r['blocked'] for r in rays),'Candidate camera occluded; nothing saved'
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
for name,key in [('CameraWide','wide'),('CameraClose','close')]:
    position,target,focal=shots[key]
    camera=next(a for a in actors if isinstance(a,unreal.CineCameraActor) and 'LS_SurvivorsClearAndQuarantine_'+name in a.get_actor_label())
    assert abs(camera.get_cine_camera_component().current_focal_length-focal)<.01,'Focal change would require protected map save'
    binding=bindings[name]
    assert all(isinstance(t,unreal.MovieScene3DTransformTrack) for t in binding.get_tracks())
    cinematic._clear_binding_presentation(binding)
    end=cinematic._add(position,(30.,0.,0.))
    cinematic._transform_track(binding,sequence.get_playback_end(),position,end,
        cinematic._look_at(position,target),cinematic._look_at(end,target))
assert all(before[n]==values(b) for n,b in bindings.items() if not n.startswith('Camera'))
assert unreal.EditorAssetLibrary.save_loaded_asset(sequence)
assert hashlib.sha256(map_file.read_bytes()).hexdigest()==map_hash
(out/'camera-edit.json').write_text(json.dumps(dict(status='saved_requires_playback_review',shots=shots,
    rays_clear=len(rays),participant_tracks_unchanged=True,map_sha256=map_hash),indent=2))
