"""Read-only complete departure floor, roof and native-body census."""
import hashlib,json,os,runpy
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not level.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_name()=='L_Aurelion_M13'
rows=[]
for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
    label=actor.get_actor_label()
    if not (label.startswith('Z12_') or label.startswith('Aurelion_Art_M13_Z12_')):continue
    origin,extent=actor.get_actor_bounds(False);components=[]
    for c in actor.get_components_by_class(unreal.StaticMeshComponent):
        mesh=c.static_mesh
        if not mesh:continue
        b=mesh.get_bounds()
        transforms=([c.get_instance_transform(i,world_space=True).export_text() for i in range(c.get_instance_count())]
            if isinstance(c,unreal.InstancedStaticMeshComponent) else [c.get_world_transform().export_text()])
        components.append(dict(name=c.get_name(),mesh=mesh.get_path_name(),mesh_origin=b.origin.export_text(),mesh_extent=b.box_extent.export_text(),
            transforms=transforms,collision=str(c.get_collision_enabled()),visible=c.get_editor_property('visible'),hidden_in_game=c.get_editor_property('hidden_in_game'),
            materials=[c.get_material(i).get_path_name() if c.get_material(i) else None for i in range(c.get_num_materials())]))
    rows.append(dict(actor=label,transform=actor.get_actor_transform().export_text(),origin=origin.export_text(),extent=extent.export_text(),
        collision=actor.get_actor_enable_collision(),hidden=actor.get_editor_property('hidden'),components=components))
(out/'departure-envelope.json').write_text(json.dumps(dict(read_only=True,actors=rows,
    map_hashes={n:hashlib.sha256((root/'Content/Aurelion/Maps'/n).read_bytes()).hexdigest() for n in ('L_Aurelion_M12.umap','L_Aurelion_M13.umap')}),indent=2))
runpy.run_path(str(root/'Scripts/Editor/preview_m13_route.py'),init_globals={
    'M13_ROUTE_VIEWS':[('floor-overview',(0,47900,450)),('ceiling-overview',(0,47700,170))],
    'M13_ROUTE_YAWS':{'floor-overview':-90,'ceiling-overview':-90},
    'M13_ROUTE_PITCHES':{'floor-overview':-30,'ceiling-overview':25}})
