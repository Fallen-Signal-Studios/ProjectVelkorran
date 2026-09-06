"""Face each copied development map's PlayerStart toward its placed hound pack.
Preserves the original CombatGreybox and every other actor transform.
"""
import hashlib
import json
import math
import os
from pathlib import Path
import unreal

WORK = Path(os.environ.get('VELKORRAN_SETUP_OUTPUT', str(Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())) / 'Validation/WorkPCSetup')))
WORK.mkdir(parents=True, exist_ok=True)
project = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
source = project / 'Content/Maps/CombatGreybox.umap'
editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
if editor.is_in_play_in_editor():
    raise RuntimeError('End PIE before configuring the map entry')
report = {'source_sha256_before': hashlib.sha256(source.read_bytes()).hexdigest(), 'maps': [], 'saved': []}
try:
    for hero in ('Tarrik', 'Selene'):
        path = '/Game/Maps/Development/L_' + hero + 'Combat'
        if not editor.load_level(path):
            raise RuntimeError('Could not open the project development map: ' + path)
        level_actors = actors.get_all_level_actors()
        starts = [a for a in level_actors if isinstance(a, unreal.PlayerStart)]
        coordinators = [a for a in level_actors if isinstance(a, unreal.SovDominionPackCoordinator)]
        if len(coordinators) != 1 or not coordinators[0].has_valid_spawner_configuration():
            raise RuntimeError('Expected one valid authored Dominion pack coordinator')
        coordinator = coordinators[0]
        enemies = list(coordinator.get_editor_property('hound_spawners')) + [coordinator.get_editor_property('handler_spawner')]
        if len(starts) != 1 or len(enemies) != 4:
            report['observed_actors'] = [{'name': a.get_name(), 'label': a.get_actor_label(), 'class': a.get_class().get_path_name(), 'location': a.get_actor_location().export_text()} for a in level_actors]
            raise RuntimeError('Expected one PlayerStart and the authored four-member Dominion pack')
        start = starts[0]
        origin = start.get_actor_location()
        center_x = sum(a.get_actor_location().x for a in enemies) / len(enemies)
        center_y = sum(a.get_actor_location().y for a in enemies) / len(enemies)
        yaw = math.degrees(math.atan2(center_y - origin.y, center_x - origin.x))
        old = start.get_actor_rotation()
        snapshots = {a.get_path_name(): a.get_actor_transform().export_text() for a in level_actors if a != start}
        start.modify()
        if not start.set_actor_rotation(unreal.Rotator(pitch=old.pitch, yaw=yaw, roll=old.roll), True):
            raise RuntimeError('Could not orient the PlayerStart')
        after = start.get_actor_rotation()
        if abs(after.yaw - yaw) > 0.01 or start.get_actor_location() != origin:
            raise RuntimeError('PlayerStart orientation/location verification failed')
        if snapshots != {a.get_path_name(): a.get_actor_transform().export_text() for a in level_actors if a != start}:
            raise RuntimeError('An unrelated actor transform changed')
        if not editor.save_current_level():
            raise RuntimeError('Could not save ' + path)
        report['maps'].append({'map': path, 'old_rotation': old.export_text(), 'new_rotation': after.export_text(),
                               'target_pack_center_xy': [center_x, center_y], 'other_actor_transforms_unchanged': True})
        report['saved'].append(path)
    report['status'] = 'saved; both starts face their existing Dominion pack'
except Exception as exc:
    report['error'] = str(exc)
    raise
finally:
    report['source_sha256_after'] = hashlib.sha256(source.read_bytes()).hexdigest()
    report['source_map_unchanged'] = report['source_sha256_before'] == report['source_sha256_after']
    (WORK / 'combat-map-entry-setup.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    unreal.log('VELKORRAN_COMBAT_MAP_ENTRY ' + str(WORK / 'combat-map-entry-setup.json'))
    if not report['source_map_unchanged']:
        raise RuntimeError('Original CombatGreybox changed')
