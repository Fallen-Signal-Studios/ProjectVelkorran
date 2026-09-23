"""Read-only current M13 Z11 inventory and editor views before custom furniture work."""
import hashlib
import json
import os
import runpy
from pathlib import Path

import unreal

root = Path(unreal.Paths.project_dir())
out = Path(os.environ["SOV_AURELION_RUN_DIRECTORY"])
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not level.is_in_play_in_editor()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_name() == "L_Aurelion_M13"

maps = {
    name: hashlib.sha256((root / "Content/Aurelion/Maps" / name).read_bytes()).hexdigest()
    for name in ("L_Aurelion_M12.umap", "L_Aurelion_M13.umap")
}
rows = []
visible_instances = []
for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
    location = actor.get_actor_location()
    if not actor.get_editor_property("hidden"):
        for component in actor.get_components_by_class(unreal.StaticMeshComponent):
            mesh = component.static_mesh
            if not mesh or not component.get_editor_property("visible") or component.get_editor_property("hidden_in_game"):
                continue
            transforms = (
                [(i, component.get_instance_transform(i, world_space=True))
                 for i in range(component.get_instance_count())]
                if isinstance(component, unreal.InstancedStaticMeshComponent)
                else [(None, component.get_world_transform())]
            )
            for index, transform in transforms:
                p = transform.translation
                if -1300 <= p.x <= 1300 and 42200 <= p.y <= 44000 and -100 <= p.z <= 700:
                    visible_instances.append(dict(
                        actor=actor.get_actor_label(),
                        component=component.get_name(),
                        mesh=mesh.get_path_name(),
                        index=index,
                        transform=transform.export_text(),
                        collision=str(component.get_collision_enabled()),
                    ))
    if not (-2000 <= location.x <= 2000 and 40500 <= location.y <= 43500):
        continue
    origin, extent = actor.get_actor_bounds(False)
    components = []
    for component in actor.get_components_by_class(unreal.StaticMeshComponent):
        mesh = component.static_mesh
        if not mesh:
            continue
        instances = (
            [component.get_instance_transform(i, world_space=True).export_text()
             for i in range(component.get_instance_count())]
            if isinstance(component, unreal.InstancedStaticMeshComponent)
            else [component.get_world_transform().export_text()]
        )
        components.append(dict(
            name=component.get_name(),
            mesh=mesh.get_path_name(),
            bounds=mesh.get_bounds().box_extent.export_text(),
            visible=component.get_editor_property("visible"),
            hidden_in_game=component.get_editor_property("hidden_in_game"),
            collision=str(component.get_collision_enabled()),
            instances=instances,
        ))
    rows.append(dict(
        label=actor.get_actor_label(),
        class_name=actor.get_class().get_name(),
        transform=actor.get_actor_transform().export_text(),
        bounds_origin=origin.export_text(),
        bounds_extent=extent.export_text(),
        hidden=actor.get_editor_property("hidden"),
        collision=actor.get_actor_enable_collision(),
        components=components,
    ))
(out / "observation-gallery-audit.json").write_text(json.dumps(dict(
    status="read_only",
    zone="Z11",
    actors=rows,
    visible_instances=visible_instances,
    map_hashes_before=maps,
), indent=2))

if os.environ.get("SOV_Z11_NO_CAPTURE") != "1":
    runpy.run_path(str(root / "Scripts/Editor/preview_m13_route.py"), init_globals={
        "M13_ROUTE_VIEWS": [
            ("z11-entry", (0, 41050, 180)),
            ("z11-center", (250, 42000, 190)),
            ("z11-return", (0, 42900, 190)),
        ],
        "M13_ROUTE_YAWS": {"z11-entry": 90, "z11-center": 90, "z11-return": -90},
        "M13_ROUTE_PITCHES": {"z11-entry": -5, "z11-center": -5, "z11-return": -5},
    })
assert {
    name: hashlib.sha256((root / "Content/Aurelion/Maps" / name).read_bytes()).hexdigest()
    for name in maps
} == maps
