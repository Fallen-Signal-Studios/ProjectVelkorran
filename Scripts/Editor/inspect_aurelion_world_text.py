"""Read-only inventory of M12 TextRender components and their visible text."""
import json
import os
from pathlib import Path

import unreal


out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY']) / 'world-text-inventory.json'
rows = []
for actor in unreal.EditorLevelLibrary.get_all_level_actors():
    for component in actor.get_components_by_class(unreal.TextRenderComponent):
        label = component.get_editor_property('text')
        rows.append(dict(
            actor=actor.get_path_name(),
            actor_class=actor.get_class().get_path_name(),
            component=component.get_path_name(),
            text=str(label) if label else '',
            location=str(component.get_world_location()),
            rotation=str(component.get_world_rotation()),
            world_size=component.get_editor_property('world_size'),
            visible=component.get_editor_property('visible'),
            hidden_in_game=component.get_editor_property('hidden_in_game'),
        ))
out.write_text(json.dumps(dict(status='passed', rows=rows), indent=2), encoding='utf-8')
