"""Preview or save Z11's one-table/six-chair custom art, retaining native collision."""
import hashlib
import json
import os
import runpy
import shutil
from pathlib import Path

import unreal

root = Path(unreal.Paths.project_dir())
source = root / "Art/Source/Aurelion/Z11ObservationFurniture"
out = Path(os.environ["SOV_AURELION_RUN_DIRECTORY"])
baseline = json.loads((source / "baseline.json").read_text())
manifest = json.loads((source / "manifest.json").read_text())["modules"]
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not level.is_in_play_in_editor()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_name() == "L_Aurelion_M13"

maps = {
    name: hashlib.sha256((root / "Content/Aurelion/Maps" / name).read_bytes()).hexdigest()
    for name in baseline["map_hashes"]
}
assert maps == baseline["map_hashes"], "Map changed since the measured furniture baseline"
fbx_hashes = {
    spec["asset"]: hashlib.sha256((source / (spec["asset"] + ".fbx")).read_bytes()).hexdigest()
    for spec in manifest
}
save = os.environ.get("SOV_Z11_SAVE") == "1"
if save:
    review = json.loads(Path(os.environ["SOV_Z11_REVIEWED_PREVIEW"]).read_text())
    assert review["status"] == "unsaved_preview"
    assert review["map_hashes_before"] == maps and review["fbx_hashes"] == fbx_hashes

actorsub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors = list(actorsub.get_all_level_actors())
by_label = {actor.get_actor_label(): actor for actor in actors}
vendor_labels = set(baseline["vendor_instances"])
assert len(vendor_labels) == 2
assert all(label in by_label for label in vendor_labels)
assert not any(label.startswith("Aurelion_Custom_Z11_") for label in by_label)
for label, expected in baseline["vendor_instances"].items():
    actor = by_label[label]
    component = actor.get_component_by_class(unreal.InstancedStaticMeshComponent)
    assert component and component.get_instance_count() == 6
    assert component.get_editor_property("visible") and not component.get_editor_property("hidden_in_game")
    assert component.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION
    for row in expected:
        assert component.get_instance_transform(row["index"], world_space=True).export_text() == row["transform"]
        assert component.static_mesh.get_path_name() == row["mesh"]

for row in baseline["native_collision_actors"]:
    actor = by_label[row["label"]]
    component = actor.get_component_by_class(unreal.StaticMeshComponent)
    assert actor.get_actor_transform().export_text() == row["transform"]
    assert actor.get_actor_enable_collision() == row["collision"]
    assert component.static_mesh.get_path_name() == row["components"][0]["mesh"]
    assert str(component.get_collision_enabled()) == row["components"][0]["collision"]
    assert component.get_editor_property("visible") == row["components"][0]["visible"]
    assert component.get_editor_property("hidden_in_game") == row["components"][0]["hidden_in_game"]

helper = runpy.run_path(str(root / "Scripts/Editor/aurelion_architecture_helpers.py"))
other_before = helper["snapshot_actor_state"](
    [actor for actor in actors if actor.get_actor_label() not in vendor_labels]
)
destination = "/Game/Aurelion/Environment/ArchitectureKit"
material_path = destination + "/Materials/M_AurelionKit_ObservationBlackStone"
material_file = root / "Content/Aurelion/Environment/ArchitectureKit/Materials/M_AurelionKit_ObservationBlackStone.uasset"
if save:
    assert material_file.is_file()
    assert hashlib.sha256(material_file.read_bytes()).hexdigest() == review["material_sha256"]
else:
    material = (
        unreal.load_asset(material_path)
        if unreal.EditorAssetLibrary.does_asset_exist(material_path)
        else unreal.EditorAssetLibrary.duplicate_asset(
            destination + "/Materials/M_AurelionKit_PavingBasalt", material_path
        )
    )
    assert isinstance(material, unreal.Material)
    library = unreal.MaterialEditingLibrary
    base = library.get_material_property_input_node(material, unreal.MaterialProperty.MP_BASE_COLOR)
    assert isinstance(base, unreal.MaterialExpressionLinearInterpolate)
    constant = library.get_inputs_for_material_expression(material, base)[0]
    assert isinstance(constant, unreal.MaterialExpressionConstant3Vector)
    constant.set_editor_property("constant", unreal.LinearColor(.009, .011, .015, 1))
    base.set_editor_property("const_alpha", .035)
    roughness = library.get_material_property_input_node(material, unreal.MaterialProperty.MP_ROUGHNESS)
    assert isinstance(roughness, unreal.MaterialExpressionAdd)
    roughness.set_editor_property("const_b", .69)
    library.recompile_material(material)
    assert unreal.EditorAssetLibrary.save_loaded_asset(material)
material_sha256 = hashlib.sha256(material_file.read_bytes()).hexdigest()
materials = {
    "M_Aurelion_BlackStone": material_path,
    "M_Aurelion_AncientGold": destination + "/Materials/M_AurelionKit_Gold",
    "M_Aurelion_ChannelShadow": destination + "/Materials/M_AurelionKit_Reveal",
}
meshes = {}
for spec in manifest:
    asset = destination + "/Meshes/" + spec["asset"]
    mesh = unreal.load_asset(asset) if save else helper["import_owned_mesh"](
        spec, source, destination + "/Meshes", materials
    )
    assert mesh
    meshes[spec["asset"]] = mesh

for label in vendor_labels:
    component = by_label[label].get_component_by_class(unreal.InstancedStaticMeshComponent)
    component.modify()
    component.set_visibility(False, False)
    component.set_hidden_in_game(True, False)
    assert component.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION

placements = [(
    "Aurelion_Custom_Z11_ObservationTable",
    meshes["SM_Aurelion_KIT_Z11ObservationTable"],
    (0, 43100, 0),
    0,
)]
for index, (y, yaw) in enumerate(((42880, 0), (43320, 180))):
    for column, x in enumerate((-250, 0, 250)):
        placements.append((
            "Aurelion_Custom_Z11_Chair_" + str(index * 3 + column + 1),
            meshes["SM_Aurelion_KIT_Z11ObservationChair"],
            (x, y, 0),
            yaw,
        ))

spawned = {}
for label, mesh, point, yaw in placements:
    actor = actorsub.spawn_actor_from_class(
        unreal.StaticMeshActor, unreal.Vector(*point), unreal.Rotator(yaw=yaw)
    )
    assert actor
    actor.set_actor_label(label)
    component = actor.get_component_by_class(unreal.StaticMeshComponent)
    component.set_static_mesh(mesh)
    component.set_collision_profile_name("NoCollision")
    component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    component.set_editor_property("can_ever_affect_navigation", False)
    actor.set_actor_enable_collision(False)
    origin, extent = actor.get_actor_bounds(False)
    unreal.log("Z11_FURNITURE_BOUNDS " + json.dumps(dict(
        label=label, point=point,
        origin=[origin.x, origin.y, origin.z],
        extent=[extent.x, extent.y, extent.z],
    )))
    assert abs(origin.x-point[0]) <= 4 and abs(origin.y-point[1]) <= 10
    assert -.1 <= origin.z-extent.z <= .1
    if label.endswith("ObservationTable"):
        assert extent.x <= 250.1 and extent.y <= 100.1 and origin.z+extent.z <= 80.1
    else:
        assert extent.x <= 39 and extent.y <= 43 and origin.z+extent.z <= 121
    spawned[label] = dict(
        transform=actor.get_actor_transform().export_text(),
        mesh=mesh.get_path_name(),
        collision=str(component.get_collision_enabled()),
        origin=origin.export_text(),
        extent=extent.export_text(),
    )
assert helper["snapshot_actor_state"](
    [actor for actor in actors if actor.get_actor_label() not in vendor_labels]
) == other_before

report = dict(
    status="unsaved_preview",
    map_hashes_before=maps,
    fbx_hashes=fbx_hashes,
    material_sha256=material_sha256,
    source_baseline=str(source / "baseline.json"),
    vendor_instances_preserved=True,
    native_collision_preserved=True,
    other_actor_transforms_and_collision_preserved=True,
    spawned=spawned,
)
if save:
    assert spawned == review["spawned"]
    shutil.copy2(root / "Content/Aurelion/Maps/L_Aurelion_M13.umap",
                 out / "L_Aurelion_M13.before.umap")
    assert level.save_current_level()
    assert level.load_level("/Game/Aurelion/Maps/L_Aurelion_M13")
    restored = {actor.get_actor_label(): actor for actor in actorsub.get_all_level_actors()}
    assert all(label in restored for label in spawned)
    for label, row in spawned.items():
        actor = restored[label]
        component = actor.get_component_by_class(unreal.StaticMeshComponent)
        assert actor.get_actor_transform().export_text() == row["transform"]
        assert component.static_mesh.get_path_name() == row["mesh"]
        assert not actor.get_actor_enable_collision()
        assert str(component.get_collision_enabled()) == row["collision"]
        assert not component.get_editor_property("can_ever_affect_navigation")
    for label, expected in baseline["vendor_instances"].items():
        component = restored[label].get_component_by_class(unreal.InstancedStaticMeshComponent)
        assert not component.get_editor_property("visible")
        assert component.get_editor_property("hidden_in_game")
        assert component.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION
        assert component.get_instance_count() == len(expected)
        for row in expected:
            assert component.get_instance_transform(row["index"], world_space=True).export_text() == row["transform"]
    for row in baseline["native_collision_actors"]:
        actor = restored[row["label"]]
        component = actor.get_component_by_class(unreal.StaticMeshComponent)
        assert actor.get_actor_transform().export_text() == row["transform"]
        assert actor.get_actor_enable_collision() == row["collision"]
        assert str(component.get_collision_enabled()) == row["components"][0]["collision"]
    assert helper["snapshot_actor_state"]([
        actor for actor in restored.values()
        if actor.get_actor_label() not in vendor_labels and actor.get_actor_label() not in spawned
    ]) == other_before
    assert hashlib.sha256((root / "Content/Aurelion/Maps/L_Aurelion_M12.umap").read_bytes()).hexdigest() == maps["L_Aurelion_M12.umap"]
    assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    report["status"] = "saved_reloaded"
    report["m12_unchanged"] = True

(out / "observation-furniture-fit.json").write_text(json.dumps(report, indent=2))
runpy.run_path(str(root / "Scripts/Editor/preview_m13_route.py"), init_globals={
    "ALLOW_DIRTY_PREVIEW": not save,
    "M13_ROUTE_VIEWS": [
        ("z11-furniture-entry", (0, 42500, 175)),
        ("z11-furniture-window", (750, 43100, 190)),
    ],
    "M13_ROUTE_YAWS": {
        "z11-furniture-entry": 90,
        "z11-furniture-window": 180,
    },
    "M13_ROUTE_PITCHES": {
        "z11-furniture-entry": -8,
        "z11-furniture-window": -8,
    },
})
