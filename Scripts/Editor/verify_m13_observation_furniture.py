"""Read-only fresh-load verification of the Z11 furniture fit and native collision."""
import hashlib
import json
import os
from pathlib import Path

import unreal

root = Path(unreal.Paths.project_dir())
out = Path(os.environ["SOV_AURELION_RUN_DIRECTORY"])
baseline = json.loads((root / "Art/Source/Aurelion/Z11ObservationFurniture/baseline.json").read_text())
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not level.is_in_play_in_editor()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_name() == "L_Aurelion_M13"
maps = {
    name: hashlib.sha256((root / "Content/Aurelion/Maps" / name).read_bytes()).hexdigest()
    for name in baseline["map_hashes"]
}
assert maps["L_Aurelion_M12.umap"] == baseline["map_hashes"]["L_Aurelion_M12.umap"]
actors = {actor.get_actor_label(): actor for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()}
custom = {label for label in actors if label == "Aurelion_Custom_Z11_ObservationTable"
          or label.startswith("Aurelion_Custom_Z11_Chair_")}
expected = {"Aurelion_Custom_Z11_ObservationTable": (
    (0, 43100, 0), "SM_Aurelion_KIT_Z11ObservationTable"
)}
for index, (y, _) in enumerate(((42880, 0), (43320, 180))):
    for column, x in enumerate((-250, 0, 250)):
        expected["Aurelion_Custom_Z11_Chair_" + str(index * 3 + column + 1)] = (
            (x, y, 0), "SM_Aurelion_KIT_Z11ObservationChair"
        )
assert custom == set(expected), "Unexpected Z11 custom actor count or label"

for label, (point, mesh_name) in expected.items():
    actor = actors[label]
    component = actor.get_component_by_class(unreal.StaticMeshComponent)
    position = actor.get_actor_location()
    assert all(abs(a-b) < .1 for a, b in zip(
        (position.x, position.y, position.z), point
    ))
    expected_mesh = "/Game/Aurelion/Environment/ArchitectureKit/Meshes/" + mesh_name + "." + mesh_name
    assert component.static_mesh.get_path_name() == expected_mesh
    assert component.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION
    assert not actor.get_actor_enable_collision()
    assert not component.get_editor_property("can_ever_affect_navigation")
    assert component.get_editor_property("visible")
    assert not component.get_editor_property("hidden_in_game")
    origin, extent = actor.get_actor_bounds(False)
    assert abs(origin.z-extent.z) < .1
    if mesh_name.endswith("Table"):
        assert extent.x <= 250.1 and extent.y <= 100.1 and origin.z+extent.z <= 80.1
    else:
        assert extent.x <= 39 and extent.y <= 43 and origin.z+extent.z <= 121

for label, instances in baseline["vendor_instances"].items():
    component = actors[label].get_component_by_class(unreal.InstancedStaticMeshComponent)
    assert component and component.get_instance_count() == 6
    assert not component.get_editor_property("visible")
    assert component.get_editor_property("hidden_in_game")
    assert component.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION
    for row in instances:
        assert component.get_instance_transform(row["index"], world_space=True).export_text() == row["transform"]
        assert component.static_mesh.get_path_name() == row["mesh"]

for row in baseline["native_collision_actors"]:
    actor = actors[row["label"]]
    component = actor.get_component_by_class(unreal.StaticMeshComponent)
    assert actor.get_actor_transform().export_text() == row["transform"]
    assert actor.get_actor_enable_collision() == row["collision"]
    assert component.static_mesh.get_path_name() == row["components"][0]["mesh"]
    assert str(component.get_collision_enabled()) == row["components"][0]["collision"]
    assert component.get_editor_property("visible") == row["components"][0]["visible"]
    assert component.get_editor_property("hidden_in_game") == row["components"][0]["hidden_in_game"]

assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
assert maps == {
    name: hashlib.sha256((root / "Content/Aurelion/Maps" / name).read_bytes()).hexdigest()
    for name in maps
}
(out / "observation-furniture-verify.json").write_text(json.dumps(dict(
    status="passed",
    map_hashes=maps,
    custom_visuals=len(custom),
    hidden_vendor_instances=sum(len(v) for v in baseline["vendor_instances"].values()),
    unchanged_native_collision_actors=len(baseline["native_collision_actors"]),
    dirty_map_packages=0,
), indent=2))
