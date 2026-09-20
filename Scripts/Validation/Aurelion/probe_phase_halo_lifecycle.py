"""Controlled protection presentation under continuously cleared/foreign overlay slots.
Initializes real Elite/Weaver visuals; changes floor flags and overlay slots in SIE only.
Not a mission, damage, poise, or checkpoint acceptance test. No assets/maps are saved.
"""
import hashlib,json,os,time,traceback
from pathlib import Path
import unreal
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
worlds=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert not worlds.get_game_world()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
assert level.load_level('/Game/Aurelion/ArtReview/Chaos/L_Aurelion_ChaosPrototype')
classes=[]
for i,role in enumerate(('Elite','Weaver')):
    cls=unreal.load_asset('/Game/Aurelion/Enemies/BP_Aurelion'+role).generated_class();classes.append(cls)
    actor=editor.spawn_actor_from_class(cls,unreal.Vector(0,i*380,160),unreal.Rotator(0,0,0))
    actor.set_editor_property('authored_placed_definition',unreal.load_asset('/Game/Aurelion/Enemies/NPC_Aurelion'+role))
    actor.set_actor_label('PhaseHalo'+role)
worlds.set_level_viewport_camera_info(unreal.Vector(750,190,255),unreal.Rotator(pitch=-8,yaw=180,roll=0))
capture=editor.spawn_actor_from_class(unreal.SceneCapture2D,unreal.Vector(750,190,255),unreal.Rotator(pitch=-8,yaw=180,roll=0))
capture.set_actor_label('PhaseHaloCapture')
component=capture.get_component_by_class(unreal.SceneCaptureComponent2D)
component.set_editor_property('texture_target',unreal.RenderingLibrary.create_render_target2d(worlds.get_editor_world(),1600,900,unreal.TextureRenderTargetFormat.RTF_RGBA8))
component.set_editor_property('capture_source',unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
component.set_editor_property('fov_angle',65.)
component.set_editor_property('capture_every_frame',True)
fill=editor.spawn_actor_from_class(unreal.DirectionalLight,unreal.Vector(600,190,600),unreal.Rotator(pitch=-35,yaw=180,roll=0))
fill.light_component.set_intensity(8.)
cue_class=unreal.load_class(None,'/Game/Cues/Aurelion/GC_AurelionLethalFloor.GC_AurelionLethalFloor_C')
foreign=unreal.load_asset('/Game/Cues/OverlayEffect/MI_OverlayStatic')
report=dict(status='running',scope=__doc__,stages=[],active_samples=0)
state=dict(start=time.monotonic(),at=time.monotonic(),stage=0)
def write():(out/'phase-halo-lifecycle.json').write_text(json.dumps(report,indent=2))
def screenshot(world,name,after=None):
    live=[a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.SceneCapture2D) if a.get_actor_label()=='PhaseHaloCapture']
    assert len(live)==1
    comp=live[0].get_component_by_class(unreal.SceneCaptureComponent2D)
    comp.capture_scene()
    state['capture']=(comp.get_editor_property('texture_target'),name,time.monotonic()+.5,after)
def stop(error=None):
    report.update(status='failed' if error else 'passed_requires_visual_review',error=error)
    write();level.editor_request_end_play();state['stopping']=True
def tick(delta):
    try:
        if state.get('stopping'):
            if not level.is_in_play_in_editor():
                unreal.unregister_slate_post_tick_callback(handle);unreal.EditorPythonScripting.set_keep_python_script_alive(False)
            return
        now=time.monotonic();assert now-state['start']<120,'Timed out'
        world=worlds.get_game_world()
        if not world:return
        if state.get('capture'):
            target,name,ready,after=state['capture']
            if now<ready:return
            unreal.RenderingLibrary.export_render_target(world,target,str(out),name+'.png')
            assert (out/(name+'.png')).is_file(),'Render target export did not produce evidence'
            del state['capture']
            if after:after()
        targets=[a for cls in classes for a in unreal.GameplayStatics.get_all_actors_of_class(world,cls) if a.get_actor_label().startswith('PhaseHalo')]
        if len(targets)!=2 or not all(a.get_character_visual() and a.get_character_visual().get_all_meshes() for a in targets):return
        floors=[a.get_component_by_class(unreal.SovLethalFloorComponent) for a in targets]
        meshes=[m for a in targets for m in a.get_character_visual().get_all_meshes()]
        cues=[c for c in unreal.GameplayStatics.get_all_actors_of_class(world,cue_class) if c.get_owner() in targets and not c.get_editor_property('hidden')]
        if state['stage'] in (1,2,3,5) and now-state['at']>.5:
            assert len(cues)==2,('Expected two live protection cues',len(cues))
            for cue in cues:
                assert cue.get_attach_parent_actor()==cue.get_owner(),'Protection failed to follow its owner'
                halo=cue.get_component_by_class(unreal.StaticMeshComponent)
                assert halo and halo.is_visible()
                assert halo.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
                assert halo.get_material(0).get_path_name()=='/Game/Aurelion/Enemies/Materials/M_AurelionPhaseHalo.M_AurelionPhaseHalo'
                assert (halo.get_world_location()-cue.get_owner().get_actor_location()).length()<2
            report['active_samples']+=1
        if state['stage']==2:
            for mesh in meshes:mesh.set_overlay_material(None)
        if now-state['at']<3:return
        stage=state['stage']
        if stage==0:
            for floor in floors:floor.set_floor_held(True)
        elif stage==1:
            screenshot(world,'phase-halo-active')
        elif stage==2:
            screenshot(world,'phase-halo-overlay-cleared',lambda:[mesh.set_overlay_material(foreign) for mesh in meshes])
        elif stage==3:
            assert all(mesh.get_overlay_material()==foreign for mesh in meshes),'Foreign overlay overwritten'
            for floor in floors:floor.set_floor_held(False)
        elif stage==4:
            assert not cues,'Halo survived cue removal'
            assert all(mesh.get_overlay_material()==foreign for mesh in meshes),'Foreign overlay erased during removal'
            for mesh in meshes:mesh.set_overlay_material(None)
            for floor in floors:floor.set_floor_held(True)
        elif stage==5:
            for floor in floors:floor.set_floor_held(False)
        elif stage==6:
            assert not cues,'Reused halo survived cue removal'
            screenshot(world,'phase-halo-removed')
        elif stage==7:
            assert hashlib.sha256((out/'phase-halo-active.png').read_bytes()).digest()!=hashlib.sha256((out/'phase-halo-removed.png').read_bytes()).digest(),'Stale capture: active and removed images are identical'
            stop();return
        report['stages'].append(dict(stage=stage,elapsed=now-state['start'],active_cues=len(cues)))
        state.update(stage=stage+1,at=now);write()
    except Exception:stop(traceback.format_exc())
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
handle=unreal.register_slate_post_tick_callback(tick)
write();level.editor_play_simulate()
