"""Preview satin black stone using the existing roughness texture network."""
import json
from pathlib import Path
import unreal
assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor()
library=unreal.MaterialEditingLibrary
rows=[]
for name in ('M_Radiance_LowerBlack','M_Radiance_LowerBlackWorld'):
    material=unreal.load_asset('/Game/Aurelion/Environment/RadianceMaterials/'+name)
    add=library.get_material_property_input_node(material,unreal.MaterialProperty.MP_ROUGHNESS)
    assert isinstance(add,unreal.MaterialExpressionAdd)
    inputs=library.get_inputs_for_material_expression(material,add)
    multiply=inputs[0]
    assert inputs[1] is None and isinstance(multiply,unreal.MaterialExpressionMultiply)
    assert library.get_inputs_for_material_expression(material,multiply)[1] is None
    assert any(abs(add.get_editor_property('const_b')-v)<.001 for v in (.18,.48))
    assert any(abs(multiply.get_editor_property('const_b')-v)<.001 for v in (.08,.12))
    material.modify(); add.modify(); multiply.modify()
    add.set_editor_property('const_b',.48)
    multiply.set_editor_property('const_b',.12)
    library.recompile_material(material)
    rows.append(dict(material=material.get_path_name(),base_roughness=.48,texture_variation=.12))
out=Path(__file__).resolve().parents[2]/'Saved/Validation/Aurelion/FloorFinish-20260913'
(out/'preview.json').write_text(json.dumps(dict(status='UNSAVED_PREVIEW',materials=rows,
    geometry_and_assignments_changed=False),indent=2),encoding='utf8')
unreal.log('SATIN_FLOOR_PREVIEW_UNSAVED')

