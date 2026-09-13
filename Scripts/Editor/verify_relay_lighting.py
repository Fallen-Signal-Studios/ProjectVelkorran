"""Validate relay fill parameters without claiming gameplay or performance acceptance."""
import json
from pathlib import Path
import unreal

assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor()
assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_name() == 'L_Aurelion_M12'
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
by_label = {a.get_actor_label(): a for a in actors}
ceiling = by_label['Aurelion_Art_M12_Z04_86_7c5bc5']
components = ceiling.get_components_by_class(unreal.StaticMeshComponent)
assert len(components) == 1
assert components[0].get_material(0).get_path_name() == '/Game/Aurelion/Environment/RadianceMaterials/M_Radiance_IvoryStone.M_Radiance_IvoryStone'
rows = []
for i, (x, y) in enumerate(((5000,-11900),(9000,-11900),(5000,-10100),(9000,-10100))):
    light = by_label['ENVL_Z04_CeilingBounce_' + str(i+1)]
    p = light.get_actor_location()
    assert abs(p.x-x) < .1 and abs(p.y-y) < .1 and abs(p.z-600) < .1
    assert abs(light.get_actor_rotation().pitch+90) < .1
    c = light.get_component_by_class(unreal.RectLightComponent)
    assert c.get_editor_property('intensity_units') == unreal.LightUnits.LUMENS
    for property_name, expected in (('intensity',1200),('attenuation_radius',2300),('source_width',1200),('source_height',900)):
        assert abs(c.get_editor_property(property_name)-expected) < .1, property_name
    rows.append(dict(label=light.get_actor_label(),position=p.export_text(),lumens=1200))
root = Path(__file__).resolve().parents[2]
report = dict(status='PASS',lights=rows,ceiling_material='M_Radiance_IvoryStone',
    scope='Authored parameter verification only; runtime, both priorities and GPU performance remain unqualified')
(root/'Saved/Validation/Aurelion/RoomReview-20260913/relay-lighting-verified.json').write_text(json.dumps(report,indent=2),encoding='utf8')
unreal.log('RELAY_LIGHTING_PARAMETERS_VERIFIED')
