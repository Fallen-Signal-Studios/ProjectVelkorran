"""Isolated Chaos solver test. Deliberate strain is not a campaign damage receipt."""
from pathlib import Path
import json, os, time, unreal

out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
worlds=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
asset=unreal.load_asset('/Game/Aurelion/ArtReview/Chaos/GC_Aurelion_CargoPrototype')
assert asset
review='/Game/Aurelion/ArtReview/Chaos/L_Aurelion_ChaosPrototype'
reuse=unreal.EditorAssetLibrary.does_asset_exist(review)
assert editor.load_level(review) if reuse else editor.new_level(review)
if reuse:
    # This dedicated review scene is fully owned by this script.
    for old in actors.get_all_level_actors():
        if old.get_actor_label() in ('Chaos_Review_Floor','Chaos_Control','Chaos_StrainTarget') or isinstance(old,(unreal.DirectionalLight,unreal.SkyLight)):
            assert actors.destroy_actor(old)
world=worlds.get_editor_world()
world.get_world_settings().set_editor_property('default_game_mode',unreal.GameModeBase)
floor=actors.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(0,0,-25))
floor.set_actor_label('Chaos_Review_Floor')
floor.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'))
floor.set_actor_scale3d(unreal.Vector(24,24,.5))
floor.static_mesh_component.set_collision_profile_name('BlockAll')
for label,y in [('Chaos_Control',-300),('Chaos_StrainTarget',300)]:
    actor=actors.spawn_actor_from_class(unreal.GeometryCollectionActor,unreal.Vector(0,y,5))
    actor.set_actor_label(label)
    comp=actor.get_component_by_class(unreal.GeometryCollectionComponent)
    comp.set_rest_collection(asset)
    comp.set_collision_profile_name('Destructible')
    comp.set_simulate_physics(True)
sun=actors.spawn_actor_from_class(unreal.DirectionalLight,unreal.Vector(0,0,800),unreal.Rotator(-45,-30,0))
sun.light_component.set_intensity(5)
sky=actors.spawn_actor_from_class(unreal.SkyLight,unreal.Vector(0,0,500))
unreal.EditorLevelLibrary.set_level_viewport_camera_info(unreal.Vector(-850,-950,550),unreal.Rotator(-20,48,0))
assert editor.save_current_level()
# Shared simulation begins here; the fresh validator loads the saved scene first.
camera=actors.spawn_actor_from_class(unreal.CameraActor,unreal.Vector(-850,-950,550),unreal.Rotator(-20,48,0))
camera.get_component_by_class(unreal.CameraComponent).set_field_of_view(65)
editor.pilot_level_actor(camera)
editor.editor_set_game_view(True)
capture_actor=actors.spawn_actor_from_class(unreal.SceneCapture2D,unreal.Vector(-850,-950,550),
    unreal.MathLibrary.find_look_at_rotation(unreal.Vector(-850,-950,550),unreal.Vector(0,0,100)))
capture_actor.set_actor_label('Chaos_Review_Capture')
capture_component=capture_actor.get_component_by_class(unreal.SceneCaptureComponent2D)
capture_component.set_editor_property('texture_target',unreal.RenderingLibrary.create_render_target2d(
    worlds.get_editor_world(),1600,900,unreal.TextureRenderTargetFormat.RTF_RGBA8))
capture_component.set_editor_property('capture_source',unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
capture_component.set_editor_property('fov_angle',65.0)
capture_component.set_editor_property('capture_every_frame',True)
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
state=dict(stage='starting',started=time.monotonic(),samples=[])
editor.editor_play_simulate()

def capture(game,name):
    live=[a for a in unreal.GameplayStatics.get_all_actors_of_class(game,unreal.SceneCapture2D)
          if a.get_actor_label()=='Chaos_Review_Capture']
    assert len(live)==1
    component=live[0].get_component_by_class(unreal.SceneCaptureComponent2D)
    component.capture_scene()
    state['pending_capture']=(component,name,time.monotonic()+.5)

def sample(world):
    found={a.get_actor_label():a.get_component_by_class(unreal.GeometryCollectionComponent)
           for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.GeometryCollectionActor)}
    assert set(found)=={'Chaos_Control','Chaos_StrainTarget'}, list(found)
    row={name:dict(broken=c.is_root_broken(),root_index=c.get_root_index(),
                   root_transform=c.get_root_current_transform().export_text(),
                   sockets={str(n):[c.get_socket_location(n).x,c.get_socket_location(n).y,c.get_socket_location(n).z]
                            for n in c.get_all_socket_names()},
                   transforms=len(c.get_local_rest_transforms())) for name,c in found.items()}
    state['samples'].append(dict(stage=state['stage'],time_seconds=unreal.GameplayStatics.get_time_seconds(world),actors=row))
    (out/'chaos-solver-progress.json').write_text(json.dumps({k:v for k,v in state.items() if k not in ('capture','pending_capture')},indent=2))
    return found,row

def finish():
    unreal.unregister_slate_post_tick_callback(handle)
    try:editor.editor_request_end_play()
    finally:unreal.EditorPythonScripting.set_keep_python_script_alive(False)

def tick(delta):
    try:
        assert time.monotonic()-state['started']<150, 'Chaos simulation timed out'
        game=worlds.get_game_world()
        if not game:return
        if state.get('pending_capture'):
            component,name,ready=state['pending_capture']
            if time.monotonic()<ready:return
            unreal.RenderingLibrary.export_render_target(game,component.get_editor_property('texture_target'),str(out),name+'.png')
            del state['pending_capture']
        now=unreal.GameplayStatics.get_time_seconds(game)
        if state['stage']=='starting' and now>=10:
            found,row=sample(game)
            assert all(not r['broken'] for r in row.values()), 'Spontaneous spawn fracture'
            capture(game,'chaos-intact')
            target=found['Chaos_StrainTarget']
            target.apply_external_strain(target.get_root_index(),unreal.Vector(0,300,100),300,0,1,100)
            state.update(stage='low_strain',at=now)
        elif state['stage']=='low_strain' and now-state['at']>=3:
            found,row=sample(game)
            assert all(not r['broken'] for r in row.values()), 'Low strain prematurely fractured cover'
            target=found['Chaos_StrainTarget']
            target.apply_external_strain(target.get_root_index(),unreal.Vector(0,300,100),300,0,1,100000000)
            state.update(stage='high_strain',at=now)
        elif state['stage']=='high_strain' and now-state['at']>=1:
            found,row=sample(game)
            assert row['Chaos_StrainTarget']['broken'] and not row['Chaos_Control']['broken']
            found['Chaos_StrainTarget'].add_radial_impulse(unreal.Vector(-80,280,40),350,250,unreal.RadialImpulseFalloff.RIF_CONSTANT,True)
            for index in range(1,13):
                found['Chaos_StrainTarget'].apply_linear_velocity(index,unreal.Vector(200 if index%2 else -200,150,250))
            state.update(stage='fragment_impulse',at=now)
        elif state['stage']=='fragment_impulse' and now-state['at']>=3:
            found,row=sample(game)
            assert not row['Chaos_Control']['broken'], 'Unaffected control fractured'
            assert row['Chaos_StrainTarget']['broken'], 'High strain did not fracture target'
            before=state['samples'][0]['actors']['Chaos_StrainTarget']['sockets']
            after=row['Chaos_StrainTarget']['sockets']
            motion={key:sum((a-b)**2 for a,b in zip(value,before[key]))**.5 for key,value in after.items()}
            state['fragment_displacement_cm']=motion
            (out/'chaos-fragment-motion.json').write_text(json.dumps(motion,indent=2))
            assert len(motion)==13 and sum(value>10 for value in motion.values())==12, 'Expected all twelve fragments to move'
            capture(game,'chaos-fractured')
            state['stage']='capturing_final'
        elif state['stage']=='capturing_final':
            assert (out/'chaos-intact.png').is_file() and (out/'chaos-fractured.png').is_file()
            (out/'chaos-solver-verification.json').write_text(json.dumps(dict(status='passed',samples=state['samples'],
                qualification='Isolated solver test only. Real attacks, fracture appearance, navigation, debris budgets and checkpoint restoration remain unqualified.'),indent=2))
            finish()
    except Exception:
        finish()
        raise
handle=unreal.register_slate_post_tick_callback(tick)
