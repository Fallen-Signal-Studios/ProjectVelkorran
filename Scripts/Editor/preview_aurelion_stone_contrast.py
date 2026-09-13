"""Unsaved material-only preview; run in a stopped editor after route validation."""
import json
from pathlib import Path
import shutil
import unreal

assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor(), 'End PIE first'
root = Path(unreal.Paths.project_dir()).resolve()
out = root / 'Saved/Validation/Aurelion/StoneContrast-20260913'
out.mkdir(parents=True, exist_ok=True)
library = unreal.MaterialEditingLibrary
settings = {
    'M_Radiance_LowerBlack': ((.012, .016, .022), .55),
    'M_Radiance_LowerBlackWorld': ((.012, .016, .022), .55),
    'M_Radiance_IvoryStone': ((.50, .52, .54), .45),
}
prepared = []
for name, (color, strength) in settings.items():
    path = '/Game/Aurelion/Environment/RadianceMaterials/' + name
    material = unreal.load_asset(path)
    assert isinstance(material, unreal.Material), path
    blend = library.get_material_property_input_node(material, unreal.MaterialProperty.MP_BASE_COLOR)
    assert isinstance(blend, unreal.MaterialExpressionLinearInterpolate)
    base, textured, alpha = library.get_inputs_for_material_expression(material, blend)
    assert isinstance(base, unreal.MaterialExpressionConstant3Vector)
    assert isinstance(textured, unreal.MaterialExpressionMultiply) and alpha is None
    texture = library.get_inputs_for_material_expression(material, textured)[0]
    assert isinstance(texture, unreal.MaterialExpressionTextureSample)
    assert abs(blend.get_editor_property('const_alpha') - 1.) < .001, 'Preview expects original contrast'
    disk = root / ('Content/' + path.removeprefix('/Game/') + '.uasset')
    backup = out / disk.name
    assert not backup.exists(), 'Preserve the prior preview backup'
    prepared.append((material, blend, base, texture, color, strength, disk, backup))

rows = []
for material, blend, base, texture, color, strength, disk, backup in prepared:
    shutil.copy2(disk, backup)
    previous = base.get_editor_property('constant')
    rows.append(dict(material=material.get_path_name(), before_color=previous.export_text(),
        before_texture_strength=1., after_color=color, after_texture_strength=strength,
        retained_texture=texture.get_editor_property('texture').get_path_name()))
    material.modify()
    base.modify()
    blend.modify()
    base.set_editor_property('constant', unreal.LinearColor(*color, 1.))
    blend.set_editor_property('const_alpha', strength)
    library.recompile_material(material)

(out / 'preview.json').write_text(json.dumps(dict(status='UNSAVED_PREVIEW', materials=rows,
    geometry_assignments_normal_roughness_changed=False), indent=2), encoding='utf8')
unreal.log('STONE_CONTRAST_PREVIEW_UNSAVED: review before saving')
