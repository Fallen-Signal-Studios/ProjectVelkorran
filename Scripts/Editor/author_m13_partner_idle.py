"""Author stationary Tarrik dialogue performances in three existing sequences."""
import hashlib,json,os,sys
from pathlib import Path
import unreal
sys.path.insert(0,str(Path(__file__).resolve().parent))
import cinematic_content as cinematic
project=Path(unreal.Paths.project_dir())
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor()
map_paths=[project/'Content/Aurelion/Maps'/name for name in ('L_Aurelion_M12.umap','L_Aurelion_M13.umap')]
before={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in map_paths}
rows=[]
for beat in ('MeridianContainment','FifthWitness','GrammarPropagation'):
    sequence=unreal.load_asset('/Game/Aurelion/Cinematics/LS_'+beat)
    binding=next(b for b in sequence.get_bindings() if b.get_name()=='Partner')
    existing=list(binding.get_tracks())
    assert len(existing)==1 and isinstance(existing[0],unreal.MovieScene3DTransformTrack),'Unexpected existing performance'
    transform=existing[0].get_path_name()
    cinematic._stationary_partner_performance(binding,beat,sequence.get_playback_end())
    tracks=[t for t in binding.get_tracks() if isinstance(t,unreal.MovieSceneSkeletalAnimationTrack)]
    assert len(tracks)==1
    section=tracks[0].get_sections()[0]
    animation=section.get_editor_property('params').get_editor_property('animation')
    mesh=unreal.load_asset('/NarrativePro/Pro/Core/Character/Biped/Art/Mannequin/Meshes/SKM_Quinn')
    assert animation.get_editor_property('skeleton')==mesh.get_editor_property('skeleton')
    assert animation.get_editor_property('sequence_length')>1.,'Require animated idle, not a static pose'
    assert existing[0].get_path_name()==transform and existing[0] in binding.get_tracks()
    assert unreal.EditorAssetLibrary.save_loaded_asset(sequence)
    rows.append(dict(beat=beat,animation=animation.get_path_name(),seconds=animation.get_editor_property('sequence_length'),
        completion=str(section.get_completion_mode()),force_custom_mode=section.get_editor_property('params').get_editor_property('force_custom_mode')))
assert before=={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in map_paths}
(out/'partner-idle.json').write_text(json.dumps(dict(status='saved_requires_playback_review',sequences=rows,maps_unchanged=before),indent=2))
