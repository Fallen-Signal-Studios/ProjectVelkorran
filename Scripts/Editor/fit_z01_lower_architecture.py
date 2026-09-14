"""Fit custom lower-wall modules at unit scale; preview by default, explicit entry point saves."""
import json
import os
from pathlib import Path
import shutil
import statistics
import time
import unreal
persist=bool(globals().get('PERSIST',False))
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not editor.is_in_play_in_editor()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_name()=='L_Aurelion_M12'
subsystem=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
original=list(subsystem.get_all_level_actors())
by_label={a.get_actor_label():a for a in original}
def snapshot(actors):
    return {a.get_path_name():dict(transform=a.get_actor_transform().export_text(),
        collision=a.get_actor_enable_collision(),components=[(c.get_name(),c.get_world_transform().export_text(),str(c.get_collision_enabled()))
        for c in a.get_components_by_class(unreal.PrimitiveComponent)]) for a in actors}
before=snapshot(original)
for side,x in ((-1,-8300),(1,-5700)):
    wall=by_label['Z01_Wall_EW'+str(side)]; p=wall.get_actor_location()
    assert abs(p.x-x)<.1 and abs(p.y+14700)<.1 and abs(p.z-300)<.1
    assert wall.static_mesh_component.get_collision_enabled()==unreal.CollisionEnabled.QUERY_AND_PHYSICS
meshes={name:unreal.load_asset('/Game/Aurelion/Environment/ArchitectureKit/Meshes/SM_Aurelion_KIT_'+name)
        for name in ('WallPlain_4x7','Pier_1x7')}
assert all(meshes.values())
enclosure=by_label['aurelionwalls']
upper_name='SM_Aurelion_Z01_UpperEnclosure'
upper_path='/Game/Aurelion/Environment/ArchitectureKit/Meshes/'+upper_name
upper_source=Path(unreal.Paths.project_dir())/'Art/Source/Aurelion/Z01Upper'/(upper_name+'.fbx')
if not unreal.EditorAssetLibrary.does_asset_exist(upper_path):
    task=unreal.AssetImportTask(); task.filename=str(upper_source); task.destination_path=upper_path.rsplit('/',1)[0]
    task.destination_name=upper_name; task.automated=True; task.save=False; task.factory=unreal.FbxFactory()
    options=unreal.FbxImportUI(); options.import_mesh=True; options.import_as_skeletal=False
    options.import_materials=False; options.import_textures=False; options.automated_import_should_detect_type=False
    options.mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH
    options.static_mesh_import_data.combine_meshes=True; options.static_mesh_import_data.auto_generate_collision=False
    options.static_mesh_import_data.generate_lightmap_u_vs=True
    options.static_mesh_import_data.normal_import_method=unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS
    task.options=options; unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
upper_mesh=unreal.load_asset(upper_path); assert upper_mesh
assert Path(upper_mesh.get_editor_property('asset_import_data').get_first_filename()).resolve()==upper_source.resolve()
for i,slot in enumerate(upper_mesh.get_editor_property('static_materials')):
    index=int(str(slot.get_editor_property('imported_material_slot_name')).removeprefix('M_Z01_Upper_'))
    upper_mesh.set_material(i,enclosure.static_mesh_component.get_material(index))
assert len(upper_mesh.get_editor_property('static_materials'))==4
assert unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem).get_simple_collision_count(upper_mesh)==0
assert unreal.EditorAssetLibrary.save_loaded_asset(upper_mesh)
ignored=[a for a in original if a!=enclosure]
probes=[]; surface_faces={}
for side,end_x in (('West',-8500),('East',-5500)):
    samples=[]
    for y in [-17300+400*i for i in range(14)]:
        for z in (150,350,550):
            raw=unreal.SystemLibrary.line_trace_single(world,unreal.Vector(-7000,y,z),unreal.Vector(end_x,y,z),
                unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,True,ignored,unreal.DrawDebugTrace.NONE,True)
            result=next(v for v in raw if isinstance(v,unreal.HitResult)) if isinstance(raw,tuple) else raw
            assert isinstance(result,unreal.HitResult) and result.to_tuple()[0], (side,y,z,'No enclosure surface')
            point=result.to_tuple()[5]
            samples.append(point.x); probes.append(dict(side=side,y=y,z=z,x=point.x))
    surface_faces[side]=statistics.median(samples)+(1 if side=='West' else -1)
(out/'wall-surface-probes.json').write_text(json.dumps(dict(probes=probes,median_front_planes=surface_faces),indent=2))
upper=by_label.get('KIT_Z01_UpperEnclosure')
if not upper: upper=subsystem.spawn_actor_from_class(unreal.StaticMeshActor,enclosure.get_actor_location())
upper.set_actor_label('KIT_Z01_UpperEnclosure'); upper.set_folder_path('Aurelion/CustomArchitecture/Z01/UpperEnclosure')
upper.set_actor_transform(enclosure.get_actor_transform(),False,False)
upper.static_mesh_component.set_static_mesh(upper_mesh)
upper.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION); upper.set_actor_enable_collision(False)
origin,extent=upper.get_actor_bounds(False)
assert abs(origin.z-extent.z-695)<.2,(origin.z,extent.z)
enclosure.static_mesh_component.set_visibility(False,False)
enclosure.static_mesh_component.set_hidden_in_game(True,False)
rows=[]
for side,face,yaw in (('West',surface_faces['West'],-90),('East',surface_faces['East'],90)):
    for name,stations in (('WallPlain_4x7',[-17300+400*i for i in range(14)]),
                          ('Pier_1x7',[-17500+400*i for i in range(1,14)])):
        mesh=meshes[name]; bounds=mesh.get_bounds(); front=bounds.origin.y+bounds.box_extent.y
        x=face-front if side=='West' else face+front
        for i,y in enumerate(stations):
            label='KIT_Z01_'+side+'_'+name+'_'+str(i).zfill(2)
            a=by_label.get(label)
            if a: assert isinstance(a,unreal.StaticMeshActor)
            else: a=subsystem.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(x,y,0),unreal.Rotator(yaw=yaw))
            a.set_actor_label(label); a.set_folder_path('Aurelion/CustomArchitecture/Z01/LowerWalls')
            a.set_actor_location(unreal.Vector(x,y,0),False,False); a.set_actor_rotation(unreal.Rotator(yaw=yaw),False)
            a.set_actor_scale3d(unreal.Vector(1,1,1))
            c=a.static_mesh_component; c.set_static_mesh(mesh); c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
            a.set_actor_enable_collision(False)
            origin,extent=a.get_actor_bounds(False)
            actual_face=origin.x+extent.x if side=='West' else origin.x-extent.x
            assert abs(actual_face-face)<.2,(label,actual_face,face)
            rows.append(dict(label=label,mesh=mesh.get_path_name(),transform=a.get_actor_transform().export_text(),front_plane_x=actual_face))
assert snapshot(original)==before, 'Existing actor transforms/collision changed'
assert len(rows)==54
if persist:
    source=Path(unreal.Paths.project_dir())/'Content/Aurelion/Maps/L_Aurelion_M12.umap'
    shutil.copy2(source,out/'L_Aurelion_M12-before.umap')
    assert editor.save_current_level()
(out/'z01-fit.json').write_text(json.dumps(dict(status='saved' if persist else 'unsaved_preview',placements=rows,
    original_actors_preserved=len(original),upper_shell=upper_mesh.get_path_name(),
    scope='Lower-wall replacement plus retained upper-shell derivative; original collision retained; no mission qualification'),indent=2))
p=by_label['Z01_Entry_StandIn'].get_actor_location()
camera=subsystem.spawn_actor_from_class(unreal.CameraActor,unreal.Vector(p.x,p.y,p.z+165),unreal.Rotator(yaw=90))
camera.get_component_by_class(unreal.CameraComponent).set_field_of_view(90)
editor.pilot_level_actor(camera)
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
state=dict(start=time.monotonic(),phase=0,busy=False)
views=[('entry',unreal.Vector(p.x,p.y,p.z+165),unreal.Rotator(yaw=90),90),
       ('west-wall',unreal.Vector(-7600,-14900,165),unreal.Rotator(pitch=12,yaw=180),75)]
def tick(delta):
    if state['busy']: return
    state['busy']=True
    try:
        elapsed=time.monotonic()-state['start']; phase=state['phase']
        if state.get('task') and not state['task'].is_task_done(): return
        if phase<4 and elapsed>15+phase*10:
            name,pos,rot,fov=views[phase//2]
            state['phase']+=1
            if phase%2==0:
                camera.set_actor_location(pos,False,False); camera.set_actor_rotation(rot,False)
                camera.get_component_by_class(unreal.CameraComponent).set_field_of_view(fov); editor.pilot_level_actor(camera)
            else: state['task']=unreal.AutomationLibrary.take_high_res_screenshot(1600,900,str(out/('z01-'+name+'.png')),camera)
        elif phase==4 and elapsed>60:
            assert all((out/('z01-'+v[0]+'.png')).exists() for v in views)
            unreal.unregister_slate_post_tick_callback(state['handle']); unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    except Exception:
        unreal.unregister_slate_post_tick_callback(state['handle']); unreal.EditorPythonScripting.set_keep_python_script_alive(False)
        raise
    finally: state['busy']=False
state['handle']=unreal.register_slate_post_tick_callback(tick)
