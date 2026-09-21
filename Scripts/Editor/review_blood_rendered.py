"""Render owned blood variants at fixed ages in an unsaved comparison studio."""
import json
import os
import time
import traceback
from pathlib import Path
import unreal

out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not level.is_in_play_in_editor()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
world = unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
plane = unreal.load_asset('/Engine/BasicShapes/Plane')
back = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(-150, 0, 250), unreal.Rotator(pitch=-90))
back.static_mesh_component.set_static_mesh(plane)
back.set_actor_scale3d(unreal.Vector(12, 16, 1))
for intensity, yaw in ((5, 160), (2, 20)):
    light = actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(500, 0, 600), unreal.Rotator(pitch=-35, yaw=yaw))
    light.get_component_by_class(unreal.DirectionalLightComponent).set_intensity(intensity)
camera = actors.spawn_actor_from_class(unreal.CameraActor, unreal.Vector(1100, 0, 270), unreal.Rotator(yaw=180))
camera.get_component_by_class(unreal.CameraComponent).set_field_of_view(70)
level.pilot_level_actor(camera)
level.editor_set_game_view(True)
level.editor_set_viewport_realtime(True)
components = []
report = dict(status='preparing', maps_saved=[], gameplay_receipts=False,
              random_seed_locked=False, independent_simulation_per_capture=True, variants=[], captures=[])
for row, color in enumerate(('Red', 'Black')):
    for col, kind in enumerate(('Hit', 'Slash', 'Burst', 'Low')):
        position = unreal.Vector(0, -450 + col * 300, 430 - row * 310)
        path = '/Game/Aurelion/VFX/Blood/NS_Aurelion_Blood' + color + kind
        asset = unreal.load_asset(path)
        assert asset and unreal.SovCombatFeedbackAuthoringLibrary.finish_feedback_compilation(asset), path
        actor = actors.spawn_actor_from_class(unreal.NiagaraActor, position)
        component = actor.get_component_by_class(unreal.NiagaraComponent)
        component.set_asset(asset)
        component.set_force_solo(True)
        component.activate(True)
        component.advance_simulation(7, 1.0 / 60.0)
        component.set_paused(True)
        components.append(component)
        label = actors.spawn_actor_from_class(unreal.TextRenderActor, position + unreal.Vector(15, -90, -110))
        text = label.get_component_by_class(unreal.TextRenderComponent)
        text.set_text(unreal.Text(color + ' / ' + kind))
        text.set_world_size(22)
        report['variants'].append(dict(asset=path, position=position.export_text()))

ages = (.12, .30, .60)
state = dict(next=time.monotonic()+20, index=0, task=None, busy=False, start=time.monotonic())
unreal.EditorPythonScripting.set_keep_python_script_alive(True)


def finish():
    (out/'blood-rendered-review.json').write_text(json.dumps(report, indent=2), encoding='utf8')
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)


def tick(delta):
    if state['busy'] or time.monotonic() < state['next']:
        return
    state['busy'] = True
    try:
        assert time.monotonic()-state['start'] < 180, 'Bounded render timed out'
        if state['task'] is not None:
            if not state['task'].is_task_done():
                return
            assert Path(report['captures'][-1]['image']).exists()
            state['index'] += 1
            state['task'] = None
            if state['index'] == len(ages):
                report['status'] = 'captured_requires_visual_review'
                finish()
                return
            for component in components:
                component.set_paused(False)
                component.reinitialize_system()
                component.advance_simulation(round(ages[state['index']]*60), 1.0 / 60.0)
                component.set_paused(True)
            state['next'] = time.monotonic()+4
            return
        age = ages[state['index']]
        path = out/('blood-age-%03d.png' % round(age*1000))
        state['task'] = unreal.AutomationLibrary.take_high_res_screenshot(1800, 1000, str(path), camera)
        ticks = round(age*60)
        report['captures'].append(dict(requested_age=age, simulation_ticks=ticks,
                                       simulated_seconds=ticks/60.0, image=str(path)))
        state['next'] = time.monotonic()+2
    except Exception:
        report.update(status='failed', error=traceback.format_exc())
        finish()
    finally:
        state['busy'] = False


handle = unreal.register_slate_post_tick_callback(tick)
