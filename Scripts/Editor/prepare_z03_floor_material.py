"""Owned floor finish; preserve shared architecture materials."""
import unreal

def prepare_z03_floor_material():
    lib=unreal.MaterialEditingLibrary
    base='/Game/Aurelion/Environment/ArchitectureKit/Materials/'
    path=base+'M_AurelionKit_Z03FloorStone'
    material=unreal.load_asset(path)
    if material is None:
        material=unreal.EditorAssetLibrary.duplicate_asset(base+'M_AurelionKit_PavingIvory',path)
    assert material
    color=lib.get_material_property_input_node(material,unreal.MaterialProperty.MP_BASE_COLOR)
    normal=lib.get_material_property_input_node(material,unreal.MaterialProperty.MP_NORMAL)
    assert isinstance(color,unreal.MaterialExpressionLinearInterpolate)
    assert isinstance(normal,unreal.MaterialExpressionLinearInterpolate)
    color.set_editor_property('const_alpha',.65)
    normal.set_editor_property('const_alpha',.22)
    lib.recompile_material(material)
    assert unreal.EditorAssetLibrary.save_loaded_asset(material,False)
    return material
