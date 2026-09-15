"""Read-only stopped-editor census: which authored interactables sit near M12's E3 rescue door.

Loads the M12 map in the editor world and reports every Aurelion request control, world transit
actor and rescue destination with its location, interaction range and priority, plus the distance to
the rescue door and to the position a pilot stood at when a door hold failed. No PIE, no writes.
"""
import json
import math
import os
from pathlib import Path
import unreal

MAP = '/Game/Aurelion/Maps/L_Aurelion_M12'
DOOR_TRANSIT_ID = 'M12_E3_RescueApproach'
# The recorded player position from EastFocusPriorityRoute-20260915-163844-388e3ef1's failed door hold.
HOLD_POSITION = (1129.6412193138638, 8302.449217160673, -509.85000220148885)


def _xyz(vector):
    return [vector.x, vector.y, vector.z]


def _row(actor, door_location):
    interactable = None
    for component in actor.get_components_by_class(unreal.NarrativeInteractableComponent):
        interactable = component
        break
    location = _xyz(actor.get_actor_location())
    row = dict(actor=actor.get_path_name(), cls=actor.get_class().get_name(), location=location,
               distance_to_hold_position=round(math.dist(location, list(HOLD_POSITION)), 1))
    if door_location is not None:
        row['distance_to_door'] = round(math.dist(location, door_location), 1)
    if interactable is not None:
        row.update(interactable=interactable.get_name(),
                   interaction_distance=float(interactable.get_editor_property('interaction_distance')),
                   interaction_priority=int(interactable.get_editor_property('interaction_priority')),
                   interaction_time=float(interactable.get_editor_property('interaction_time')))
    return row


def run(output_directory=None):
    out = Path(output_directory or os.environ['SOV_AURELION_RUN_DIRECTORY'])
    out.mkdir(parents=True, exist_ok=True)
    report = dict(read_only=True, status='failed', map=MAP, hold_position=list(HOLD_POSITION))
    try:
        unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(MAP)
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        assert world is not None and 'L_Aurelion_M12' in world.get_name(), 'The M12 editor world did not load'
        report['world'] = world.get_path_name()
        doors = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SovWorldTransitActor)
                 if str(a.get_editor_property('transit_id')) == DOOR_TRANSIT_ID]
        door_location = _xyz(doors[0].get_actor_location()) if doors else None
        report['door'] = _row(doors[0], door_location) if doors else None
        families = dict(aurelion_requests=unreal.SovAurelionRequestActor,
                        transits=unreal.SovWorldTransitActor,
                        rescue_destinations=unreal.SovRescueDestination)
        for name, cls in families.items():
            rows = [_row(a, door_location) for a in unreal.GameplayStatics.get_all_actors_of_class(world, cls)]
            report[name] = sorted(rows, key=lambda r: r['distance_to_hold_position'])
        # Anything whose own reach covers the pilot's standing position competes for the prompt there.
        competitors = []
        for name in families:
            for row in report[name]:
                reach = row.get('interaction_distance')
                if reach is not None and row['distance_to_hold_position'] <= reach:
                    competitors.append(dict(family=name, **row))
        report['competitors_at_hold_position'] = sorted(competitors, key=lambda r: r['distance_to_hold_position'])
        report['status'] = 'passed: read-only authored census'
    except Exception as exc:
        report['error'] = str(exc)
        raise
    finally:
        (out / 'rescue-door-interactables.json').write_text(json.dumps(report, indent=2, default=str), encoding='utf8')
        unreal.log('AURELION_DOOR_CENSUS ' + str(report['status']) + ' ' + str(out / 'rescue-door-interactables.json'))
    return report


if __name__ == '__main__':
    run()
