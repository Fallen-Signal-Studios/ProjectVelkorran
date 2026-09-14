"""Broad terminal conversation fill, previewed before saving the M13 map."""
import unreal
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not level.is_in_play_in_editor()
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_name()=='L_Aurelion_M13'
subsystem=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors=subsystem.get_all_level_actors()
by_label={a.get_actor_label():a for a in actors}
with unreal.ScopedEditorTransaction('Add broad terminal conversation fill'):
    for index,y in enumerate((34100.,35400.,36700.)):
        label='ENVL_M13_TerminalConversation_'+str(index+1)
        light=by_label.get(label)
        if light: assert isinstance(light,unreal.RectLight)
        else: light=subsystem.spawn_actor_from_class(unreal.RectLight,unreal.Vector(600.,y,-650.),unreal.Rotator(pitch=-90.))
        light.modify(); light.set_actor_label(label); light.set_folder_path('Aurelion/00_Lighting')
        component=light.get_component_by_class(unreal.RectLightComponent)
        component.set_mobility(unreal.ComponentMobility.MOVABLE)
        component.set_editor_property('intensity_units',unreal.LightUnits.LUMENS)
        component.set_intensity(3000.)
        component.set_attenuation_radius(2100.)
        component.set_source_width(1400.); component.set_source_height(1000.)
        component.set_light_color(unreal.LinearColor(.85,.93,1.,1.))
unreal.log('M13_TERMINAL_FILL_UNSAVED')
