"""Fresh basalt normal input, preserved finish and preceding architecture checks."""
from pathlib import Path
import runpy,time
import unreal
root=Path(unreal.Paths.project_dir());DEFER_Z08_COURT_AUTORUN=True
exec(compile((root/'Scripts/Editor/verify_z08_court.py').read_text(),'verify_z08_court','exec'),globals())
verify_z08_court_stage=verify
del DEFER_Z08_COURT_AUTORUN
def verify():
    verify_z08_court_stage()
    lib=unreal.MaterialEditingLibrary;material=unreal.load_asset('/Game/Aurelion/Environment/ArchitectureKit/Materials/M_AurelionKit_PavingBasalt')
    normal=lib.get_material_property_input_node(material,unreal.MaterialProperty.MP_NORMAL)
    assert isinstance(normal,unreal.MaterialExpressionLinearInterpolate) and abs(normal.get_editor_property('const_alpha')-.12)<.001
    node=lib.get_inputs_for_material_expression(material,normal)[1];texture=node.get_editor_property('texture')
    assert texture.get_name()=='T_AurelionKit_StoneFine_N' and texture.get_editor_property('compression_settings')==unreal.TextureCompressionSettings.TC_NORMALMAP and not texture.get_editor_property('srgb') and texture.get_editor_property('flip_green_channel')
    base=lib.get_material_property_input_node(material,unreal.MaterialProperty.MP_BASE_COLOR)
    assert isinstance(base,unreal.MaterialExpressionLinearInterpolate) and abs(base.get_editor_property('const_alpha')-.06)<.001
    color=lib.get_inputs_for_material_expression(material,base)[0].get_editor_property('constant')
    assert max(abs(a-b) for a,b in zip((color.r,color.g,color.b),(.025,.028,.033)))<.001
    rough=lib.get_material_property_input_node(material,unreal.MaterialProperty.MP_ROUGHNESS)
    assert isinstance(rough,unreal.MaterialExpressionAdd) and abs(rough.get_editor_property('const_b')-.63)<.001
    assert abs(lib.get_inputs_for_material_expression(material,rough)[0].get_editor_property('const_b')-.1)<.001
    runpy.run_path(str(root/'Scripts/Editor/export_court_material_graphs.py'))
    exported=json.loads((out/'material-exports.json').read_text())
    assert all(not row['serialized_depth_displacement_or_mask_bindings'] for row in exported['exports'])
    assert len(actors)==3140 and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    (out/'basalt-normal-verification.json').write_text(json.dumps(dict(status='passed',actor_count=len(actors),normal_texture=texture.get_path_name(),normal_strength=.12,qualification='Saved material input and finish constants; remaining visual artifacts, live play and performance are not qualified.'),indent=2))
if not globals().get('DEFER_BASALT_NORMAL_AUTORUN',False):
    unreal.EditorPythonScripting.set_keep_python_script_alive(True);started=time.monotonic()
    def tick(delta):
        if time.monotonic()-started<15:return
        unreal.unregister_slate_post_tick_callback(handle)
        try:verify()
        finally:unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    handle=unreal.register_slate_post_tick_callback(tick)
