"""Read-only inventory of asteroid field actors and their pawn collision in M12."""
import json
import os
from pathlib import Path

import unreal


world = unreal.EditorLevelLibrary.get_editor_world()
assert world and world.get_name() == 'L_Aurelion_M12'
rows = []
for actor in unreal.EditorLevelLibrary.get_all_level_actors():
    if 'AsteroidField_Globular' not in actor.get_class().get_name():
        continue
    origin, extent = actor.get_actor_bounds(False)
    components = []
    for component in actor.get_components_by_class(unreal.InstancedStaticMeshComponent):
        components.append(dict(
            name=component.get_name(),
            instances=component.get_instance_count(),
            profile=str(component.get_collision_profile_name()),
            pawn_response=str(component.get_collision_response_to_channel(
                unreal.CollisionChannel.ECC_PAWN)),
            location=component.get_world_location().export_text(),
        ))
    rows.append(dict(
        path=actor.get_path_name(), label=actor.get_actor_label(),
        class_path=actor.get_class().get_path_name(),
        location=actor.get_actor_location().export_text(),
        bounds_origin=origin.export_text(), bounds_extent=extent.export_text(),
        hidden_in_game=actor.get_editor_property('hidden'),
        collision_enabled=actor.get_actor_enable_collision(),
        components=components,
    ))
Path(os.environ['SOV_AURELION_RUN_DIRECTORY'], 'm12-asteroid-fields.json').write_text(
    json.dumps(dict(status='passed', read_only=True, rows=rows), indent=2),
    encoding='utf-8')
