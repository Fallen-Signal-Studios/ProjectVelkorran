"""Two broad neutral concourse fills; shared specification for PIE preview and authoring."""
import unreal

PREFIX='ENVL_M13_DepartureReadability_'
SPECS=(('North',(0.,48600.,400.),-90.),('South',(0.,46900.,400.),90.))

def configure(actor,name,position,yaw):
    actor.set_actor_label(PREFIX+name)
    c=actor.get_component_by_class(unreal.RectLightComponent)
    assert c
    c.set_mobility(unreal.ComponentMobility.MOVABLE)
    actor.set_actor_location(unreal.Vector(*position),False,False)
    actor.set_actor_rotation(unreal.Rotator(pitch=-15.,yaw=yaw),False)
    assert (actor.get_actor_location()-unreal.Vector(*position)).length()<.01,'Light placement was rejected'
    rotation=actor.get_actor_rotation()
    assert abs(rotation.pitch+15)<.01 and abs(rotation.yaw-yaw)<.01,'Light orientation was rejected'
    c.set_editor_property('intensity_units',unreal.LightUnits.LUMENS)
    c.set_intensity(1600.)
    c.set_attenuation_radius(3000.)
    c.set_source_width(1400.);c.set_source_height(200.)
    c.set_light_color(unreal.LinearColor(1.,.95,.88,1.))
    c.set_editor_property('specular_scale',.3)
    c.set_editor_property('cast_shadows',True)
    return actor

def describe(actor):
    c=actor.get_component_by_class(unreal.RectLightComponent)
    return dict(label=actor.get_actor_label(),transform=actor.get_actor_transform().export_text(),
        intensity=c.intensity,radius=c.attenuation_radius,width=c.source_width,height=c.source_height,
        color=c.get_light_color().export_text(),specular=c.get_editor_property('specular_scale'),
        shadows=c.get_editor_property('cast_shadows'))

def spawn_preview(world):
    assert 'UEDPIE_' in world.get_path_name() and 'L_Aurelion_M13' in world.get_name()
    existing=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.RectLight)
    assert not any(a.get_actor_label().startswith(PREFIX) for a in existing),'Preview requires the unchanged production map'
    result=[]
    unreal.SystemLibrary.execute_console_command(world,'EnableCheats')
    for name,position,yaw in SPECS:
        before={a.get_path_name() for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.Actor)}
        unreal.SystemLibrary.execute_console_command(world,'Summon /Script/Engine.RectLight')
        added=[a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.Actor) if a.get_path_name() not in before]
        assert len(added)==1 and isinstance(added[0],unreal.RectLight),'Preview did not spawn exactly one RectLight'
        actor=added[0]
        result.append(configure(actor,name,position,yaw))
    return result
