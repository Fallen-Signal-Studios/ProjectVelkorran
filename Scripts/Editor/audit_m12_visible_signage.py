"""Read-only inventory of M12's player-visible world-space text."""
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
map_file = root / "Content/Aurelion/Maps/L_Aurelion_M12.umap"
before = hashlib.sha256(map_file.read_bytes()).hexdigest()
rows = []
for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
    for component in actor.get_components_by_class(unreal.TextRenderComponent):
        rows.append(dict(actor_name=actor.get_name(), actor_label=actor.get_actor_label(),
                         text=str(component.get_editor_property("text")),
                         world_size=component.get_editor_property("world_size"),
                         location=component.get_world_location().export_text(),
                         rotation=component.get_world_rotation().export_text(),
                         color=component.get_editor_property("text_render_color").export_text(),
                         visible=component.get_editor_property("visible"),
                         hidden_in_game=component.get_editor_property("hidden_in_game"),
                         actor_hidden=actor.get_editor_property("hidden"),
                         material=component.get_material(0).get_path_name() if component.get_material(0) else None))
rows.sort(key=lambda row: (row["actor_name"], row["text"]))
(out / "m12-visible-signage.json").write_text(json.dumps(dict(status="read_only", map_sha256=before,
                                                         text_components=rows), indent=2))
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
assert hashlib.sha256(map_file.read_bytes()).hexdigest() == before
