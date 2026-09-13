"""Preview relay room stone and broad ceiling-mounted fill; leave map unsaved for visual review."""
import json
from pathlib import Path
import unreal

level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not level.is_in_play_in_editor()
assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_name() == 'L_Aurelion_M12'
subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors = subsystem.get_all_level_actors()
by_label = {a.get_actor_label(): a for a in actors}
ceiling = by_label['Aurelion_Art_M12_Z04_86_7c5bc5']
material = unreal.load_asset('/Game/Aurelion/Environment/RadianceMaterials/M_Radiance_IvoryStone')
assert material
components = ceiling.get_components_by_class(unreal.StaticMeshComponent)
assert len(components) == 1
component = components[0]
assert component.get_editor_property('static_mesh').get_name() == 'SM_Scifi_Floor_04'
assert component.get_num_materials() == 1
assert component.get_material(0).get_name() in ('M_Radiance_ceiling', 'M_Radiance_IvoryStone')

def physical_state():
    return {a.get_actor_label(): (a.get_actor_transform().export_text(),
        [(c.get_name(), c.get_world_transform().export_text(), str(c.get_collision_enabled()))
         for c in a.get_components_by_class(unreal.PrimitiveComponent)])
        for a in actors}

before = physical_state()
lights = []
with unreal.ScopedEditorTransaction('Improve relay room ceiling readability'):
    component.modify()
    component.set_material(0, material)
    for index, (x, y) in enumerate(((5000,-11900),(9000,-11900),(5000,-10100),(9000,-10100))):
        label = 'ENVL_Z04_CeilingBounce_' + str(index+1)
        light = by_label.get(label)
        if light:
            assert isinstance(light, unreal.RectLight)
        else:
            light = subsystem.spawn_actor_from_class(unreal.RectLight, unreal.Vector(x,y,600), unreal.Rotator(pitch=-90))
        light.set_actor_location(unreal.Vector(x,y,600), False, False)
        light.set_actor_rotation(unreal.Rotator(pitch=-90), False)
        light.set_actor_label(label)
        light.set_folder_path('Aurelion/00_Lighting')
        c = light.get_component_by_class(unreal.RectLightComponent)
        c.set_mobility(unreal.ComponentMobility.MOVABLE)
        c.set_editor_property('intensity_units', unreal.LightUnits.LUMENS)
        c.set_intensity(1200)
        c.set_attenuation_radius(2300)
        c.set_source_width(1200)
        c.set_source_height(900)
        c.set_light_color(unreal.LinearColor(1,.94,.83,1))
        lights.append(label)
after = physical_state()
assert all(before[k] == after[k] for k in before if k not in lights), 'Existing scene transforms or collision changed'
report = dict(status='PREVIEW_UNSAVED', ceiling=ceiling.get_actor_label(),
    material=material.get_path_name(), lights=lights, lumens_each=1200,
    unchanged_existing_actor_component_transforms_and_collision=sum(k not in lights for k in before),
    limitations='Requires visual review, saved-map verification, gameplay and packaged GPU performance checks')
out = Path(__file__).resolve().parents[2] / 'Saved/Validation/Aurelion/RoomReview-20260913/relay-lighting-preview.json'
out.write_text(json.dumps(report,indent=2),encoding='utf8')
unreal.log('RELAY_LIGHTING_PREVIEW_UNSAVED ' + str(out))
