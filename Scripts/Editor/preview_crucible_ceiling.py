"""Unsaved material preview for one crucible ceiling actor; no geometry edits."""
import unreal

actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_name() == 'L_Aurelion_M12'
matches = [a for a in actors if a.get_actor_label() == 'Aurelion_Art_M12_Z08_89_b5e7cb']
assert len(matches) == 1
material = unreal.load_asset('/Game/Aurelion/Environment/RadianceMaterials/M_Radiance_IvoryStone')
assert material
with unreal.ScopedEditorTransaction('Preview pale crucible ceiling'):
    for component in matches[0].get_components_by_class(unreal.StaticMeshComponent):
        assert component.get_num_materials() == 1
        component.modify()
        component.set_material(0, material)
    subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for side in (-1, 1):
        label = 'ENVL_Z08_CeilingBounce_' + str(side)
        existing = [a for a in actors if a.get_actor_label() == label]
        assert len(existing) <= 1, label
        light = existing[0] if existing else subsystem.spawn_actor_from_class(
            unreal.RectLight, unreal.Vector(side*2000, 20800, -850),
            unreal.Rotator(pitch=90, yaw=0, roll=0))
        assert isinstance(light, unreal.RectLight)
        light.set_actor_label(label)
        light.set_folder_path('Aurelion/00_Lighting')
        c = light.get_component_by_class(unreal.RectLightComponent)
        c.set_mobility(unreal.ComponentMobility.MOVABLE)
        c.set_editor_property('intensity_units', unreal.LightUnits.LUMENS)
        c.set_intensity(6000)
        c.set_attenuation_radius(3000)
        c.set_source_width(600)
        c.set_source_height(300)
        c.set_light_color(unreal.LinearColor(1, .88, .7, 1))
unreal.log('CRUCIBLE_CEILING_PREVIEW_UNSAVED')
