"""Read-only current fit survey for the generic Z11 observation-room wall field."""
from pathlib import Path
from collections import Counter
import hashlib
import json
import os
import runpy
import unreal

root = Path(unreal.Paths.project_dir())
out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_name() == 'L_Aurelion_M13' and not level.is_in_play_in_editor()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
maps = {name: root/'Content/Aurelion/Maps'/name for name in ('L_Aurelion_M12.umap','L_Aurelion_M13.umap')}
digest = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
before = {name: digest(path) for name,path in maps.items()}
assert before['L_Aurelion_M13.umap'] == 'a98dd8dafb2839836d64e18dfadce23cbf52c7b8edd826543e8642de6a271b5c'
actors = list(unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors())
by_label = {actor.get_actor_label(): actor for actor in actors}
owner = by_label['Aurelion_Art_M13_Z11_1_f5f218']
parts = [c for c in owner.get_components_by_class(unreal.InstancedStaticMeshComponent)
         if c.static_mesh and c.static_mesh.get_name() == 'SM_Scifi_Wall_03_4m']
assert len(parts) == 1
part = parts[0]
mesh = part.static_mesh
bounds = mesh.get_bounds()
rows = []
for i in range(part.get_instance_count()):
    t = part.get_instance_transform(i, world_space=True)
    p = t.translation
    rows.append(dict(index=i, transform=t.export_text(), x=round(p.x,3), y=round(p.y,3),
                     z=round(p.z,3), scale=t.scale3d.export_text(), rotation=t.rotation.export_text()))
assert len(rows) == 146
native = {}
for label in ('Z11_Floor','Z11_Wall_S-1','Z11_Wall_S1','Z11_Lintel_S','Z11_Wall_EW1',
              'Z11_Rib_1_-1','Z11_Rib_1_1'):
    actor = by_label[label]
    c = actor.static_mesh_component
    native[label] = dict(transform=actor.get_actor_transform().export_text(),
                         visible=c.get_editor_property('visible'), collision=str(c.get_collision_enabled()))
assert all(not value['visible'] for value in native.values())
assert {name: digest(path) for name,path in maps.items()} == before
(out/'z11-wall-field.json').write_text(json.dumps(dict(
    status='read_only', map_hashes=before, owner=owner.get_actor_label(),
    mesh=mesh.get_path_name(), mesh_bounds_origin=bounds.origin.export_text(),
    mesh_bounds_extent=bounds.box_extent.export_text(), instance_count=len(rows),
    component_collision=str(part.get_collision_enabled()),
    materials=[part.get_material(i).get_path_name() for i in range(part.get_num_materials())],
    z_counts=dict(Counter(str(row['z']) for row in rows)), instances=rows,
    native=native),indent=2))
runpy.run_path(str(root/'Scripts/Editor/preview_m13_route.py'),init_globals={
    'M13_ROUTE_VIEWS':[('z11-wall-center',(750,43100,190)),
                       ('z11-wall-oblique',(600,42650,190))],
    'M13_ROUTE_YAWS':{'z11-wall-center':180,'z11-wall-oblique':170},
    'M13_ROUTE_PITCHES':{'z11-wall-center':-8,'z11-wall-oblique':-6},
})
print('M13_Z11_WALL_FIELD_READ_ONLY_PASS')
