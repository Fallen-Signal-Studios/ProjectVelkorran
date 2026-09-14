"""Save the three reviewed stone graphs after verifying all other surface inputs."""
import json
from pathlib import Path
import re
import runpy
import unreal

assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor()
root = Path(unreal.Paths.project_dir()).resolve()
out = root / 'Saved/Validation/Aurelion/StoneContrast-20260913'
preview = json.loads((out / 'preview.json').read_text(encoding='utf8'))
baseline = json.loads((root / 'Saved/Validation/Aurelion/HUDGeometryRoute-20260913-144115-df8125cd/surface-palette.json').read_text(encoding='utf8'))
census = runpy.run_path(str(Path(__file__).with_name('inspect_aurelion_surface_palette.py')))
current = census['rows']
canonical = lambda value: re.sub(r'0x[0-9A-Fa-f]+', '0xADDR', json.dumps(value, sort_keys=True))
library = unreal.MaterialEditingLibrary
materials = []
for row in preview['materials']:
    material = unreal.load_asset(row['material'])
    name = material.get_name()
    for prop in ('normal', 'roughness'):
        assert canonical(current[name][prop]) == canonical(baseline['materials'][name][prop]), (name, prop)
    blend = library.get_material_property_input_node(material, unreal.MaterialProperty.MP_BASE_COLOR)
    base, textured, alpha = library.get_inputs_for_material_expression(material, blend)
    assert alpha is None
    assert abs(blend.get_editor_property('const_alpha') - row['after_texture_strength']) < .001
    value = base.get_editor_property('constant')
    assert all(abs(a-b) < .001 for a,b in zip((value.r, value.g, value.b), row['after_color']))
    texture = library.get_inputs_for_material_expression(material, textured)[0]
    assert texture.get_editor_property('texture').get_path_name() == row['retained_texture']
    materials.append(material)
for material in materials:
    assert unreal.EditorAssetLibrary.save_loaded_asset(material), material.get_path_name()
(out / 'saved.json').write_text(json.dumps(dict(status='PASS',
    scope='Reviewed Z01/Z08 stopped-editor views and verified material graph values; wider runtime review pending',
    materials=[m.get_path_name() for m in materials], normal_and_roughness_unchanged=True), indent=2), encoding='utf8')
unreal.log('STONE_CONTRAST_SAVED_VERIFIED')
