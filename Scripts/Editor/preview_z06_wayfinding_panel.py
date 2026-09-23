"""Import the owned Z06 wall cassette; inspect unsaved player-height mounting."""
from pathlib import Path
import hashlib
import json
import os
import runpy
import shutil
import time
import unreal

root=Path(unreal.Paths.project_dir())
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
source=root/'Art/Source/Aurelion/Z06WayfindingPanel'
persist=bool(globals().get('PERSIST_Z06_WAYFINDING',False))
mapfile=root/'Content/Aurelion/Maps/L_Aurelion_M12.umap'
before_hash=hashlib.sha256(mapfile.read_bytes()).hexdigest()
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_name()=='L_Aurelion_M12' and not editor.is_in_play_in_editor()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
original=list(actors.get_all_level_actors())
helpers=runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'))
sign=next(a for a in original if a.get_name()=='TextRenderActor_113')
assert sign.get_actor_label()=='Aurelion_Art_Sign_Z06_2351aa'
text=sign.get_component_by_class(unreal.TextRenderComponent)
assert str(text.text)=='BREACH RESCUE\nAHEAD: CAPTURE GALLERY'
protected=[a for a in original if a is not sign]
protected_before=helpers['snapshot_actor_state'](protected)
sign_before=sign.get_actor_transform().export_text()

dest='/Game/Aurelion/Environment/ArchitectureKit'
materials={k:dest+'/Materials/M_AurelionKit_'+v for k,v in {
    'M_Aurelion_IvoryStone':'PavingIvory',
    'M_Aurelion_AncientGold':'Gold',
    'M_Aurelion_ChannelShadow':'Reveal',
}.items()}
spec=json.loads((source/'manifest.json').read_text())['modules'][0]
unreal.SystemLibrary.execute_console_command(world,'Interchange.FeatureFlags.Import.FBX 0')
mesh=helpers['import_owned_mesh'](spec,source,dest+'/Meshes',materials)
assert mesh.get_name()=='SM_Aurelion_KIT_Z06WayfindingPanel'

# This face is 16.75m from the lane center. No authored geometry enters the
# primary traversal strip. The retained native text remains searchable/editable.
sign.modify();text.modify()
sign.set_actor_location_and_rotation(unreal.Vector(1631,7600,-366),unreal.Rotator(yaw=180),False,True)
text.set_text('BREACH RESCUE\nCAPTURE GALLERY')
text.set_world_size(32)
text.set_horizontal_alignment(unreal.HorizTextAligment.EHTA_CENTER)
text.set_vertical_alignment(unreal.VerticalTextAligment.EVRTA_TEXT_CENTER)
text.set_text_render_color(unreal.Color(238,219,175,255))
panel=actors.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(1645,7600,-425),unreal.Rotator(yaw=90))
panel.set_actor_label('KIT_Z06_Wayfinding_BreachRescue')
panel.set_folder_path('Aurelion/CustomArchitecture/Z06/Wayfinding')
component=panel.static_mesh_component
component.set_static_mesh(mesh)
component.set_collision_profile_name('NoCollision')
component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
component.set_editor_property('can_ever_affect_navigation',False)
panel.set_actor_enable_collision(False)

assert helpers['snapshot_actor_state'](protected)==protected_before
assert component.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
assert not panel.get_actor_enable_collision()
assert hashlib.sha256(mapfile.read_bytes()).hexdigest()==before_hash
assert all(p.get_name()=='/Game/Aurelion/Maps/L_Aurelion_M12'
           for p in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages())
if persist:
    shutil.copy2(mapfile,out/'L_Aurelion_M12-before.umap')
    assert editor.save_current_level()
    assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    assert hashlib.sha256(mapfile.read_bytes()).hexdigest()!=before_hash
(out/'wayfinding-preview.json').write_text(json.dumps(dict(
    status='saved' if persist else 'unsaved_preview',mesh=mesh.get_path_name(),triangles=spec['triangles'],
    uv_layers=spec['uv_layers'],original_sign_transform=sign_before,
    new_sign_transform=sign.get_actor_transform().export_text(),
    panel_transform=panel.get_actor_transform().export_text(),
    protected_original_actors=len(protected),protected_original_state_preserved=True,
    new_panel_collision='none',map_sha256_before=before_hash,
    map_sha256_after=hashlib.sha256(mapfile.read_bytes()).hexdigest()),indent=2))

editor.editor_set_game_view(True)
camera=actors.spawn_actor_from_class(unreal.CameraActor,unreal.Vector(0,7600,-435),unreal.Rotator(yaw=0))
camera.get_component_by_class(unreal.CameraComponent).set_field_of_view(85)
views=[('lane-center',(0,7600,-435),(3,0),85),
       ('route-oblique',(700,6900,-435),(3,42),75),
       ('panel-detail',(930,7600,-340),(0,0),70)]
state=dict(index=0,phase=0,next=time.monotonic()+12,busy=False)
unreal.EditorPythonScripting.set_keep_python_script_alive(True)


def tick(delta):
    if state['busy'] or time.monotonic()<state['next']:
        return
    state['busy']=True
    try:
        if state.get('task') and not state['task'].is_task_done():
            return
        if state['index']==len(views):
            assert all((out/(v[0]+'.png')).is_file() for v in views)
            editor.eject_pilot_level_actor()
            assert actors.destroy_actor(camera)
            (out/'wayfinding-capture.json').write_text(json.dumps(dict(
                status='captured_saved' if persist else 'captured_unsaved_preview',
                views=[v[0] for v in views],
                map_sha256_after=hashlib.sha256(mapfile.read_bytes()).hexdigest()),indent=2))
            unreal.unregister_slate_post_tick_callback(state['handle'])
            unreal.EditorPythonScripting.set_keep_python_script_alive(False)
            return
        name,pos,rot,fov=views[state['index']]
        if state['phase']==0:
            camera.set_actor_location(unreal.Vector(*pos),False,False)
            camera.set_actor_rotation(unreal.Rotator(pitch=rot[0],yaw=rot[1]),False)
            camera.get_component_by_class(unreal.CameraComponent).set_field_of_view(fov)
            editor.pilot_level_actor(camera)
            state.update(phase=1,next=time.monotonic()+10)
        else:
            state['task']=unreal.AutomationLibrary.take_high_res_screenshot(
                1600,900,str(out/(name+'.png')),camera)
            state.update(index=state['index']+1,phase=0,next=time.monotonic()+5)
    except Exception:
        unreal.unregister_slate_post_tick_callback(state['handle'])
        unreal.EditorPythonScripting.set_keep_python_script_alive(False)
        raise
    finally:
        state['busy']=False


state['handle']=unreal.register_slate_post_tick_callback(tick)
