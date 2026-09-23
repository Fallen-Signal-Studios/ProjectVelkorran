"""Read-only M12 E1 obstacle inventory at a failed ordinary-input combat sightline."""
import hashlib
import json
import os
from pathlib import Path

import unreal

root = Path(unreal.Paths.project_dir())
out = Path(os.environ["SOV_AURELION_RUN_DIRECTORY"])
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not level.is_in_play_in_editor()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_name() == "L_Aurelion_M12"
maps = {name: hashlib.sha256((root / "Content/Aurelion/Maps" / name).read_bytes()).hexdigest()
        for name in ("L_Aurelion_M12.umap", "L_Aurelion_M13.umap")}
impact = unreal.Vector(-7377.188369883592, -16567.80964929264, 112.28870875731414)
rows = []
for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
    label = actor.get_actor_label()
    origin, extent = actor.get_actor_bounds(False)
    nearest_x = max(origin.x - extent.x, min(impact.x, origin.x + extent.x))
    nearest_y = max(origin.y - extent.y, min(impact.y, origin.y + extent.y))
    nearest_z = max(origin.z - extent.z, min(impact.z, origin.z + extent.z))
    distance = (unreal.Vector(nearest_x, nearest_y, nearest_z) - impact).length()
    if actor.get_name() != "StaticMeshActor_1127" and distance > 600:
        continue
    components = []
    for component in actor.get_components_by_class(unreal.StaticMeshComponent):
        mesh = component.static_mesh
        components.append(dict(
            name=component.get_name(), mesh=mesh.get_path_name() if mesh else None,
            collision=str(component.get_collision_enabled()),
            profile=str(component.get_collision_profile_name()),
            visible=component.get_editor_property("visible"),
            hidden_in_game=component.get_editor_property("hidden_in_game"),
            transform=component.get_world_transform().export_text(),
        ))
    rows.append(dict(label=label, name=actor.get_name(), actor_path=actor.get_path_name(),
                     transform=actor.get_actor_transform().export_text(),
                     bounds_origin=origin.export_text(), bounds_extent=extent.export_text(),
                     nearest_distance_cm=round(distance, 2),
                     hidden=actor.get_editor_property("hidden"),
                     collision=actor.get_actor_enable_collision(), components=components))
assert any(row["name"] == "StaticMeshActor_1127" for row in rows)
rows.sort(key=lambda row: row["nearest_distance_cm"])
(out / "e1-cover-audit.json").write_text(json.dumps(dict(
    status="read_only", impact=impact.export_text(), actors=rows,
    map_hashes_before=maps,
), indent=2))
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
assert maps == {name: hashlib.sha256((root / "Content/Aurelion/Maps" / name).read_bytes()).hexdigest()
                for name in maps}
