"""Verify retained ceiling edits against the pre-change authored census."""
import json
from pathlib import Path
import unreal

root = Path(__file__).resolve().parents[2]
baseline = json.loads((root / 'Saved/Validation/Aurelion/CrucibleRecessAudit-20260913-094129-975a508b/crucible-recesses.json').read_text())
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
by_label = {a.get_actor_label(): a for a in actors}
assert len(by_label) == len(actors), 'Duplicate actor labels need explicit matching'
checked = 0
visual_bounds_changes = []
for row in baseline['actors']:
    actor = by_label[row['label']]
    origin, extent = actor.get_actor_bounds(False)
    assert actor.get_actor_location().export_text() == row['location'], row['label']
    if origin.export_text() != row['bounds_origin'] or extent.export_text() != row['bounds_extent']:
        # HISM render bounds can finish updating after the startup census. Keep
        # those differences visible; never treat changed physical bounds as OK.
        assert not row['collision'] or (row['meshes'] and all(m['collision'] == '<CollisionEnabled.NO_COLLISION: 0>' for m in row['meshes'])), row['label']
        visual_bounds_changes.append(dict(label=row['label'], old_origin=row['bounds_origin'],
            old_extent=row['bounds_extent'], origin=origin.export_text(), extent=extent.export_text()))
    assert actor.get_actor_enable_collision() == row['collision'], row['label']
    components = actor.get_components_by_class(unreal.StaticMeshComponent)
    assert len(components) == len(row['meshes']), row['label']
    for c, old in zip(components, row['meshes']):
        mesh = c.get_editor_property('static_mesh')
        assert (mesh.get_path_name() if mesh else None) == old['mesh'], row['label']
        assert str(c.get_collision_enabled()) == old['collision'], row['label']
    checked += 1
for side in (-1, 1):
    a = by_label['ENVL_Z08_CeilingBounce_' + str(side)]
    c = a.get_component_by_class(unreal.RectLightComponent)
    assert c and abs(c.get_editor_property('intensity') - 6000) < .1
    assert abs(c.get_editor_property('attenuation_radius') - 3000) < .1
ceiling = by_label['Aurelion_Art_M12_Z08_89_b5e7cb'].get_component_by_class(unreal.StaticMeshComponent)
assert ceiling.get_material(0).get_name() == 'M_Radiance_IvoryStone'
out = root / 'Saved/Validation/Aurelion/CrucibleCeiling-20260913/verification.json'
out.write_text(json.dumps(dict(status='PASS', unchanged_census_actors=checked,
    new_lights=2, intensity_lumens_each=6000,
    noncolliding_visual_bounds_changes=visual_bounds_changes,
    scope='Authored geometry/collision invariant only; runtime and GPU acceptance pending'), indent=2))
unreal.log('CRUCIBLE_CEILING_VERIFIED ' + str(checked))
