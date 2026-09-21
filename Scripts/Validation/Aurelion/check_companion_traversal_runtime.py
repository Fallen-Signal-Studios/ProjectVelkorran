"""Inspect whether direct NPC spawning provides a valid companion traversal fixture.

Calls the same native entry point used by AI partial-path recovery. This does not
qualify automatic follow/path selection or either mission route. Proxies that
remain staged are rejected before traversal; use campaign activation for those.
"""
from pathlib import Path
import json,os,time,traceback,unreal
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not level.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
editor_world=unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
editor_world.get_world_settings().set_editor_property('default_game_mode',unreal.GameModeBase)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
cube=unreal.load_asset('/Engine/BasicShapes/Cube')
for position,scale in [((0,0,-25),(20,20,.5)),((0,-250,50),(1,3,1)),((0,250,50),(1,3,1))]:
    actor=actors.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(*position))
    actor.static_mesh_component.set_static_mesh(cube)
    actor.static_mesh_component.set_collision_profile_name('BlockAll')
    actor.set_actor_scale3d(unreal.Vector(*scale))
light=actors.spawn_actor_from_class(unreal.DirectionalLight,unreal.Vector(300,0,600),unreal.Rotator(pitch=-45,yaw=120))
light.get_component_by_class(unreal.DirectionalLightComponent).set_intensity(5)
camera=actors.spawn_actor_from_class(unreal.CameraActor,unreal.Vector(-700,-1000,450),unreal.Rotator(pitch=-18,yaw=55))
camera.set_actor_label('TraversalReviewCamera')
camera_name=camera.get_name()
report=dict(status='running',scope=__doc__,maps_saved=[],samples=[],requests=[],errors=[])
state=dict(phase='spawn',start=time.monotonic(),busy=False,last=0)
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
def write(): (out/'companion-traversal-runtime.json').write_text(json.dumps(report,indent=2))
def finish(error=None):
    report['status']='failed' if error else 'observed_requires_review'
    if error:report['errors'].append(error)
    write();level.editor_request_end_play();state['phase']='stopping'
def tick(dt):
    if state['busy']:return
    state['busy']=True
    try:
        if state['phase']=='stopping':
            if not level.is_in_play_in_editor():
                unreal.unregister_slate_post_tick_callback(handle)
                unreal.EditorPythonScripting.set_keep_python_script_alive(False)
            return
        now=time.monotonic();assert now-state['start']<180,'Runtime probe timed out'
        world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        if not world:return
        if state['phase']=='spawn':
            subsystem=next(s for s in unreal.ObjectIterator(unreal.NarrativeCharacterSubsystem) if s.get_world()==world)
            subjects=[]
            for hero,y in (('Selene',-250),('Tarrik',250)):
                definition=unreal.load_asset('/Game/Aurelion/Characters/NPC_Aurelion'+hero+'Companion')
                subject=subsystem.spawn_npc(definition,unreal.Transform(location=unreal.Vector(-110,y,100)),unreal.NPCSpawnParams())
                assert subject and isinstance(subject,unreal.SovProtagonistCompanionCharacter)
                subjects.append(subject)
            state.update(phase='ready',subjects=subjects,at=now)
            pc=unreal.GameplayStatics.get_player_controller(world,0)
            camera=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.CameraActor) if a.get_name()==camera_name)
            pc.set_view_target_with_blend(camera,0)
            return
        subjects=state['subjects']
        if state['phase']=='ready':
            if any(s.is_character_pending_load() for s in subjects):state['at']=now;return
            if now-state['at']<10:return
            for s in subjects:
                table=s.get_editor_property('TraversalTable');assert table
                mesh=s.get_editor_property('mesh');assert mesh.get_anim_instance()
                position=s.get_actor_location().export_text()
                visual=s.get_character_visual()
                report.setdefault('initialization',[]).append(dict(actor=s.get_path_name(),
                    hidden=s.get_editor_property('hidden'),pending=s.is_character_pending_load(),
                    movement_tick=s.get_editor_property('character_movement').is_component_tick_enabled(),
                    mesh_tick=mesh.is_component_tick_enabled(),mesh_visible=mesh.is_visible(),
                    anim_class=mesh.get_anim_instance().get_class().get_path_name(),
                    root_motion_mode=str(mesh.get_anim_instance().get_editor_property('root_motion_mode')),
                    visual=visual.get_path_name() if visual else None,
                    visual_hidden=visual.get_editor_property('hidden') if visual else None))
                if s.get_editor_property('hidden') or not s.get_editor_property('character_movement').is_component_tick_enabled():
                    report['fixture_rejection']='Direct NPC spawn leaves the companion staged; requires campaign activation'
                    finish(report['fixture_rejection']);return
                accepted=s.try_attach_warp(True,unreal.Vector2D(0,1),-1)
                report['requests'].append(dict(actor=s.get_path_name(),position=position,
                    table=table.get_path_name(),accepted=accepted,
                    props=s.get_editor_property('AttachWarpProps').export_text()))
                montage=mesh.get_anim_instance().get_current_active_montage()
                if montage:
                    clips=[]
                    for path in unreal.AssetRegistryHelpers.get_asset_registry().get_dependencies(montage.get_path_name().split('.')[0],unreal.AssetRegistryDependencyOptions()):
                        candidate=unreal.load_asset(str(path))
                        if isinstance(candidate,unreal.AnimSequence):
                            clips.append(dict(asset=candidate.get_path_name(),root_motion=candidate.get_editor_property('enable_root_motion'),
                                force_root_lock=candidate.get_editor_property('force_root_lock')))
                    report.setdefault('montage_clips',{})[montage.get_path_name()]=clips
            state.update(phase='observe',at=now);write();return
        if now-state['last']>.05:
            state['last']=now
            for s in subjects:
                anim=s.get_editor_property('mesh').get_anim_instance();montage=anim.get_current_active_montage()
                report['samples'].append(dict(elapsed=now-state['at'],actor=s.get_path_name(),
                    position=s.get_actor_location().export_text(),montage=montage.get_path_name() if montage else None,
                    montage_position=anim.montage_get_position(montage) if montage else None,
                    root_position=s.get_editor_property('mesh').get_socket_location('root').export_text(),
                    pelvis_position=s.get_editor_property('mesh').get_socket_location('pelvis').export_text(),
                    warp=s.get_editor_property('IsPlayingAttachWarpMontage'),
                    movement=str(s.get_editor_property('character_movement').get_editor_property('movement_mode'))))
            write()
        if now-state['at']>.25 and 'capture' not in state:
            state['capture']=unreal.AutomationLibrary.take_high_res_screenshot(1800,1000,str(out/'companion-traversal.png'))
        if now-state['at']>8:
            if state.get('capture') and not state['capture'].is_task_done():return
            finish()
    except Exception:finish(traceback.format_exc())
    finally:state['busy']=False
handle=unreal.register_slate_post_tick_callback(tick);write();level.editor_request_begin_play()
