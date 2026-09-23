"""Read-only inventory of M12 request surfaces and player-facing action labels."""
import json
import os
from pathlib import Path

import unreal


out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY']) / 'request-prompts.json'
rows = []
for actor in unreal.EditorLevelLibrary.get_all_level_actors():
    if not isinstance(actor, unreal.SovAurelionRequestActor):
        continue
    component = actor.get_editor_property('interactable')
    rows.append(dict(
        actor=actor.get_path_name(), location=str(actor.get_actor_location()),
        operation=str(actor.get_editor_property('operation')),
        action_text=str(actor.get_editor_property('action_text')),
        name_text=str(component.get_editor_property('interactable_name_text')) if component else None,
    ))
out.write_text(json.dumps(dict(status='passed', rows=rows), indent=2), encoding='utf-8')
