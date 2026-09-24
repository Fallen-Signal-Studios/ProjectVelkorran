"""Fresh UE load qualification after the guarded Z12 plate save."""
import hashlib
import json
import os
import runpy
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir())
out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_name() == 'L_Aurelion_M13' and not level.is_in_play_in_editor()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
baseline = root / 'Saved/Validation/Aurelion/Z12RemnantPlateSaved-20260923-220323-de8f6c05/L_Aurelion_M13.before.umap'
assert hashlib.sha256(baseline.read_bytes()).hexdigest() == 'ce18e4035a2ce6726d2970c1669a3255937ec7327b918aba06aef0187dc27250'
rows = []
all_actors = list(actors.get_all_level_actors())
for faction, x, z, yaw, code in (
    ('Dominion', -8700, 400, 180, 'return float2(UV.y * 0.42, 1.0-UV.x);'),
    ('Reformation', 8700, 0, 0,
     'return float2(0.85 + UV.y * 0.14, 0.25 + (1.0-UV.x) * 0.45);'),
):
    label = 'Aurelion_Z12_%s_RemnantPlate' % faction
    found = [a for a in all_actors if a.get_actor_label() == label]
    assert len(found) == 1
    actor = found[0]
    assert max(abs(a-b) for a, b in zip((actor.get_actor_location().x,
                                         actor.get_actor_location().y,
                                         actor.get_actor_location().z),
                                        (x, 47500, z))) < .1
    assert abs(actor.get_actor_rotation().yaw - yaw) < .1 or abs(abs(actor.get_actor_rotation().yaw-yaw)-360) < .1
    scale = actor.get_actor_scale3d()
    assert max(abs(a-b) for a, b in zip((scale.x, scale.y, scale.z), (30, 40, 1))) < .001
    component = actor.static_mesh_component
    assert component.static_mesh.get_path_name() == '/Engine/BasicShapes/Plane.Plane'
    assert component.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION
    assert not component.get_editor_property('can_ever_affect_navigation')
    assert not actor.get_actor_enable_collision()
    material = component.get_material(0)
    assert material.get_path_name().endswith('M_Aurelion_Z12%sRemnant.M_Aurelion_Z12%sRemnant' % (faction, faction))
    edit = unreal.MaterialEditingLibrary
    sample = edit.get_material_property_input_node(material, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    custom = [node for node in edit.get_inputs_for_material_expression(material, sample)
              if isinstance(node, unreal.MaterialExpressionCustom)]
    assert len(custom) == 1 and custom[0].get_editor_property('code') == code
    rows.append(dict(label=label, transform=actor.get_actor_transform().export_text(),
                     material=material.get_path_name(), collision='NoCollision', nav=False))
unexpected = [a.get_actor_label() for a in all_actors
              if a.get_actor_label().startswith('Aurelion_Z12_') and 'OpenVista_' in a.get_actor_label()]
assert not unexpected, unexpected
assert sum('RemnantPlate' in a.get_actor_label() for a in all_actors) == 2
maps = {name: hashlib.sha256((root / 'Content/Aurelion/Maps' / name).read_bytes()).hexdigest()
        for name in ('L_Aurelion_M12.umap', 'L_Aurelion_M13.umap')}
assert maps['L_Aurelion_M12.umap'] == '64a4517bb88719694793a46ae859f0eb6fbc861eef10e956e0574d8962b844a4'
assert maps['L_Aurelion_M13.umap'] == 'f50d390fb9dc648c092fb92d083d326ab364cbe5007c5dd005bd6da721726503'
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
(out / 'remnant-plates-fresh.json').write_text(json.dumps(dict(
    status='fresh_map_pass', plates=rows, actor_count=len(all_actors), map_hashes=maps,
    original_map_backup_sha256=hashlib.sha256(baseline.read_bytes()).hexdigest(),
    qualification='Fresh asset/map load, exact two-plate state; PIE recovery and performance separate.'
), indent=2))
runpy.run_path(str(root / 'Scripts/Editor/preview_m13_route.py'), init_globals={
    'M13_ROUTE_VIEWS': [('dominion-window', (-900, 47200, 180)),
                        ('reformation-window', (900, 47800, 180))],
    'M13_ROUTE_YAWS': {'dominion-window': 180, 'reformation-window': 0},
    'M13_ROUTE_PITCHES': {'dominion-window': 9, 'reformation-window': 9},
})
print('Z12_REMNANT_PLATES_FRESH_MAP_PASS')
