"""Read-only runtime priority census; import before starting the route."""
import json
import os
from pathlib import Path
import unreal

output = Path(os.environ['SOV_AURELION_RUN_DIRECTORY']) / 'runtime-loot-priority.json'
report = dict(read_only=True, actors={}, unexpected=[])
enemy_classes = {'BP_Aurelion' + role + '_C' for role in
                 ('Enforcer', 'Linkbound', 'SecurityDrone', 'ContaminatedDrone',
                  'WallRunner', 'Weaver', 'Elite')}

def tick(delta):
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    if not world:
        return
    changed = False
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.NarrativeNPCCharacter):
        if actor.get_class().get_name() not in enemy_classes:
            continue
        key = actor.get_path_name()
        if key in report['actors']:
            continue
        components = [c for c in actor.get_components_by_class(unreal.NarrativeInteractableComponent)
                      if c.get_name() == 'NPCInteractable']
        if not components:
            continue
        priority = components[0].get_editor_property('interaction_priority')
        report['actors'][key] = priority
        if priority != -2:
            report['unexpected'].append(key)
        changed = True
    if changed:
        output.write_text(json.dumps(report, indent=2))

handle = unreal.register_slate_post_tick_callback(tick)
