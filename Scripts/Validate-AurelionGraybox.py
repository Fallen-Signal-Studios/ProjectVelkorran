"""Validate the Aurelion graybox inside Project Velkorran.

Run with UnrealEditor-Cmd and the PythonScript commandlet.
"""

import json

import unreal


MAP = "/Game/Aurelion/Maps/L_Aurelion_Graybox_Velkorran"
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)

if not levels.load_level(MAP):
    raise RuntimeError(f"Could not load {MAP}")

world = editor.get_editor_world()
world_settings = world.get_world_settings()
all_actors = actors.get_all_level_actors()
static_mesh_actors = [
    actor for actor in all_actors if isinstance(actor, unreal.StaticMeshActor)
]
zone_folders = sorted(
    {
        str(actor.get_folder_path())
        for actor in all_actors
        if str(actor.get_folder_path()).startswith("Aurelion/Z")
    }
)

registry = unreal.AssetRegistryHelpers.get_asset_registry()
options = unreal.AssetRegistryDependencyOptions(
    include_soft_package_references=True,
    include_hard_package_references=True,
    include_searchable_names=False,
    include_soft_management_references=True,
    include_hard_management_references=True,
)
dependencies = sorted(str(item) for item in registry.get_dependencies(MAP, options))
missing_game_dependencies = [
    package
    for package in dependencies
    if package.startswith("/Game/") and not unreal.EditorAssetLibrary.does_asset_exist(package)
]

report = {
    "map": world.get_path_name(),
    "actors": len(all_actors),
    "static_mesh_actors": len(static_mesh_actors),
    "zone_folders": zone_folders,
    "game_mode_override": str(world_settings.get_editor_property("default_game_mode")),
    "direct_game_dependencies": [
        package for package in dependencies if package.startswith("/Game/")
    ],
    "missing_game_dependencies": missing_game_dependencies,
}

if len(all_actors) < 1000:
    raise RuntimeError(f"Actor count is unexpectedly low: {len(all_actors)}")
if len(zone_folders) < 13:
    raise RuntimeError(f"Expected 13 zone folders, found {len(zone_folders)}")
if world_settings.get_editor_property("default_game_mode") is not None:
    raise RuntimeError("The repository map must inherit Project Velkorran's game mode")
if missing_game_dependencies:
    raise RuntimeError(f"Missing /Game dependencies: {missing_game_dependencies}")

print("AURELION_GRAYBOX_VALID", json.dumps(report, sort_keys=True))
