"""Previewable, idempotent architectural uplights for the M13 chamber.

Only apply() mutates the current editor world; saving is an explicit separate step.
"""
import math
import unreal

PREFIX = 'ENVL_M13_ChamberUplight_'

def apply():
    level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    assert not level.is_in_play_in_editor() and world.get_name() == 'L_Aurelion_M13'
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    existing = {a.get_actor_label(): a for a in actors.get_all_level_actors()}
    result = []
    with unreal.ScopedEditorTransaction('Light the M13 chamber architectural ribs'):
        for angle in (30, 55, 125, 150, 210, 235, 305, 330):
            label = PREFIX + str(angle)
            radians = math.radians(angle)
            position = unreal.Vector(4900 * math.cos(radians), 35000 + 4900 * math.sin(radians), -1600)
            rotation = unreal.Rotator(pitch=65, yaw=angle)
            light = existing.get(label)
            if light:
                assert isinstance(light, unreal.SpotLight)
            else:
                light = actors.spawn_actor_from_class(unreal.SpotLight, position, rotation)
            light.modify()
            light.set_actor_label(label)
            light.set_folder_path('Aurelion/00_Lighting')
            light.set_actor_location(position, False, False)
            light.set_actor_rotation(rotation, False)
            component = light.get_component_by_class(unreal.SpotLightComponent)
            component.set_mobility(unreal.ComponentMobility.MOVABLE)
            component.set_editor_property('intensity_units', unreal.LightUnits.LUMENS)
            component.set_intensity(2000)
            component.set_attenuation_radius(4500)
            component.set_inner_cone_angle(0)
            component.set_outer_cone_angle(60)
            component.set_light_color(unreal.LinearColor(1, .82, .59, 1))
            component.set_editor_property('cast_shadows', True)
            result.append(label)
    return result
