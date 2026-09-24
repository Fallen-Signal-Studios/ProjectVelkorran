"""Read-only UE 5.7 survey of Z11 scene-request console geometry and ownership."""
import hashlib
import json
import os
from pathlib import Path

import unreal

root = Path(unreal.Paths.project_dir())
out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
out.mkdir(parents=True, exist_ok=True)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world and world.get_name() == 'L_Aurelion_M13'
assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor()
map_path = root / 'Content/Aurelion/Maps/L_Aurelion_M13.umap'
before = hashlib.sha256(map_path.read_bytes()).hexdigest()

def vec(v):
    return [round(v.x, 4), round(v.y, 4), round(v.z, 4)]

def transform(t):
    return dict(location=vec(t.translation), scale=vec(t.scale3d),
                rotation=t.rotation.export_text())

rows = []
for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
    p = actor.get_actor_location()
    if not (-900 <= p.x <= 600 and 42400 <= p.y <= 43500):
        continue
    if not isinstance(actor, unreal.SovAurelionRequestActor):
        continue
    components = []
    for comp in actor.get_components_by_class(unreal.PrimitiveComponent):
        row = dict(name=comp.get_name(), cls=comp.get_class().get_name(),
                   world_transform=transform(comp.get_world_transform()),
                   collision=str(comp.get_collision_enabled()),
                   visible=bool(comp.is_visible()),
                   can_affect_nav=bool(comp.get_editor_property('can_ever_affect_navigation')))
        if isinstance(comp, unreal.StaticMeshComponent) and comp.static_mesh:
            row['mesh'] = comp.static_mesh.get_path_name()
            row['materials'] = [comp.get_material(i).get_path_name() if comp.get_material(i) else None
                                for i in range(comp.get_num_materials())]
        components.append(row)
    origin, extent = actor.get_actor_bounds(False)
    rows.append(dict(label=actor.get_actor_label(), path=actor.get_path_name(),
                     actor_transform=transform(actor.get_actor_transform()),
                     bounds_origin=vec(origin), bounds_extent=vec(extent),
                     actor_collision=bool(actor.get_actor_enable_collision()),
                     components=components))

assert len(rows) == 5, [r['label'] for r in rows]
assert all(any(c.get('mesh', '').endswith('SM_KB3D_CPI_PropConsoleLarge_A') for c in r['components'])
           for r in rows)
after = hashlib.sha256(map_path.read_bytes()).hexdigest()
assert before == after
report = dict(status='read_only_pass', map=world.get_name(), map_sha256=before,
              actors=sorted(rows, key=lambda r: r['label']))
(out / 'z11-witness-tablet-survey.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
print('Z11_WITNESS_TABLET_SURVEY_PASS', out)
