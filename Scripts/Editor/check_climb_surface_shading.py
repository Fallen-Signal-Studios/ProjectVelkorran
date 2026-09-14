"""Selected climb-light properties and climb-stone material."""
from pathlib import Path
import json,unreal

def check_climb_surface_shading(actors):
    root=Path(unreal.Paths.project_dir());fit=json.loads((root/'Art/Source/Aurelion/Z08WallKit/climb-light-fit.json').read_text());row=fit['light']
    a=next(a for a in actors if a.get_actor_label()==row['actor']);c=a.get_component_by_class(unreal.LightComponent)
    assert c.get_world_transform().export_text()==row['transform']
    assert {k:str(c.get_editor_property(k)) for k in row['properties']}==row['properties']
    color=c.get_light_color();assert max(abs(v-w) for v,w in zip((color.r,color.g,color.b,color.a),fit['selected_linear_color']))<.0001
    mesh=unreal.load_asset('/Game/Aurelion/Environment/ArchitectureKit/Meshes/SM_Aurelion_KIT_Z06ClimbPanel')
    sm=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    assert not sm.get_nanite_settings(mesh).get_editor_property('explicit_tangents')
    material=unreal.load_asset('/Game/Aurelion/Environment/ArchitectureKit/Materials/M_AurelionKit_ClimbIvory');assert material
    expected={'M_Aurelion_IvoryStone':'ClimbIvory','M_Aurelion_StoneGrout':'StoneGrout','M_Aurelion_AncientGold':'Gold','M_Aurelion_ChannelShadow':'Reveal'}
    slots=mesh.get_editor_property('static_materials');assert len(slots)==len(expected)
    for i,slot in enumerate(slots):
        key=str(slot.get_editor_property('imported_material_slot_name'))
        assert mesh.get_material(i).get_name()=='M_AurelionKit_'+expected[key]
    surfaces=[c for a in actors for c in a.get_components_by_class(unreal.StaticMeshComponent) if c.static_mesh==mesh]
    assert len(surfaces)==1 and not surfaces[0].get_editor_property('override_materials')
    normal=unreal.MaterialEditingLibrary.get_material_property_input_node(material,unreal.MaterialProperty.MP_NORMAL)
    assert abs(normal.get_editor_property('const_alpha')-.03)<.0001
    stone=unreal.load_asset('/Game/Aurelion/Environment/ArchitectureKit/Materials/M_AurelionKit_PavingIvory')
    normal=unreal.MaterialEditingLibrary.get_material_property_input_node(stone,unreal.MaterialProperty.MP_NORMAL)
    assert abs(normal.get_editor_property('const_alpha')-.12)<.0001
    assert unreal.SystemLibrary.get_console_variable_int_value('r.ShadowQuality')==5
    return dict(neutral_key=row['actor'],climb_material=material.get_path_name(),climb_normal_strength=.03,stone_normal_strength=.12,qualification='Saved shader/light settings; live lighting, remaining meshes and performance unqualified.')
