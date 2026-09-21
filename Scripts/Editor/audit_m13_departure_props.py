"""Identify visible departure geometry and capture its saved presentation.

Read-only census; camera-only editor captures do not qualify live gameplay.
"""
import hashlib,json,os,runpy
from pathlib import Path
import unreal

root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not level.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_name()=='L_Aurelion_M13'
rows=[];physical=[]
for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
    position=actor.get_actor_location()
    if 46000<=position.y<=49000 and abs(position.x)<=5500:
        origin,extent=actor.get_actor_bounds(False)
        physical.append(dict(actor=actor.get_actor_label(),class_name=actor.get_class().get_name(),
            transform=actor.get_actor_transform().export_text(),origin=origin.export_text(),extent=extent.export_text(),
            collision=actor.get_actor_enable_collision(),hidden=actor.get_editor_property('hidden'),
            primitives=[dict(name=c.get_name(),collision=str(c.get_collision_enabled())) for c in actor.get_components_by_class(unreal.PrimitiveComponent)]))
    if actor.get_editor_property('hidden'):continue
    for component in actor.get_components_by_class(unreal.StaticMeshComponent):
        mesh=component.static_mesh
        if not mesh or not component.get_editor_property('visible') or component.get_editor_property('hidden_in_game'):continue
        transforms=([(i,component.get_instance_transform(i,world_space=True)) for i in range(component.get_instance_count())]
            if isinstance(component,unreal.InstancedStaticMeshComponent) else [(None,component.get_world_transform())])
        near=[dict(index=i,transform=t.export_text()) for i,t in transforms if
            actor.get_actor_label()=='Aurelion_Art_M13_Z12_6_21a580' or (46500<=t.translation.y<=49000 and abs(t.translation.x)<=5500)]
        if not near:continue
        bounds=mesh.get_bounds()
        rows.append(dict(actor=actor.get_actor_label(),actor_path=actor.get_path_name(),component=component.get_name(),
            mesh=mesh.get_path_name(),mesh_origin=bounds.origin.export_text(),mesh_extent=bounds.box_extent.export_text(),
            actor_transform=actor.get_actor_transform().export_text(),collision=str(component.get_collision_enabled()),
            actor_collision=actor.get_actor_enable_collision(),materials=[component.get_material(i).get_path_name() if component.get_material(i) else None for i in range(component.get_num_materials())],instances=near))
maps={name:hashlib.sha256((root/'Content/Aurelion/Maps'/name).read_bytes()).hexdigest() for name in ('L_Aurelion_M12.umap','L_Aurelion_M13.umap')}
(out/'departure-props.json').write_text(json.dumps(dict(status='read_only',components=rows,physical=physical,map_hashes=maps),indent=2))
if not globals().get('SKIP_DEPARTURE_CAPTURE',False):runpy.run_path(str(root/'Scripts/Editor/preview_m13_route.py'),init_globals={
    'M13_ROUTE_VIEWS':[('selene-background',(1350,48200,170)),('departure-props',(0,48300,250))],
    'M13_ROUTE_YAWS':{'selene-background':-90,'departure-props':-90},'M13_ROUTE_PITCHES':{'departure-props':-5}})
