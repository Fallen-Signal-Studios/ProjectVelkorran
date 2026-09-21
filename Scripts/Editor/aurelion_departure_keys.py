"""Local departure keys to reveal protagonist armor without lifting the whole hall."""
import unreal
PREFIX='ENVL_M13_DepartureKey_'
SPECS=(('Tarrik',(-1350.,48450.,260.)),('Selene',(1350.,48450.,260.)))
INTENSITIES={'Tarrik':600.,'Selene':240.}
def configure(actor,name,position,intensity=None):
    actor.set_actor_label(PREFIX+name)
    c=actor.get_component_by_class(unreal.RectLightComponent)
    c.set_mobility(unreal.ComponentMobility.MOVABLE)
    actor.set_actor_location(unreal.Vector(*position),False,False)
    actor.set_actor_rotation(unreal.Rotator(pitch=-12.,yaw=-90.),False)
    assert (actor.get_actor_location()-unreal.Vector(*position)).length()<.01
    c.set_editor_property('intensity_units',unreal.LightUnits.LUMENS)
    c.set_intensity(INTENSITIES[name] if intensity is None else intensity);c.set_attenuation_radius(950.)
    c.set_source_width(220.);c.set_source_height(180.)
    c.set_light_color(unreal.LinearColor(1.,.97,.92,1.))
    c.set_editor_property('specular_scale',.65)
    c.set_editor_property('cast_shadows',True)
    return actor
def describe(actor):
    c=actor.get_component_by_class(unreal.RectLightComponent)
    return dict(label=actor.get_actor_label(),transform=actor.get_actor_transform().export_text(),
        intensity=c.intensity,radius=c.attenuation_radius,width=c.source_width,height=c.source_height,
        color=c.get_light_color().export_text(),specular=c.get_editor_property('specular_scale'),shadows=c.get_editor_property('cast_shadows'))
def summon(world,cls):
    before={a.get_path_name() for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.Actor)}
    unreal.SystemLibrary.execute_console_command(world,'EnableCheats')
    unreal.SystemLibrary.execute_console_command(world,'Summon /Script/Engine.'+cls)
    added=[a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.Actor) if a.get_path_name() not in before]
    assert len(added)==1,cls
    return added[0]
def spawn_preview(world):
    assert 'UEDPIE_' in world.get_path_name() and 'L_Aurelion_M13' in world.get_name()
    assert not any(a.get_actor_label().startswith(PREFIX) for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.RectLight))
    return [configure(summon(world,'RectLight'),name,position,0.) for name,position in SPECS]
