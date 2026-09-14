"""Read-only light inventory for the remaining crucible lighting pass."""
import unreal

def audit_z08_lighting(actors):
    rows=[]
    for actor in actors:
        for c in actor.get_components_by_class(unreal.LightComponent):
            p=c.get_world_location()
            if not (-5500<=p.x<=5500 and 17000<=p.y<=24500 and -1600<=p.z<=1600):continue
            color=c.get_light_color();d=c.get_forward_vector()
            values={}
            for key in ('intensity','intensity_units','attenuation_radius','cast_shadows','source_width','source_height','inner_cone_angle','outer_cone_angle','indirect_lighting_intensity','volumetric_scattering_intensity'):
                try:values[key]=str(c.get_editor_property(key))
                except Exception:pass
            rows.append(dict(actor=actor.get_actor_label(),component=c.get_name(),class_name=c.get_class().get_name(),transform=c.get_world_transform().export_text(),forward=[d.x,d.y,d.z],linear_color=[color.r,color.g,color.b,color.a],properties=values))
    return dict(scope='Light settings in and near Z08; spatial inclusion alone does not establish contribution or causality for visible dark areas.',lights=rows)
