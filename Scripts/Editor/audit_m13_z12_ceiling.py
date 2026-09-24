"""Read-only Z12 concourse roof inventory with fixed player-eye captures."""
from pathlib import Path
import hashlib
import json
import os
import runpy
import unreal

root=Path(unreal.Paths.project_dir())
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_name()=='L_Aurelion_M13' and not level.is_in_play_in_editor()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
mapfile=root/'Content/Aurelion/Maps/L_Aurelion_M13.umap'
before=hashlib.sha256(mapfile.read_bytes()).hexdigest()
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
rows=[]
art_clusters=[]
for actor in actors:
    if actor.get_actor_label().startswith('Aurelion_Art_M13_Z12_'):
        grouped=[]
        for comp in actor.get_components_by_class(unreal.StaticMeshComponent):
            if not comp.static_mesh:continue
            item=dict(component=comp.get_name(),mesh=comp.static_mesh.get_path_name(),
                      collision=str(comp.get_collision_enabled()),visible=comp.get_editor_property('visible'))
            if isinstance(comp,unreal.InstancedStaticMeshComponent):
                positions=[comp.get_instance_transform(i,world_space=True).translation
                           for i in range(comp.get_instance_count())]
                item.update(instances=len(positions),
                            min_xyz=[min(getattr(p,axis) for p in positions) for axis in 'xyz'] if positions else [],
                            max_xyz=[max(getattr(p,axis) for p in positions) for axis in 'xyz'] if positions else [])
            grouped.append(item)
        art_clusters.append(dict(label=actor.get_actor_label(),components=grouped))
    center,extent=actor.get_actor_bounds(False)
    if center.x+extent.x < -2300 or center.x-extent.x > 2300:continue
    if center.y+extent.y < 46200 or center.y-extent.y > 48800:continue
    if center.z+extent.z < 450 or center.z-extent.z > 1800:continue
    if max(extent.x,extent.y,extent.z)>10000:continue
    components=[]
    for comp in actor.get_components_by_class(unreal.StaticMeshComponent):
        if not comp.static_mesh:continue
        item=dict(name=comp.get_name(),mesh=comp.static_mesh.get_path_name(),
                  collision=str(comp.get_collision_enabled()),visible=comp.get_editor_property('visible'),
                  hidden_in_game=comp.get_editor_property('hidden_in_game'))
        if isinstance(comp,unreal.InstancedStaticMeshComponent):
            item['instances']=comp.get_instance_count()
            item['sample_positions']=[comp.get_instance_transform(i,world_space=True).translation.export_text()
                                      for i in range(min(8,comp.get_instance_count()))]
        components.append(item)
    if not components:continue
    rows.append(dict(label=actor.get_actor_label(),path=actor.get_path_name(),
                     center=center.export_text(),extent=extent.export_text(),components=components))
assert hashlib.sha256(mapfile.read_bytes()).hexdigest()==before
(out/'z12-ceiling-audit.json').write_text(json.dumps(dict(status='read_only',m13_sha256=before,
    actor_count=len(rows),actors=rows,art_clusters=art_clusters),indent=2))
runpy.run_path(str(root/'Scripts/Editor/preview_m13_route.py'),init_globals={
    'M13_ROUTE_VIEWS':[('center-north',(0,47500,180)),('center-east',(0,47500,180))],
    'M13_ROUTE_YAWS':{'center-north':90,'center-east':0},
    'M13_ROUTE_PITCHES':{'center-north':25,'center-east':25}})
print('Z12_CEILING_READ_ONLY_AUDIT_PASS')
