"""Actual native cover owner with authored Chaos cargo; isolated simulation only."""
from pathlib import Path
import json, os, time, unreal
out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
worlds = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
# Use the existing isolated scene without changing its saved actors.
assert editor.load_level('/Game/Aurelion/ArtReview/Chaos/L_Aurelion_ChaosPrototype')
for actor in list(actors.get_all_level_actors()):
    if isinstance(actor, unreal.GeometryCollectionActor):
        assert actors.destroy_actor(actor)
mesh = unreal.load_asset('/Game/Aurelion/Environment/ArchitectureKit/Meshes/SM_Aurelion_KIT_Z08SealedStores')
collection = unreal.load_asset('/Game/Aurelion/ArtReview/Chaos/GC_Aurelion_CargoPrototype')
assert mesh and collection
bounds = mesh.get_bounds()
for i, y in enumerate((-300, 300)):
    owner = actors.spawn_actor_from_class(unreal.SovDestructibleCover, unreal.Vector(0, y, bounds.origin.z))
    owner.set_actor_label('NativeCover_Control' if i == 0 else 'NativeCover_Target')
    guid = unreal.Guid()
    guid.import_text('000000010000000200000003%08x' % (i + 1))
    owner.set_editor_property('placement_guid', guid)
    owner.set_editor_property('fractured_asset', collection)
    owner.set_editor_property('destruction_enabled', True)
    owner.obstruction.set_box_extent(bounds.box_extent)
    owner.intact_visual.set_static_mesh(mesh)
    owner.intact_visual.set_relative_location(unreal.Vector(0, 0, -bounds.origin.z), False, False)

camera_location = unreal.Vector(-850, -950, 550)
camera_rotation = unreal.MathLibrary.find_look_at_rotation(camera_location, unreal.Vector(0, 0, 100))
camera = actors.spawn_actor_from_class(unreal.CameraActor, camera_location, camera_rotation)
camera.get_component_by_class(unreal.CameraComponent).set_field_of_view(65)
editor.pilot_level_actor(camera)
editor.editor_set_game_view(True)
capture_actor = actors.spawn_actor_from_class(unreal.SceneCapture2D, camera_location,
    unreal.MathLibrary.find_look_at_rotation(camera_location, unreal.Vector(0, 0, 100)))
capture_actor.set_actor_label('NativeCover_Capture')
capture_component = capture_actor.get_component_by_class(unreal.SceneCaptureComponent2D)
capture_component.set_editor_property('texture_target', unreal.RenderingLibrary.create_render_target2d(
    worlds.get_editor_world(), 1600, 900, unreal.TextureRenderTargetFormat.RTF_RGBA8))
capture_component.set_editor_property('capture_source', unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
capture_component.set_editor_property('capture_every_frame', True)
capture_component.set_editor_property('fov_angle', 65.0)
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
state = dict(stage='settle', started=time.monotonic(), results={})

def finish():
    unreal.unregister_slate_post_tick_callback(handle)
    editor.editor_request_end_play()
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)

def capture(game, name):
    live = next(a for a in unreal.GameplayStatics.get_all_actors_of_class(game, unreal.SceneCapture2D)
                if a.get_actor_label() == 'NativeCover_Capture')
    c = live.get_component_by_class(unreal.SceneCaptureComponent2D)
    if not state.get('live_capture_target'):
        # PIE duplicates the component but can share its editor render target.
        # A separate target prevents the editor world's intact view overwriting it.
        c.set_editor_property('texture_target', unreal.RenderingLibrary.create_render_target2d(
            game, 1600, 900, unreal.TextureRenderTargetFormat.RTF_RGBA8))
        state['live_capture_target'] = True
        c.set_editor_property('capture_every_frame', True)
    c.capture_scene()
    state['capture'] = (c, name, time.monotonic() + .5)

def tick(delta):
    try:
        assert time.monotonic() - state['started'] < 120
        game = worlds.get_game_world()
        if not game: return
        if state.get('capture'):
            c, name, ready = state['capture']
            if time.monotonic() < ready: return
            unreal.RenderingLibrary.export_render_target(game, c.get_editor_property('texture_target'), str(out), name + '.png')
            del state['capture']
        owners = {a.get_actor_label(): a for a in unreal.GameplayStatics.get_all_actors_of_class(game, unreal.SovDestructibleCover)}
        target, control = owners['NativeCover_Target'], owners['NativeCover_Control']
        now = unreal.GameplayStatics.get_time_seconds(game)
        if state['stage'] == 'settle' and now > 3:
            assert not target.is_broken() and not control.is_broken()
            assert not target.get_components_by_class(unreal.GeometryCollectionComponent)
            capture(game, 'native-cover-intact')
            state['stage'] = 'damage'
        elif state['stage'] == 'damage':
            assert unreal.GameplayStatics.apply_damage(target, 20, None, None, unreal.DamageType) == 20
            assert not target.is_broken()
            assert unreal.GameplayStatics.apply_damage(target, 100, None, None, unreal.DamageType) == 100
            assert target.is_broken()
            initial_debris = target.get_components_by_class(unreal.GeometryCollectionComponent)[0]
            state['initial_sockets'] = {str(n): [initial_debris.get_socket_location(n).x,
                initial_debris.get_socket_location(n).y, initial_debris.get_socket_location(n).z]
                for n in initial_debris.get_all_socket_names()}
            assert target.obstruction.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION
            assert unreal.GameplayStatics.apply_damage(target, 100, None, None, unreal.DamageType) == 0
            state.update(stage='fracture', at=now)
        elif state['stage'] == 'fracture' and now - state['at'] > 2:
            debris = target.get_components_by_class(unreal.GeometryCollectionComponent)
            assert len(debris) == 1
            assert debris[0].is_root_broken(), 'Native damage did not fracture authored collection'
            assert debris[0].get_collision_response_to_channel(unreal.CollisionChannel.ECC_PAWN) == unreal.CollisionResponseType.ECR_IGNORE
            assert not control.is_broken()
            (out/'native-cover-debug.txt').write_text(debris[0].get_debug_info())
            locations = {str(n): [debris[0].get_socket_location(n).x, debris[0].get_socket_location(n).y,
                debris[0].get_socket_location(n).z] for n in debris[0].get_all_socket_names()}
            motion = {n: sum((a-b)**2 for a,b in zip(v,state['initial_sockets'][n]))**.5 for n,v in locations.items()}
            state['results']['displacement_cm'] = motion
            (out/'native-cover-motion.json').write_text(json.dumps(motion, indent=2))
            assert sum(distance > 10 for distance in motion.values()) >= 8, 'Fragments did not visibly separate'
            state['results']['root_broken'] = True
            state['results']['fragment_transforms'] = len(debris[0].get_initial_local_rest_transforms())
            state['results']['sockets'] = {str(n): str(debris[0].get_socket_location(n)) for n in debris[0].get_all_socket_names()}
            capture(game, 'native-cover-fractured')
            state['stage'] = 'cleanup'
        elif state['stage'] == 'cleanup' and now - state['at'] > 7:
            assert not target.get_components_by_class(unreal.GeometryCollectionComponent), 'Debris outlived budget'
            assert not control.is_broken()
            capture(game, 'native-cover-cleared')
            state['stage'] = 'final_capture'
        elif state['stage'] == 'final_capture':
            state['results'].update(status='passed', cleanup_seconds=now-state['at'],
                qualification='Native damage and Chaos simulation only; campaign, protagonist weapons, nav traversal and full checkpoint reload still pending.')
            (out/'native-cover-runtime.json').write_text(json.dumps(state['results'], indent=2))
            unreal.log('NATIVE_DESTRUCTIBLE_COVER_SIMULATION_PASS')
            finish()
    except Exception:
        finish()
        raise

handle = unreal.register_slate_post_tick_callback(tick)
editor.editor_play_simulate()
