"""Preview new art on recess back walls. Existing physical walls remain authoritative."""
import unreal

editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not editor.is_in_play_in_editor()
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_name() == 'L_Aurelion_M12'
subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors = subsystem.get_all_level_actors()
by_label = {a.get_actor_label(): a for a in actors}
for label, x in (('Z08_WestSurvivorRecess_North', -2600), ('Z08_EastSurvivorRecess_North', 3050)):
    wall = by_label[label]
    p = wall.get_actor_location()
    assert abs(p.x-x) < .1 and abs(p.y-22900) < .1 and abs(p.z+1050) < .1
mesh = unreal.load_asset('/Game/Aurelion/Environment/Blender/SM_Aurelion_RecessPanel_2m')
assert mesh
placements = [('West', x, 1.) for x in (-3100,-2900,-2700,-2500,-2300,-2100)]
placements += [('East', x, scale) for x,scale in ((2800,1.),(3000,1.),(3200,1.),(3350,.5))]
with unreal.ScopedEditorTransaction('Dress Aurelion survivor recess back walls'):
    for index, (side,x,scale) in enumerate(placements):
        label = 'ART_RecessPanel_' + side + '_' + str(index+1).zfill(2)
        actor = by_label.get(label)
        if actor:
            assert isinstance(actor, unreal.StaticMeshActor) and actor.static_mesh_component.static_mesh == mesh
        else:
            actor = subsystem.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(x,22870,-1200), unreal.Rotator(yaw=180))
        actor.set_actor_label(label)
        actor.set_actor_location(unreal.Vector(x,22870,-1200), False, False)
        actor.set_actor_rotation(unreal.Rotator(yaw=180), False)
        actor.set_folder_path('Aurelion/EnvironmentArt/Z08/RecessPanels')
        actor.set_actor_scale3d(unreal.Vector(scale,1,1))
        c = actor.get_component_by_class(unreal.StaticMeshComponent)
        c.set_static_mesh(mesh)
        c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        actor.set_actor_enable_collision(False)
    for side, x in (('West', -2600), ('East', 3050)):
        label = 'ENVL_Z08_RecessFill_' + side
        light = by_label.get(label)
        if light:
            assert isinstance(light, unreal.RectLight)
        else:
            light = subsystem.spawn_actor_from_class(unreal.RectLight, unreal.Vector(x,22150,-840), unreal.Rotator(pitch=-12,yaw=90))
        light.set_actor_label(label)
        light.set_actor_location(unreal.Vector(x,22150,-840), False, False)
        light.set_actor_rotation(unreal.Rotator(pitch=-12,yaw=90), False)
        light.set_folder_path('Aurelion/00_Lighting')
        c = light.get_component_by_class(unreal.RectLightComponent)
        c.set_mobility(unreal.ComponentMobility.MOVABLE)
        c.set_editor_property('intensity_units', unreal.LightUnits.LUMENS)
        c.set_intensity(350)
        c.set_attenuation_radius(1400)
        c.set_source_width(600)
        c.set_source_height(50)
        c.set_light_color(unreal.LinearColor(.94,.9,.82,1))
unreal.log('RECESS_PANELS_PREVIEW_UNSAVED 10; physical walls, entrances and cast marks unchanged')
