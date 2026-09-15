"""Local exterior fill on the retained nonfunctional scenery owner."""
import unreal
OWNER='Aurelion_Radiance_AtriumEastRelatedForm'

def fit_carrier_fill(actors,fit):
    a=next(a for a in actors if a.get_actor_label()==OWNER)
    assert not a.get_editor_property('hidden')
    sub=unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    lib=unreal.SubobjectDataBlueprintFunctionLibrary
    existing={c.get_name():c for c in a.get_components_by_class(unreal.LightComponent)}
    assert not existing or set(existing)=={row['name'] for row in fit['lights']}
    a.modify();handles=sub.k2_gather_subobject_data_for_instance(a)
    parent=next(h for h in handles if lib.get_associated_object(lib.get_data(h))==a)
    for row in fit['lights']:
        c=existing.get(row['name'])
        if not c:
            h,reason=sub.add_new_subobject(unreal.AddNewSubobjectParams(parent_handle=parent,new_class=unreal.RectLightComponent,conform_transform_to_parent=True))
            assert lib.is_handle_valid(h),reason
            sub.rename_subobject(h,row['name']);c=lib.get_associated_object(lib.get_data(h))
        c.modify();c.set_mobility(unreal.ComponentMobility.MOVABLE)
        p=unreal.Vector(*row['location']);target=unreal.Vector(*row['target'])
        c.set_world_transform(unreal.Transform(location=p,rotation=unreal.MathLibrary.find_look_at_rotation(p,target),scale=unreal.Vector(1,1,1)),False,False)
        c.set_editor_property('intensity_units',unreal.LightUnits.LUMENS);c.set_intensity(row['lumens'])
        c.set_source_width(row['width']);c.set_source_height(row['height']);c.set_attenuation_radius(row['radius'])
        c.set_cast_shadows(True);c.set_volumetric_scattering_intensity(0)
        c.set_light_color(unreal.LinearColor(*fit['color'],1),False);c.set_visibility(True);c.set_hidden_in_game(False)

def check_carrier_fill(actors,fit):
    a=next(a for a in actors if a.get_actor_label()==OWNER)
    assert not a.get_editor_property('hidden')
    cs={c.get_name():c for c in a.get_components_by_class(unreal.LightComponent)}
    assert set(cs)=={row['name'] for row in fit['lights']}
    for row in fit['lights']:
        c=cs[row['name']];t=c.get_world_transform()
        assert (t.translation-unreal.Vector(*row['location'])).length()<.01
        expected=unreal.MathLibrary.find_look_at_rotation(unreal.Vector(*row['location']),unreal.Vector(*row['target'])).quaternion()
        assert t.rotation.angular_distance(expected)<.001
        assert c.get_editor_property('visible') and not c.get_editor_property('hidden_in_game')
        assert c.get_editor_property('intensity_units')==unreal.LightUnits.LUMENS and c.get_editor_property('intensity')==row['lumens']
        assert c.get_editor_property('attenuation_radius')==row['radius'] and c.get_editor_property('source_width')==row['width'] and c.get_editor_property('source_height')==row['height']
        assert c.get_editor_property('cast_shadows') and c.get_editor_property('volumetric_scattering_intensity')==0
    return dict(lights=fit['lights'],color=fit['color'],qualification='Two local shadow-casting exterior fills; global sun/exposure unchanged.')
