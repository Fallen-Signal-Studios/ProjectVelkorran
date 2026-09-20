"""Unsaved studio comparison of vendor poses and their Narrative retargets."""
import json
import os
import time
import traceback
from pathlib import Path
import unreal

out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
root = '/Game/Characters/Animation/SeleneGASPALS/'
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not level.is_in_play_in_editor()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
world = unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
source = unreal.load_asset('/GASPALS/Characters/UEFN_Mannequin/Meshes/SKM_UEFN_Mannequin')
target = unreal.load_asset('/NarrativePro/Pro/Core/Character/Biped/Art/Mannequin/Meshes/SKM_Quinn')
floor = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 0, -3))
floor.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Plane'))
floor.set_actor_scale3d(unreal.Vector(20, 20, 1))
for intensity, yaw in [(5, -30), (2, 150)]:
    light = actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 400), unreal.Rotator(pitch=-45, yaw=yaw))
    light.get_component_by_class(unreal.DirectionalLightComponent).set_intensity(intensity)
camera = actors.spawn_actor_from_class(unreal.CameraActor, unreal.Vector(730, 0, 105), unreal.Rotator(yaw=180))
camera.get_component_by_class(unreal.CameraComponent).set_field_of_view(60)
level.pilot_level_actor(camera)
level.editor_set_game_view(True)
level.editor_set_viewport_realtime(True)
subjects = []
evaluated = []
for index, pose in enumerate(('Stand_Idle', 'Stand_Move', 'Crouch')):
    for column, (mesh, path) in enumerate((
        (source, '/GASPALS/OverlaySystem/Overlays/Bases/Feminine/Pose_Feminine_' + pose),
        (target, root + 'SovSelene_Pose_Feminine_' + pose),
    )):
        offset = -300 + index * 240 + column * 100
        actor = actors.spawn_actor_from_class(unreal.SkeletalMeshActor, unreal.Vector(0, offset, 0), unreal.Rotator(yaw=-90))
        component = actor.skeletal_mesh_component
        component.set_skeletal_mesh_asset(mesh)
        component.set_editor_property('visibility_based_anim_tick_option', unreal.VisibilityBasedAnimTickOption.ALWAYS_TICK_POSE_AND_REFRESH_BONES)
        clip = unreal.load_asset(path)
        component.override_animation_data(clip, True, True, 0.0, 1.0)
        component.set_update_animation_in_editor(True)
        pose_data = unreal.AnimPoseExtensions.get_anim_pose_at_time(clip, 0.0, unreal.AnimPoseEvaluationOptions())
        evaluated.append(dict(asset=path, bones={bone:unreal.AnimPoseExtensions.get_bone_pose(
            pose_data, bone, unreal.AnimPoseSpaces.WORLD).export_text()
            for bone in ('pelvis', 'head', 'hand_l', 'hand_r', 'foot_l', 'foot_r')}))
        subjects.append((pose, column, actor, component, path))
        label = actors.spawn_actor_from_class(unreal.TextRenderActor, unreal.Vector(0, offset-35, 210))
        text = label.get_component_by_class(unreal.TextRenderComponent)
        text.set_text(unreal.Text(('GASP' if column == 0 else 'Selene rig') + '\n' + pose.replace('_', ' ')))
        text.set_world_size(9)

# Also inspect authoring access without modifying the Narrative animation graph.
bp = unreal.load_asset('/NarrativePro/Pro/Core/Character/Biped/Animation/ABP/Base/ABP_Biped')
capabilities = {}
for prop in ('function_graphs', 'ubergraph_pages'):
    try:
        graphs = bp.get_editor_property(prop)
        capabilities[prop] = [g.get_name() for g in graphs]
        if graphs:
            capabilities[prop + '_nodes'] = str(graphs[0].get_editor_property('nodes'))[:400]
    except Exception as exc:
        capabilities[prop] = str(exc)
state = dict(next=time.monotonic()+15, busy=False, captured=False)
unreal.EditorPythonScripting.set_keep_python_script_alive(True)

def tick(dt):
    if state['busy'] or time.monotonic() < state['next']:
        return
    state['busy'] = True
    try:
        if not state['captured']:
            state['task'] = unreal.AutomationLibrary.take_high_res_screenshot(1800, 900, str(out/'feminine-pose-comparison.png'), camera)
            state.update(captured=True, next=time.monotonic()+3)
            return
        if not state['task'].is_task_done():
            return
        rows = []
        for pose, column, actor, component, path in subjects:
            rows.append(dict(pose=pose, retargeted=bool(column), asset=path,
                bones={bone: (component.get_socket_location(bone)-actor.get_actor_location()).export_text()
                       for bone in ('pelvis', 'head', 'hand_l', 'hand_r', 'foot_l', 'foot_r')}))
        assert (out/'feminine-pose-comparison.png').exists()
        (out/'selene-pose-review.json').write_text(json.dumps(dict(status='captured_requires_visual_review',
            poses=rows, evaluated_clip_poses=evaluated, authoring_access=capabilities, maps_saved=[], gameplay_bound=False), indent=2))
        unreal.unregister_slate_post_tick_callback(handle)
        unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    except Exception:
        (out/'selene-pose-review-error.txt').write_text(traceback.format_exc())
        unreal.unregister_slate_post_tick_callback(handle)
        unreal.EditorPythonScripting.set_keep_python_script_alive(False)
        raise
    finally:
        state['busy'] = False

handle = unreal.register_slate_post_tick_callback(tick)
