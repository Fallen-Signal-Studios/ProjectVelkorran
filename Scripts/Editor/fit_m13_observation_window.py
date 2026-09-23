"""Import and preview/save Z11's open custom window surround without touching gameplay collision."""
import hashlib
import json
import os
import runpy
import shutil
from pathlib import Path

import unreal


root = Path(unreal.Paths.project_dir())
out = Path(os.environ["SOV_AURELION_RUN_DIRECTORY"])
source = root / "Art/Source/Aurelion/Z11ObservationWindow"
spec = json.loads((source / "manifest.json").read_text())["modules"][0]
fbx = source / (spec["asset"] + ".fbx")
fbx_hash = hashlib.sha256(fbx.read_bytes()).hexdigest()
map_file = root / "Content/Aurelion/Maps/L_Aurelion_M13.umap"
map_before = hashlib.sha256(map_file.read_bytes()).hexdigest()
assert map_before == "939be896bd7fbe9c062fc11e641a41a987bba9cdde7218369373474c1abfec5d"
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not level.is_in_play_in_editor()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_name() == "L_Aurelion_M13"
save = os.environ.get("SOV_Z11_WINDOW_SAVE") == "1"
if save:
    review = json.loads(Path(os.environ["SOV_Z11_WINDOW_REVIEWED_PREVIEW"]).read_text())
    assert review["status"] == "unsaved_preview"
    assert review["map_before"] == map_before and review["fbx_sha256"] == fbx_hash

destination = "/Game/Aurelion/Environment/ArchitectureKit"
material_paths = {
    "M_Aurelion_BlackStone": destination + "/Materials/M_AurelionKit_ObservationBlackStone",
    "M_Aurelion_IvoryStone": destination + "/Materials/M_AurelionKit_Ivory",
    "M_Aurelion_ChannelShadow": destination + "/Materials/M_AurelionKit_Reveal",
    "M_Aurelion_AncientGold": destination + "/Materials/M_AurelionKit_Gold",
}
helper = runpy.run_path(str(root / "Scripts/Editor/aurelion_architecture_helpers.py"))
asset = destination + "/Meshes/" + spec["asset"]
mesh = unreal.load_asset(asset) if save else helper["import_owned_mesh"](
    spec, source, destination + "/Meshes", material_paths
)
assert isinstance(mesh, unreal.StaticMesh)
if save:
    assert hashlib.sha256((root / ("Content" + asset.removeprefix("/Game") + ".uasset")).read_bytes()).hexdigest() == review["asset_sha256"]

actorsub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors = actorsub.get_all_level_actors()
label = "Aurelion_Custom_Z11_ObservationWindowSurround"
assert not any(actor.get_actor_label() == label for actor in actors)
before_actors = helper["snapshot_actor_state"](actors)
actor = actorsub.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(-1250, 43100, 0), unreal.Rotator())
assert actor
actor.set_actor_label(label)
component = actor.get_component_by_class(unreal.StaticMeshComponent)
component.set_static_mesh(mesh)
component.set_collision_profile_name("NoCollision")
component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
component.set_editor_property("can_ever_affect_navigation", False)
actor.set_actor_enable_collision(False)
assert helper["snapshot_actor_state"](actors) == before_actors
origin, extent = actor.get_actor_bounds(False)
assert -1300 < origin.x - extent.x < -1250
assert -1220 < origin.x + extent.x < -1190
assert 42300 < origin.y - extent.y < 42350
assert 43850 < origin.y + extent.y < 43900
assert -20 < origin.z - extent.z < 0
assert 600 < origin.z + extent.z < 620
assert component.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION
asset_file = root / ("Content" + asset.removeprefix("/Game") + ".uasset")
row = dict(label=label, transform=actor.get_actor_transform().export_text(),
           bounds_origin=origin.export_text(), bounds_extent=extent.export_text(),
           mesh=mesh.get_path_name(), material_slots=[
               component.get_material(i).get_path_name() for i in range(component.get_num_materials())],
           collision=str(component.get_collision_enabled()))
report = dict(status="unsaved_preview", map_before=map_before, fbx_sha256=fbx_hash,
              asset_sha256=hashlib.sha256(asset_file.read_bytes()).hexdigest(), actor=row)
if save:
    assert row == review["actor"]
    shutil.copy2(map_file, out / "L_Aurelion_M13.before.umap")
    assert level.save_current_level()
    assert level.load_level("/Game/Aurelion/Maps/L_Aurelion_M13")
    restored = [a for a in actorsub.get_all_level_actors() if a.get_actor_label() == label]
    assert len(restored) == 1
    part = restored[0].get_component_by_class(unreal.StaticMeshComponent)
    assert restored[0].get_actor_transform().export_text() == row["transform"]
    assert part.static_mesh == mesh and part.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION
    assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    report["status"] = "saved_reloaded"
report["map_after"] = hashlib.sha256(map_file.read_bytes()).hexdigest()
(out / "observation-window-fit.json").write_text(json.dumps(report, indent=2))
runpy.run_path(str(root / "Scripts/Editor/preview_m13_route.py"), init_globals={
    "ALLOW_DIRTY_PREVIEW": not save,
    "M13_ROUTE_VIEWS": [
        ("z11-window-center", (750, 43100, 190)),
        ("z11-window-oblique", (600, 42650, 190)),
    ],
    "M13_ROUTE_YAWS": {"z11-window-center": 180, "z11-window-oblique": 170},
    "M13_ROUTE_PITCHES": {"z11-window-center": -8, "z11-window-oblique": -6},
})
