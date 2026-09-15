"""Reload the saved prototype without rebuilding its collections or placements."""
from pathlib import Path
import json, os, time, unreal
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
worlds=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
assert editor.load_level('/Game/Aurelion/ArtReview/Chaos/L_Aurelion_ChaosPrototype')
placed=[a for a in actors.get_all_level_actors() if isinstance(a,unreal.GeometryCollectionActor)]
assert len(placed)==2
asset=unreal.load_asset('/Game/Aurelion/ArtReview/Chaos/GC_Aurelion_CargoPrototype')
instance=asset.get_editor_property('dataflow_instance')
assert str(instance.get_editor_property('dataflow_terminal'))=='CargoTerminal'
regenerated=unreal.new_object(unreal.GeometryCollection)
unreal.DataflowBlueprintLibrary.evaluate_terminal_node_by_name(asset.get_dataflow_asset(),'CargoTerminal',regenerated)
probe=unreal.new_object(unreal.GeometryCollectionComponent)
probe.set_rest_collection(regenerated)
assert len(probe.get_initial_local_rest_transforms())==13
(out/'chaos-recipe-verification.json').write_text(json.dumps(dict(status='passed',regenerated_transforms=13,
    terminal='CargoTerminal',qualification='Saved graph regeneration only.'),indent=2))
for actor in placed:
    comp=actor.get_component_by_class(unreal.GeometryCollectionComponent)
    assert comp.get_editor_property('rest_collection').get_path_name()=='/Game/Aurelion/ArtReview/Chaos/GC_Aurelion_CargoPrototype.GC_Aurelion_CargoPrototype'
    assert list(comp.get_editor_property('damage_threshold'))==[500000.0]
    assert len(comp.get_initial_local_rest_transforms())==13
    assert str(comp.get_collision_profile_name())=='Destructible'
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
source=(Path(unreal.Paths.project_dir())/'Scripts/Editor/validate_aurelion_chaos_prototype.py').read_text()
exec(compile(source.split('# Shared simulation begins here;')[1].split('\n',1)[1], 'chaos_saved_simulation','exec'),globals())
