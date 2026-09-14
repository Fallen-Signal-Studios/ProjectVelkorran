"""Fresh material-input validation and full saved architecture regression."""
from pathlib import Path
import time
import unreal
root=Path(unreal.Paths.project_dir());DEFER_Z06_RESCUE_GATE_AUTORUN=True
exec(compile((root/'Scripts/Editor/verify_z06_rescue_gate.py').read_text(),'verify_z06_rescue_gate','exec'),globals())
verify_rescue_gate_stage=verify
del DEFER_Z06_RESCUE_GATE_AUTORUN
def verify():
    verify_rescue_gate_stage()
    lib=unreal.MaterialEditingLibrary;texture=unreal.load_asset('/Game/Aurelion/Environment/ArchitectureKit/Textures/T_AurelionKit_StoneFine_N');assert texture
    assert texture.get_editor_property('compression_settings')==unreal.TextureCompressionSettings.TC_NORMALMAP and not texture.get_editor_property('srgb') and texture.get_editor_property('flip_green_channel')
    assert Path(texture.get_editor_property('asset_import_data').get_first_filename()).resolve()==(root/'Art/Source/Aurelion/StoneNormalKit/T_AurelionKit_StoneFine_N.png').resolve()
    rows=[]
    for name,strength,color,blend,rough,variation in [('Ivory',.20,(.62,.59,.52),.22,.34,.20),('PavingIvory',.12,(.42,.40,.35),.12,.60,.10)]:
        material=unreal.load_asset('/Game/Aurelion/Environment/ArchitectureKit/Materials/M_AurelionKit_'+name);normal=lib.get_material_property_input_node(material,unreal.MaterialProperty.MP_NORMAL)
        assert abs(normal.get_editor_property('const_alpha')-strength)<.001
        node=lib.get_inputs_for_material_expression(material,normal)[1];assert node.get_editor_property('texture')==texture and node.get_editor_property('sampler_type')==unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL
        base=lib.get_material_property_input_node(material,unreal.MaterialProperty.MP_BASE_COLOR);assert abs(base.get_editor_property('const_alpha')-blend)<.001
        inputs=lib.get_inputs_for_material_expression(material,base);c=inputs[0].get_editor_property('constant');assert max(abs(a-b) for a,b in zip((c.r,c.g,c.b),color))<.001
        assert lib.get_inputs_for_material_expression(material,inputs[1])[0].get_editor_property('texture').get_name()=='T_Aurelion_IvoryStone_BaseColor'
        r=lib.get_material_property_input_node(material,unreal.MaterialProperty.MP_ROUGHNESS);assert abs(r.get_editor_property('const_b')-rough)<.001
        v=lib.get_inputs_for_material_expression(material,r)[0];assert abs(v.get_editor_property('const_b')-variation)<.001
        assert lib.get_inputs_for_material_expression(material,v)[0].get_editor_property('texture').get_name()=='T_KB3D_UTP_ConcreteMix_roughness_jpg'
        rows.append(dict(material=name,normal_strength=strength,base_color_and_roughness_retained=True))
    assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    (out/'kit-stone-normal-verification.json').write_text(json.dumps(dict(status='passed',actor_count=len(actors),materials=rows,qualification='Saved material inputs and prior architecture; shadow artifacts and final visual/performance acceptance remain open.'),indent=2))
if not globals().get("DEFER_KIT_STONE_NORMAL_AUTORUN",False):
    unreal.EditorPythonScripting.set_keep_python_script_alive(True);started=time.monotonic()
    def tick(delta):
        if time.monotonic()-started<15:return
        unreal.unregister_slate_post_tick_callback(handle)
        try:verify()
        finally:unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    handle=unreal.register_slate_post_tick_callback(tick)
