"""Unsaved fixed-camera comparison of M13 berth ship compositions.

This only changes two existing noncolliding scenic shuttle transforms in memory.
The original actor transforms and both map files are restored/verified before
the temporary editor session ends. No placement decision is made by this script.
"""
import hashlib
import json
import os
import time
from pathlib import Path

import unreal

root = Path(unreal.Paths.project_dir())
out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actor_api = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_name() == 'L_Aurelion_M13' and not level.is_in_play_in_editor()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
maps = {name: root / 'Content/Aurelion/Maps' / name
        for name in ('L_Aurelion_M12.umap', 'L_Aurelion_M13.umap')}
before = {name: hashlib.sha256(path.read_bytes()).hexdigest() for name, path in maps.items()}
expected_m13 = '63b3ee34c524df7f474c0f62722bf63279c4a3243e5e48470d394b4d94b3814b'
assert before['L_Aurelion_M13.umap'] == expected_m13, 'M13 changed since reviewed register save'
actors = list(actor_api.get_all_level_actors())
by_label = {actor.get_actor_label(): actor for actor in actors}
ships = {faction: by_label['ART_DepartureShuttle_' + faction]
         for faction in ('Dominion', 'Reformation')}
original = {faction: actor.get_actor_transform() for faction, actor in ships.items()}
original_rotations = {faction: actor.get_actor_rotation() for faction, actor in ships.items()}
for faction, actor in ships.items():
    assert isinstance(actor, unreal.StaticMeshActor)
    assert actor.static_mesh_component.static_mesh.get_name() == 'SM_Aurelion_' + faction + '_Shuttle'
    assert actor.static_mesh_component.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION
    assert not actor.get_actor_enable_collision()
    assert abs(original[faction].scale3d.x - 1) < .0001
    assert abs((original_rotations[faction].yaw % 360) - 180) < .01

# Only within the existing 28 x 18 m scenic dock envelopes. Hulls remain
# non-boardable scenery and native wall/route collision is never touched.
cases = [
    ('baseline', {'Dominion': (-3800, 47500, 180, 1), 'Reformation': (3800, 47500, 180, 1)}),
    ('recessed', {'Dominion': (-4250, 47300, 180, .82), 'Reformation': (4250, 47700, 180, .82)}),
    ('nose_out', {'Dominion': (-4100, 47300, -90, .82), 'Reformation': (4100, 47700, 90, .82)}),
]
views = [('west', (-900, 47200, 180), 180), ('east', (900, 47800, 180), 0)]
rows = [(case_name, poses, view_name, position, yaw)
        for case_name, poses in cases for view_name, position, yaw in views]

def apply_poses(poses):
    for faction, (x, y, yaw, scale) in poses.items():
        actor = ships[faction]
        actor.set_actor_location(unreal.Vector(x, y, 0), False, False)
        actor.set_actor_rotation(unreal.Rotator(yaw=yaw), False)
        actor.set_actor_scale3d(unreal.Vector(scale, scale, scale))

camera = actor_api.spawn_actor_from_class(unreal.CameraActor, unreal.Vector(*views[0][1]), unreal.Rotator(yaw=180))
camera.get_component_by_class(unreal.CameraComponent).set_field_of_view(80)
old_delay = unreal.SystemLibrary.get_console_variable_int_value('r.HighResScreenshotDelay')
unreal.SystemLibrary.execute_console_command(world, 'r.HighResScreenshotDelay 64')
level.editor_set_game_view(True)
level.pilot_level_actor(camera)
state = dict(index=0, stage=0, next=time.monotonic()+12, busy=False, task=None, error=None)
unreal.EditorPythonScripting.set_keep_python_script_alive(True)

def finish(status):
    for faction, actor in ships.items():
        transform = original[faction]
        actor.set_actor_location(transform.translation, False, False)
        actor.set_actor_rotation(original_rotations[faction], False)
        actor.set_actor_scale3d(transform.scale3d)
    level.eject_pilot_level_actor()
    actor_api.destroy_actor(camera)
    unreal.SystemLibrary.execute_console_command(world, 'r.HighResScreenshotDelay ' + str(old_delay))
    after = {name: hashlib.sha256(path.read_bytes()).hexdigest() for name, path in maps.items()}
    restored = all(
        (ships[faction].get_actor_transform().translation-original[faction].translation).length() < .01
        and (ships[faction].get_actor_transform().scale3d-original[faction].scale3d).length() < .0001
        and ships[faction].get_actor_transform().rotation.angular_distance(original[faction].rotation) < .0001
        for faction in ships)
    report = dict(status=status if after == before and restored else 'failed_invariant',
                  map_hashes_before=before, map_hashes_after=after,
                  original_ship_transforms={key: value.export_text() for key, value in original.items()},
                  cases=[dict(case=name, ship_poses=poses) for name, poses in cases],
                  captures=[name + '-' + view + '.png' for name, _, view, _, _ in rows],
                  original_transforms_restored=restored, map_files_unchanged=after == before,
                  error=state['error'])
    (out / 'berth-sightline-preview.json').write_text(json.dumps(report, indent=2))
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)

def tick(_delta):
    if state['busy'] or time.monotonic() < state['next']:
        return
    state['busy'] = True
    try:
        task = state['task']
        if task and not task.is_task_done():
            return
        if state['index'] >= len(rows):
            assert all((out / (name + '-' + view + '.png')).is_file()
                       for name, _, view, _, _ in rows)
            finish('unsaved_preview_ready_for_visual_review')
            return
        case_name, poses, view_name, position, yaw = rows[state['index']]
        if state['stage'] == 0:
            apply_poses(poses)
            camera.set_actor_location(unreal.Vector(*position), False, False)
            camera.set_actor_rotation(unreal.Rotator(pitch=8, yaw=yaw), False)
            state.update(stage=1, next=time.monotonic()+8, task=None)
        else:
            filename = str(out / (case_name + '-' + view_name + '.png'))
            state.update(task=unreal.AutomationLibrary.take_high_res_screenshot(1600, 900, filename, camera),
                         stage=0, index=state['index']+1, next=time.monotonic()+2)
    except Exception as error:
        state['error'] = str(error)
        finish('failed')
    finally:
        state['busy'] = False

handle = unreal.register_slate_post_tick_callback(tick)
