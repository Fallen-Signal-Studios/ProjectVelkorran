"""Review the saved custom kit from assembly, player-height and detail cameras."""
import json
import os
from pathlib import Path
import time
import unreal
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not editor.is_in_play_in_editor()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
assert editor.load_level('/Game/Aurelion/ArtReview/L_Aurelion_ArchitectureKit')
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
plain=unreal.load_asset('/Game/Aurelion/Environment/ArchitectureKit/Meshes/SM_Aurelion_KIT_WallPlain_4x7')
portal_mesh=unreal.load_asset('/Game/Aurelion/Environment/ArchitectureKit/Meshes/SM_Aurelion_KIT_Portal_6m')
assert plain and portal_mesh
for a in actors.get_all_level_actors():
    if a.get_actor_label()=='KIT_REVIEW_WallBay_4x7_200': a.static_mesh_component.set_static_mesh(plain)
matches=[a for a in actors.get_all_level_actors() if a.get_actor_label()=='KIT_REVIEW_Portal_6m_1400']
assert len(matches)<=1
portal=matches[0] if matches else actors.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(1400,0,0))
portal.set_actor_label('KIT_REVIEW_Portal_6m_1400'); portal.static_mesh_component.set_static_mesh(portal_mesh)
portal.static_mesh_component.set_collision_profile_name('BlockAll')
lights=[a for a in actors.get_all_level_actors() if a.get_actor_label()=='KIT_REVIEW_PortalKey']
if not lights:
    pos=unreal.Vector(1000,-600,1000)
    light=actors.spawn_actor_from_class(unreal.RectLight,pos,unreal.MathLibrary.find_look_at_rotation(pos,unreal.Vector(1400,0,350)))
    light.set_actor_label('KIT_REVIEW_PortalKey')
    c=light.get_component_by_class(unreal.RectLightComponent); c.set_mobility(unreal.ComponentMobility.MOVABLE)
    c.set_editor_property('intensity_units',unreal.LightUnits.LUMENS); c.set_intensity(50000)
    c.set_attenuation_radius(1800); c.set_source_width(700); c.set_source_height(700)
rows=[]
for a in actors.get_all_level_actors():
    if a.get_actor_label().startswith('KIT_REVIEW_') and isinstance(a,unreal.StaticMeshActor):
        a.set_actor_rotation(unreal.Rotator(yaw=180),False)
        rows.append(dict(label=a.get_actor_label(),rotation=a.get_actor_rotation().export_text()))
existing=[a for a in actors.get_all_level_actors() if isinstance(a,unreal.CameraActor) and a.get_actor_label()=='KIT_REVIEW_Camera']
assert len(existing)<=1
camera=existing[0] if existing else actors.spawn_actor_from_class(unreal.CameraActor,unreal.Vector(850,-1400,620))
camera.set_actor_location(unreal.Vector(850,-1400,620),False,False)
camera.set_actor_label('KIT_REVIEW_Camera')
component=camera.get_component_by_class(unreal.CameraComponent)
component.set_field_of_view(50)
camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(camera.get_actor_location(),unreal.Vector(0,0,330)),False)
assert editor.save_current_level()
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
unreal.SystemLibrary.execute_console_command(world,'r.HighResScreenshotDelay 32')
sm=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
for a in actors.get_all_level_actors():
    if a.get_actor_label().startswith('KIT_REVIEW_') and isinstance(a,unreal.StaticMeshActor):
        assert sm.get_num_uv_channels(a.static_mesh_component.static_mesh,0)==2
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
(out/'review-state.json').write_text(json.dumps(dict(assembly_orientation=rows,scope='Saved review map only'),indent=2))
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
state=dict(start=time.monotonic(),phase=0)
views=[('assembly',(850,-1400,620),(0,0,330),50),
       ('player',(-140,-650,165),(0,0,340),70),
       ('detail',(-160,-280,510),(-180,0,525),55),
       ('portal',(2300,-1500,620),(1400,0,390),50)]
def blocked(x,z):
    raw=unreal.SystemLibrary.capsule_trace_single_by_profile(world,unreal.Vector(1400+x,-500,z),
        unreal.Vector(1400+x,500,z),42,88,'Pawn',False,[],unreal.DrawDebugTrace.NONE,True)
    if raw is None: return False
    result=next(v for v in raw if isinstance(v,unreal.HitResult)) if isinstance(raw,tuple) else raw
    return bool(result.to_tuple()[0])
def tick(delta):
    if state.get('busy'):
        return
    state['busy']=True
    try:
        elapsed=time.monotonic()-state['start']
        phase=state['phase']
        if state.get('task') and not state['task'].is_task_done():
            return
        if phase==0 and elapsed>15:
            assert sm.get_convex_collision_count(portal_mesh)==6
            assert all(not blocked(x,z) for x in (-250,0,250) for z in (88,250,440)), 'Capsule passage obstructed'
            assert all(blocked(x,250) for x in (-350,350)), 'Portal frame collision missing'
            state['portal_collision_verified']=True
        if phase<len(views)*2 and elapsed>15+phase*10:
            index=phase//2; name,pos,target,fov=views[index]
            if phase%2==0:
                camera.set_actor_location(unreal.Vector(*pos),False,False)
                camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(unreal.Vector(*pos),unreal.Vector(*target)),False)
                component.set_field_of_view(fov)
                editor.pilot_level_actor(camera)
            else:
                state['task']=unreal.AutomationLibrary.take_high_res_screenshot(1600,1000,str(out/('kit-'+name+'.png')),camera)
            state['phase']+=1
        elif phase==len(views)*2 and elapsed>20+len(views)*20:
            assert all((out/('kit-'+v[0]+'.png')).exists() for v in views), 'Capture set incomplete'
            (out/'capture-complete.json').write_text(json.dumps(dict(views=[v[0] for v in views],
                imported_uv_channels=2,portal_clear_capsule_queries=9,portal_blocked_side_queries=2,
                portal_convex_hulls=6,scope='Editor art review and isolated collision queries; no mission traversal qualification'),indent=2))
            unreal.unregister_slate_post_tick_callback(state['handle'])
            unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    except Exception:
        unreal.unregister_slate_post_tick_callback(state['handle'])
        unreal.EditorPythonScripting.set_keep_python_script_alive(False)
        raise
    finally:
        state['busy']=False
state['handle']=unreal.register_slate_post_tick_callback(tick)
