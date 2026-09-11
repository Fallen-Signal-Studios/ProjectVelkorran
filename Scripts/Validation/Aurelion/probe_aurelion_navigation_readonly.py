"""Inspect current reopened editor and/or PIE navigation. No loads, saves, builds, or input."""
import json
import os
from pathlib import Path
import time
import unreal


def inspect(world):
    result = unreal.SovAurelionNavigationProfileLibrary.inspect_navigation(world)
    fields = ('configured', 'registered', 'dynamic', 'error', 'config_class', 'config_outer',
              'system_class', 'agent_radius', 'agent_height', 'registered_nav_data')
    row = {name: result.get_editor_property(name) for name in fields}
    row['registered_nav_data'] = list(row['registered_nav_data'])
    row['world'] = world.get_path_name()
    row['building'] = unreal.SovAurelionNavigationLibrary.is_navigation_being_built_or_locked(world)
    row['actors'] = []
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.RecastNavMesh):
        row['actors'].append(dict(path=actor.get_path_name(), cls=actor.get_class().get_path_name(),
            radius=actor.get_editor_property('agent_radius'), height=actor.get_editor_property('agent_height'),
            generation=str(actor.get_editor_property('runtime_generation'))))
    row['passed'] = row['configured'] and row['registered'] and row['dynamic'] and not row['building']
    # The first closed journal gate is deliberately not used as a path success criterion.
    start, end = ((-7000., -20800., 0.), (-7000., -19400., 0.)) if '_M12' in world.get_path_name() else ((0., 33300., -1800.), (0., 34000., -1800.))
    pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
    path = unreal.SovAurelionNavigationLibrary.find_path_to_location_synchronously(world,
        unreal.Vector(*start), unreal.Vector(*end), pawn, None)
    row['entry_path'] = dict(complete=path is not None and path.is_valid() and not path.is_partial(),
        points=[[p.x,p.y,p.z] for p in path.path_points] if path else [])
    row['passed'] = row['passed'] and row['entry_path']['complete']
    return row


def run(output_directory=None):
    out = Path(output_directory or os.environ['SOV_AURELION_RUN_DIRECTORY'])
    out.mkdir(parents=True, exist_ok=True)
    subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    worlds = [('editor', subsystem.get_editor_world()), ('pie', subsystem.get_game_world())]
    rows = {name: inspect(world) for name, world in worlds if world is not None and '/Aurelion/Maps/' in world.get_path_name()}
    # The editor world's nav is intentionally locked while PIE is active. Qualify the active world;
    # require a separate stopped/reopened editor invocation before starting the PIE invocation.
    active = 'pie' if 'pie' in rows else 'editor'
    report = dict(scope='Current-world native navigation registration and bounded entry path; no route/cinematic completion proof',
                  mutation=False, worlds=rows, active_world=active,
                  passed=active in rows and rows[active]['passed'])
    target = out / ('navigation-readonly-'+str(time.time_ns())+'.json')
    target.write_text(json.dumps(report,indent=2,default=str),encoding='utf8')
    unreal.log('Aurelion navigation inspection: '+str(target)+' passed='+str(report['passed']))
    return report


if __name__ == '__main__':
    run()
