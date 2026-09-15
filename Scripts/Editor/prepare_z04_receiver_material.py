"""Read native disabled flag from custom primitive data slot zero."""
import unreal
def prepare_receiver_material():
    path='/Game/Aurelion/Environment/ArchitectureKit/Materials/M_AurelionKit_ReceiverStatus'
    if unreal.EditorAssetLibrary.does_asset_exist(path):return unreal.load_asset(path)
    m=unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_AurelionKit_ReceiverStatus',path.rsplit('/',1)[0],unreal.Material,unreal.MaterialFactoryNew())
    lib=unreal.MaterialEditingLibrary
    def color(value,x,y):
        n=lib.create_material_expression(m,unreal.MaterialExpressionConstant3Vector,x,y);n.set_editor_property('constant',unreal.LinearColor(*value,1));return n
    active=color((.08,2,2.6),-600,-100);inactive=color((.003,.008,.01),-600,50)
    flag=lib.create_material_expression(m,unreal.MaterialExpressionScalarParameter,-600,200)
    flag.set_editor_property('parameter_name','ReceiverDisabled');flag.set_editor_property('default_value',0);flag.set_editor_property('use_custom_primitive_data',True);flag.set_editor_property('primitive_data_index',0)
    blend=lib.create_material_expression(m,unreal.MaterialExpressionLinearInterpolate,-250,0)
    for node,pin in ((active,'A'),(inactive,'B'),(flag,'Alpha')):assert lib.connect_material_expressions(node,'',blend,pin)
    assert lib.connect_material_property(blend,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    base=color((.01,.03,.035),-250,250);assert lib.connect_material_property(base,'',unreal.MaterialProperty.MP_BASE_COLOR)
    rough=lib.create_material_expression(m,unreal.MaterialExpressionConstant,-250,400);rough.set_editor_property('r',.3);assert lib.connect_material_property(rough,'',unreal.MaterialProperty.MP_ROUGHNESS)
    lib.recompile_material(m);assert unreal.EditorAssetLibrary.save_loaded_asset(m)
    return m
