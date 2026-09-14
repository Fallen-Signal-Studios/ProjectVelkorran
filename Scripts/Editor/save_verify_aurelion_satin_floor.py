"""Save the two reviewed floor graphs and verify their connected finish inputs."""
import json
from pathlib import Path
import unreal
assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor()
rows=[]
for name in ('M_Radiance_LowerBlack','M_Radiance_LowerBlackWorld'):
    material=unreal.load_asset('/Game/Aurelion/Environment/RadianceMaterials/'+name)
    library=unreal.MaterialEditingLibrary
    add=library.get_material_property_input_node(material,unreal.MaterialProperty.MP_ROUGHNESS)
    multiply=library.get_inputs_for_material_expression(material,add)[0]
    assert abs(add.get_editor_property('const_b')-.48)<.001
    assert abs(multiply.get_editor_property('const_b')-.12)<.001
    assert library.get_inputs_for_material_expression(material,multiply)[0] is not None
    for prop,expected in ((unreal.MaterialProperty.MP_METALLIC,0.),(unreal.MaterialProperty.MP_SPECULAR,.28)):
        assert abs(library.get_material_property_input_node(material,prop).get_editor_property('r')-expected)<.001
    assert unreal.EditorAssetLibrary.save_loaded_asset(material)
    rows.append(dict(material=material.get_path_name(),roughness_range=[.48,.60],metallic=0.,specular=.28))
out=Path(__file__).resolve().parents[2]/'Saved/Validation/Aurelion/FloorFinish-20260913'
(out/'saved.json').write_text(json.dumps(dict(status='PASS',materials=rows,
    scope='Saved graph values and M13 editor preview; fresh-process runtime and other rooms pending'),indent=2),encoding='utf8')
unreal.log('SATIN_FLOOR_SAVED_VERIFIED')
